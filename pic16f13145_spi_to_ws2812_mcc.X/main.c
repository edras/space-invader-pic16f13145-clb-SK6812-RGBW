/*
 * Space Invaders 1D - PIC16F13145 + WS2812 LED Strip
 *
 * LED strip: 300 LEDs, index 0 = player end, index 299 = spawn end.
 * Invaders march from index 299 toward index 0. The group starts at
 * LEDs 280-299 with 20 random colors, grows by one each game tick, and
 * accelerates over time. Shots (R/G/B) travel from index 0 toward 299.
 * A shot destroys the first invader it hits only if the colors match;
 * otherwise the shot is absorbed with no effect on the invader.
 * Game over when any invader reaches index 0. PB1 restarts.
 *
 * RAM strategy: no frame buffer. GRB bytes are computed and sent to SPI
 * one at a time during WriteLEDs(), keeping RAM use to ~300 bytes for the
 * strip[] array plus small globals (well within the 2048-byte limit).
 *
 * Buttons:
 *   PB1 (RC4) - Restart / start game
 *   PB2 (RC5) - Fire red shot
 *   PB3 (RA4) - Fire green shot
 *   PB4 (RC3) - Fire blue shot
 *
 * On-board LEDs:
 *   LED3/LED4/LED5 blink together during game over.
 */

#include "mcc_generated_files/system/system.h"

/* -------------------------------------------------------------------------
 * Hardware / strip constants
 * ---------------------------------------------------------------------- */
#define NUM_LEDS            300u

/* Brightness cap ~35%: 255 * 0.35 = 89 */
#define BRIGHT(x)           ((uint8_t)(((uint16_t)(x) * 89u) / 255u))

#define COL_RED_G   0x00
#define COL_RED_R   BRIGHT(0xFF)
#define COL_RED_B   0x00

#define COL_GRN_G   BRIGHT(0xFF)
#define COL_GRN_R   0x00
#define COL_GRN_B   0x00

#define COL_BLU_G   0x00
#define COL_BLU_R   0x00
#define COL_BLU_B   BRIGHT(0xFF)

/* Dim white used for idle blink at LED 0 */
#define COL_IDLE    BRIGHT(0x20)

/* -------------------------------------------------------------------------
 * Invader / shot color identifiers
 * ---------------------------------------------------------------------- */
typedef enum {
    COLOR_NONE = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
} color_id_t;

/* -------------------------------------------------------------------------
 * Game constants
 * ---------------------------------------------------------------------- */
#define INVADER_START_POS   280u    /* first invader index at game start    */
#define GAME_TICK_INIT_MS   500u    /* initial ms between invader advances  */
#define GAME_TICK_MIN_MS    100u    /* fastest tick period                  */
#define SPEED_UP_EVERY      10u     /* ticks between speed increases        */
#define SPEED_UP_STEP_MS    30u     /* ms shaved off per speed-up           */
#define SHOT_STEP_MS        50u     /* ms between shot position advances    */

/* -------------------------------------------------------------------------
 * Game state
 * ---------------------------------------------------------------------- */
typedef enum {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_GAME_OVER
} game_state_t;

/* Per-LED invader color (COLOR_NONE = empty).
 * 300 bytes — the only large RAM allocation in this file. */
static color_id_t strip[NUM_LEDS];

/* Shot state: one slot per color (0=RED, 1=GREEN, 2=BLUE) */
#define NUM_SHOTS   3u
typedef struct {
    int16_t    pos;     /* current LED index; -1 = inactive */
    color_id_t color;
} shot_t;

static shot_t shots[NUM_SHOTS];

/* Timing / counters */
static volatile uint16_t ms_tick;
static game_state_t      game_state;
static uint16_t          invader_head;      /* player-side edge of the group  */
static uint16_t          game_tick_ms;
static uint16_t          tick_counter;
static uint16_t          last_invader_ms;
static uint16_t          last_shot_ms;

/* LCG PRNG state */
static uint16_t rng_state = 0xACE1u;

/* Idle/game-over blink state */
static uint16_t blink_ms;
static uint8_t  blink_on;

/* -------------------------------------------------------------------------
 * Button debounce
 * ---------------------------------------------------------------------- */
#define DEBOUNCE_MS  20u
typedef struct {
    uint8_t  last_raw;
    uint8_t  stable;
    uint16_t change_ms;
    uint8_t  pressed_event;
} button_t;

static button_t btn_pb1, btn_pb2, btn_pb3, btn_pb4;

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */
static void    Game_Init(void);
static void    Game_Update(void);
static void    WriteLEDs(uint8_t blink_invaders);
static void    Debounce_Update(button_t *b, uint8_t raw);
static uint8_t Rng_NextColor(void);

/* -------------------------------------------------------------------------
 * TMR0 1 ms callback (ISR context)
 * ---------------------------------------------------------------------- */
void Timer_1ms_Callback(void)
{
    ms_tick++;
}

/* -------------------------------------------------------------------------
 * Atomic 16-bit snapshot of ms_tick.
 * On an 8-bit PIC a 16-bit read is two instructions; the ISR can fire
 * between them and corrupt the value. Disable interrupts for the read.
 * ---------------------------------------------------------------------- */
static uint16_t Ms_Now(void)
{
    uint16_t t;
    INTERRUPT_GlobalInterruptDisable();
    t = ms_tick;
    INTERRUPT_GlobalInterruptEnable();
    return t;
}

/* -------------------------------------------------------------------------
 * Elapsed time helper (wraps safely at 65535)
 * ---------------------------------------------------------------------- */
static uint16_t Elapsed(uint16_t since)
{
    return (uint16_t)(Ms_Now() - since);
}

/* -------------------------------------------------------------------------
 * LCG pseudo-random — returns 1 (RED), 2 (GREEN), or 3 (BLUE)
 * ---------------------------------------------------------------------- */
static uint8_t Rng_NextColor(void)
{
    rng_state = rng_state * 25173u + 13849u;
    return (uint8_t)((rng_state >> 8u) % 3u) + 1u;
}

/* -------------------------------------------------------------------------
 * Button debounce
 * ---------------------------------------------------------------------- */
static void Debounce_Update(button_t *b, uint8_t raw)
{
    b->pressed_event = 0;
    if (raw != b->last_raw)
    {
        b->last_raw  = raw;
        b->change_ms = Ms_Now();
    }
    else if (raw != b->stable && Elapsed(b->change_ms) >= DEBOUNCE_MS)
    {
        uint8_t prev = b->stable;
        b->stable    = raw;
        if (b->stable == 1u && prev == 0u)
            b->pressed_event = 1;
    }
}

/* -------------------------------------------------------------------------
 * Write a single byte to SPI and block until transmission is complete.
 * SPI1_ByteWrite() only loads the buffer without waiting; we must poll
 * SSP1IF (via ByteExchange) to confirm the byte has fully shifted out.
 * ---------------------------------------------------------------------- */
static void SPI_SendByte(uint8_t byte)
{
    (void)SPI1_ByteExchange(byte);
}

/* -------------------------------------------------------------------------
 * Stream the entire strip to the WS2812 via SPI — no frame buffer needed.
 *
 * For each LED:
 *   1. Determine effective color: shot overlay takes priority over invader.
 *   2. In STATE_GAME_OVER with blink_invaders=0, invaders are dark.
 *   3. In STATE_IDLE, LED 0 pulses dim white; all others are off.
 *   4. Transmit G, R, B bytes in order (WS2812 GRB protocol).
 * ---------------------------------------------------------------------- */
static void WriteLEDs(uint8_t blink_invaders)
{
    uint16_t  i;
    uint8_t   g, r, b;
    color_id_t pixel;
    uint8_t   shot_idx;

    SPI1_Open(MSSP1_DEFAULT);

    for (i = 0; i < NUM_LEDS; i++)
    {
        /* Check if a shot occupies this LED */
        pixel    = COLOR_NONE;
        shot_idx = 0xFF;
        {
            uint8_t s;
            for (s = 0; s < NUM_SHOTS; s++)
            {
                if (shots[s].pos == (int16_t)i)
                {
                    shot_idx = s;
                    break;
                }
            }
        }

        if (shot_idx != 0xFF)
        {
            pixel = shots[shot_idx].color;
        }
        else if (game_state == STATE_PLAYING || blink_invaders)
        {
            pixel = strip[i];
        }
        /* else STATE_GAME_OVER blink-off or STATE_IDLE: pixel stays NONE */

        /* Resolve GRB bytes */
        g = 0; r = 0; b = 0;
        switch (pixel)
        {
            case COLOR_RED:
                g = COL_RED_G; r = COL_RED_R; b = COL_RED_B;
                break;
            case COLOR_GREEN:
                g = COL_GRN_G; r = COL_GRN_R; b = COL_GRN_B;
                break;
            case COLOR_BLUE:
                g = COL_BLU_G; r = COL_BLU_R; b = COL_BLU_B;
                break;
            default:
                /* Idle state: pulse LED 0 as dim white */
                if (game_state == STATE_IDLE && i == 0u && blink_on)
                {
                    g = COL_IDLE; r = COL_IDLE; b = COL_IDLE;
                }
                break;
        }

        SPI_SendByte(g);
        SPI_SendByte(r);
        SPI_SendByte(b);
    }

    SPI1_Close();
}

/* -------------------------------------------------------------------------
 * Initialize / restart the game
 * ---------------------------------------------------------------------- */
static void Game_Init(void)
{
    uint16_t i;

    /* Clear strip */
    for (i = 0; i < NUM_LEDS; i++)
        strip[i] = COLOR_NONE;

    /* Deactivate shots, assign colors */
    shots[0].pos = -1; shots[0].color = COLOR_RED;
    shots[1].pos = -1; shots[1].color = COLOR_GREEN;
    shots[2].pos = -1; shots[2].color = COLOR_BLUE;

    /* Seed 20 invaders at LEDs 280-299 */
    for (i = INVADER_START_POS; i < NUM_LEDS; i++)
        strip[i] = (color_id_t)Rng_NextColor();

    invader_head    = INVADER_START_POS;
    game_tick_ms    = GAME_TICK_INIT_MS;
    tick_counter    = 0;
    last_invader_ms = Ms_Now();
    last_shot_ms    = Ms_Now();
    game_state      = STATE_PLAYING;

    LED3_SetLow();
    LED4_SetLow();
    LED5_SetLow();
}

/* -------------------------------------------------------------------------
 * Fire a shot (called from button handler)
 * ---------------------------------------------------------------------- */
static void Fire_Shot(uint8_t shot_index)
{
    if (shots[shot_index].pos < 0)
        shots[shot_index].pos = 0;
}

/* -------------------------------------------------------------------------
 * Advance invaders one step toward the player (index 0).
 * Only the occupied window [invader_head .. NUM_LEDS-1] is shifted;
 * LEDs below invader_head are untouched (they are already COLOR_NONE).
 * ---------------------------------------------------------------------- */
static void Invaders_Advance(void)
{
    uint16_t i;

    if (invader_head == 0u)
    {
        game_state = STATE_GAME_OVER;
        return;
    }

    /* Write new position for each LED in the occupied window.
     * Destination index = i-1, source index = i. */
    for (i = invader_head; i < NUM_LEDS; i++)
        strip[i - 1u] = strip[i];

    /* Spawn a new random invader at the far end */
    strip[NUM_LEDS - 1u] = (color_id_t)Rng_NextColor();

    invader_head--;

    if (invader_head == 0u)
        game_state = STATE_GAME_OVER;
}

/* -------------------------------------------------------------------------
 * Advance all active shots one step
 * ---------------------------------------------------------------------- */
static void Shots_Advance(void)
{
    uint8_t i;

    for (i = 0; i < NUM_SHOTS; i++)
    {
        if (shots[i].pos < 0)
            continue;

        shots[i].pos++;

        if (shots[i].pos >= (int16_t)NUM_LEDS)
        {
            shots[i].pos = -1;
            continue;
        }

        if (strip[shots[i].pos] != COLOR_NONE)
        {
            if (strip[shots[i].pos] == shots[i].color)
                strip[shots[i].pos] = COLOR_NONE;   /* color match: destroy */
            shots[i].pos = -1;                      /* shot always consumed */
        }
    }
}

/* -------------------------------------------------------------------------
 * Main game logic update.
 * Timestamps are advanced by the fixed step (not reset to Ms_Now()) so
 * that a slow render loop cannot cause multiple advances in one pass.
 * ---------------------------------------------------------------------- */
static void Game_Update(void)
{
    uint16_t now = Ms_Now();

    if ((uint16_t)(now - last_shot_ms) >= SHOT_STEP_MS)
    {
        last_shot_ms += SHOT_STEP_MS;   /* advance by fixed step, not now */
        Shots_Advance();
    }

    if ((uint16_t)(now - last_invader_ms) >= game_tick_ms)
    {
        last_invader_ms += game_tick_ms;
        Invaders_Advance();

        if (game_state == STATE_GAME_OVER)
            return;

        tick_counter++;
        if ((tick_counter % SPEED_UP_EVERY) == 0u)
        {
            if (game_tick_ms > GAME_TICK_MIN_MS + SPEED_UP_STEP_MS)
                game_tick_ms -= SPEED_UP_STEP_MS;
            else
                game_tick_ms = GAME_TICK_MIN_MS;
        }
    }
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    SYSTEM_Initialize();

    TMR0_PeriodMatchCallbackRegister(Timer_1ms_Callback);
    Timer0_Start();

    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();

    /* Route SPI through the CLB WS2812 circuit */
    CLBSWINLbits.CLBSWIN0 = 1;
    __delay_ms(1);

    /* Init button state to zero */
    btn_pb1.last_raw = 0; btn_pb1.stable = 0;
    btn_pb1.change_ms = 0; btn_pb1.pressed_event = 0;
    btn_pb2.last_raw = 0; btn_pb2.stable = 0;
    btn_pb2.change_ms = 0; btn_pb2.pressed_event = 0;
    btn_pb3.last_raw = 0; btn_pb3.stable = 0;
    btn_pb3.change_ms = 0; btn_pb3.pressed_event = 0;
    btn_pb4.last_raw = 0; btn_pb4.stable = 0;
    btn_pb4.change_ms = 0; btn_pb4.pressed_event = 0;

    /* Clear shot array */
    shots[0].pos = -1; shots[0].color = COLOR_RED;
    shots[1].pos = -1; shots[1].color = COLOR_GREEN;
    shots[2].pos = -1; shots[2].color = COLOR_BLUE;

    /* Clear strip */
    {
        uint16_t i;
        for (i = 0; i < NUM_LEDS; i++)
            strip[i] = COLOR_NONE;
    }

    game_state = STATE_IDLE;
    ms_tick    = 0;
    blink_ms   = 0;
    blink_on   = 0;

    /* Send all-off frame to clear any previous state on the strip */
    WriteLEDs(0);

    while (1)
    {
        /* --- Debounce buttons --- */
        Debounce_Update(&btn_pb1, (uint8_t)PB1_GetValue());
        Debounce_Update(&btn_pb2, (uint8_t)PB2_GetValue());
        Debounce_Update(&btn_pb3, (uint8_t)PB3_GetValue());
        Debounce_Update(&btn_pb4, (uint8_t)PB4_GetValue());

        /* -----------------------------------------------------------
         * STATE: IDLE
         * --------------------------------------------------------- */
        if (game_state == STATE_IDLE)
        {
            if (btn_pb1.pressed_event)
            {
                Game_Init();
            }
            else if (Elapsed(blink_ms) >= 600u)
            {
                blink_ms = Ms_Now();
                blink_on = !blink_on;
                WriteLEDs(0);
            }
        }

        /* -----------------------------------------------------------
         * STATE: PLAYING
         * --------------------------------------------------------- */
        else if (game_state == STATE_PLAYING)
        {
            if (btn_pb1.pressed_event) Game_Init();
            if (btn_pb2.pressed_event) Fire_Shot(0);
            if (btn_pb3.pressed_event) Fire_Shot(1);
            if (btn_pb4.pressed_event) Fire_Shot(2);

            Game_Update();

            if (game_state == STATE_GAME_OVER)
            {
                blink_ms = Ms_Now();
                blink_on = 1;
                LED3_SetHigh();
                LED4_SetHigh();
                LED5_SetHigh();
            }

            /* Refresh strip every loop pass for smooth shot movement.
               Each full refresh takes ~9 ms (900 bytes at 800 kHz). */
            WriteLEDs(1);
        }

        /* -----------------------------------------------------------
         * STATE: GAME OVER
         * --------------------------------------------------------- */
        else if (game_state == STATE_GAME_OVER)
        {
            if (btn_pb1.pressed_event)
            {
                LED3_SetLow();
                LED4_SetLow();
                LED5_SetLow();
                Game_Init();
            }
            else if (Elapsed(blink_ms) >= 300u)
            {
                blink_ms = Ms_Now();
                blink_on = !blink_on;

                if (blink_on) { LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh(); }
                else          { LED3_SetLow();  LED4_SetLow();  LED5_SetLow();  }

                WriteLEDs(blink_on);
            }
        }
    }
}
