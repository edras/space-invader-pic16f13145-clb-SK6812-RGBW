/*
 * Space Invaders 1D - PIC16F13145 + SK6812 GRBW LED Strip
 *
 * LED strip: 300 LEDs (SK6812 GRBW, 4 bytes/LED), index 0 = player end,
 * index 299 = spawn end.
 *
 * All game state lives in strip[300]. Each cell is a uint8_t:
 *
 *   0x00        empty
 *   0x01        RED   invader
 *   0x02        GREEN invader
 *   0x03        BLUE  invader
 *   0x81        RED   shot     (bit 7 = shot flag)
 *   0x82        GREEN shot
 *   0x83        BLUE  shot
 *
 * Two game modes selected in IDLE by short-pressing PB1:
 *   MODE_CLASSIC  — fixed group of 20 invaders, you can win by destroying
 *                   them all (rainbow celebration). Idle blink: 1 s.
 *   MODE_ENDLESS  — a new invader is appended every step so the group
 *                   grows forever; survive as long as possible.
 *                   Idle blink: fast (150 ms).
 * Long-press PB1 (>= 1 s) starts the selected mode.
 *
 * Win  (classic only): rainbow sweep across the strip, then back to idle.
 * Lose: entire strip blinks red three times, then back to idle.
 *
 * Buttons:
 *   PB1 (RC4) - Short press: cycle mode in IDLE / restart in-game
 *               Long press:  start selected mode
 *   PB2 (RC5) - Fire red shot
 *   PB3 (RA4) - Fire green shot
 *   PB4 (RC3) - Fire blue shot
 *
 * On-board LEDs: LED3/LED4/LED5 used as mode indicators in IDLE.
 *
 * RAM: strip[300] is in BIGRAM (program-memory-mapped SRAM, 0x2000-0x23EF).
 * BIGRAM is true SRAM; writes do NOT touch flash.
 *
 * Timing: interrupts disabled during WriteLEDs (~15 ms at 800 kHz).
 * ms_tick is compensated by RENDER_MS after each frame.
 */

#include "mcc_generated_files/system/system.h"

/* -------------------------------------------------------------------------
 * Hardware / strip constants
 * ---------------------------------------------------------------------- */
#define NUM_LEDS            300u

/* Brightness cap ~35% */
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

#define COL_IDLE    BRIGHT(0x20)   /* dim white for idle blink */

/* -------------------------------------------------------------------------
 * Cell encoding
 * ---------------------------------------------------------------------- */
#define CELL_EMPTY      0x00u
#define CELL_INV_RED    0x01u
#define CELL_INV_GREEN  0x02u
#define CELL_INV_BLUE   0x03u
#define CELL_SHOT_RED   0x81u
#define CELL_SHOT_GREEN 0x82u
#define CELL_SHOT_BLUE  0x83u

#define CELL_SHOT_FLAG  0x80u
#define CELL_COLOR(c)   ((uint8_t)((c) & 0x03u))
#define CELL_IS_SHOT(c) ((uint8_t)((c) & CELL_SHOT_FLAG))
#define CELL_IS_INV(c)  ((uint8_t)((c) != CELL_EMPTY && !CELL_IS_SHOT(c)))

/* -------------------------------------------------------------------------
 * Game constants
 * ---------------------------------------------------------------------- */
#define INVADER_START_POS   280u
#define GAME_TICK_INIT_MS   500u
#define GAME_TICK_MIN_MS    100u
#define SPEED_UP_EVERY      10u
#define SPEED_UP_STEP_MS    30u
#define SHOT_STEP_MS        25u
#define RENDER_MS           16u

/* Idle blink periods */
#define BLINK_CLASSIC_MS    500u    /* 1 s total (500 ms half-period)       */
#define BLINK_ENDLESS_MS    75u     /* 150 ms total (fast)                  */

/* Long-press threshold to start the game */
#define LONG_PRESS_MS       1000u

/* Game-over strip blink: number of red flashes and their period */
#define GAMEOVER_FLASHES    6u      /* 3 on + 3 off = 3 full blinks         */
#define GAMEOVER_BLINK_MS   200u

/* Rainbow: number of frames to show and frame period */
#define RAINBOW_FRAMES      90u
#define RAINBOW_FRAME_MS    33u

/* -------------------------------------------------------------------------
 * Game / UI states
 * ---------------------------------------------------------------------- */
typedef enum {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_WIN,
    STATE_GAME_OVER
} game_state_t;

typedef enum {
    MODE_CLASSIC = 0,
    MODE_ENDLESS  = 1
} game_mode_t;

/* -------------------------------------------------------------------------
 * Global variables
 * ---------------------------------------------------------------------- */
static uint8_t strip[NUM_LEDS];      /* unified cell array in BIGRAM        */

static volatile uint16_t ms_tick;
static game_state_t      game_state;
static game_mode_t       game_mode;
static uint16_t          invader_head;
static uint16_t          game_tick_ms;
static uint16_t          tick_counter;
static uint16_t          last_invader_ms;
static uint16_t          last_shot_ms;

static uint16_t rng_state = 0xACE1u;

static uint16_t blink_ms;
static uint8_t  blink_on;

/* Win/lose animation counters */
static uint8_t  anim_count;         /* frames remaining in animation        */
static uint16_t anim_ms;            /* last animation tick timestamp        */
static uint8_t  rainbow_offset;     /* hue offset for rainbow sweep         */

/* -------------------------------------------------------------------------
 * Button debounce — on-release short/long press classification.
 *
 * pressed_event    fires on release if held < LONG_PRESS_MS  (short press)
 * long_press_event fires on release if held >= LONG_PRESS_MS (long press)
 * ---------------------------------------------------------------------- */
#define DEBOUNCE_MS  20u
typedef struct {
    uint8_t  last_raw;
    uint8_t  stable;
    uint16_t change_ms;     /* timestamp of last raw-level change (debounce) */
    uint16_t press_ms;      /* timestamp when stable press was confirmed      */
    uint8_t  pressed_event;
    uint8_t  long_press_event;
    uint8_t  long_fired;    /* marks that hold threshold was crossed          */
} button_t;

static button_t btn_pb1, btn_pb2, btn_pb3, btn_pb4;

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */
static void    Game_Init(void);
static void    Game_Update(void);
static void    WriteLEDs(uint8_t blink_invaders);

static void    WriteAllRed(uint8_t on);
static void    Debounce_Update(button_t *b, uint8_t raw);
static uint8_t Rng_NextColor(void);

/* -------------------------------------------------------------------------
 * TMR0 1 ms callback
 * ---------------------------------------------------------------------- */
void Timer_1ms_Callback(void)
{
    ms_tick++;
}

static uint16_t Ms_Now(void)
{
    uint16_t t;
    INTERRUPT_GlobalInterruptDisable();
    t = ms_tick;
    INTERRUPT_GlobalInterruptEnable();
    return t;
}

static uint16_t Elapsed(uint16_t since)
{
    return (uint16_t)(Ms_Now() - since);
}

/* -------------------------------------------------------------------------
 * LCG PRNG — returns 1 (RED), 2 (GREEN), or 3 (BLUE)
 * ---------------------------------------------------------------------- */
static uint8_t Rng_NextColor(void)
{
    rng_state = rng_state * 25173u + 13849u;
    return (uint8_t)((rng_state >> 8u) % 3u) + 1u;
}

/* -------------------------------------------------------------------------
 * Button debounce + on-release short/long press classification.
 *
 * Events are generated on RELEASE so the full hold duration is known:
 *   pressed_event    = released after  < LONG_PRESS_MS  (short press)
 *   long_press_event = released after >= LONG_PRESS_MS  (long press)
 * ---------------------------------------------------------------------- */
static void Debounce_Update(button_t *b, uint8_t raw)
{
    b->pressed_event    = 0;
    b->long_press_event = 0;

    /* --- Debounce: detect stable edges -------------------------------
     * Track raw changes; only accept a new level after it has been
     * stable for DEBOUNCE_MS. */
    if (raw != b->last_raw)
    {
        b->last_raw  = raw;
        b->change_ms = Ms_Now();
    }
    else if (raw != b->stable && Elapsed(b->change_ms) >= DEBOUNCE_MS)
    {
        uint8_t prev = b->stable;
        b->stable    = raw;

        if (prev == 0u && raw == 1u)
        {
            /* Press confirmed: start timing the hold */
            b->press_ms   = Ms_Now();
            b->long_fired = 0;
        }
        else if (prev == 1u && raw == 0u)
        {
            /* Release confirmed: classify by hold duration */
            if (b->long_fired)
                b->long_press_event = 1;
            else
                b->pressed_event    = 1;
        }
    }

    /* --- Long-press threshold: runs every call while button is held --
     * Must be outside the debounce block so it updates even when
     * raw == stable (no edge, just held). */
    if (b->stable == 1u && !b->long_fired &&
        Elapsed(b->press_ms) >= LONG_PRESS_MS)
    {
        b->long_fired = 1;
    }
}

/* -------------------------------------------------------------------------
 * SPI byte helper
 * ---------------------------------------------------------------------- */
static void SPI_SendByte(uint8_t byte)
{
    (void)SPI1_ByteExchange(byte);
}

/* -------------------------------------------------------------------------
 * Render current strip[] state to LEDs.
 * blink_invaders: 1 = show invaders (PLAYING or win blink-on),
 *                 0 = invaders dark (idle / blink-off).
 * ---------------------------------------------------------------------- */
static void WriteLEDs(uint8_t blink_invaders)
{
    uint16_t i;
    uint8_t  g, r, b;
    uint8_t  cell;

    INTERRUPT_GlobalInterruptDisable();
    SPI1_Open(MSSP1_DEFAULT);
    PIR5bits.SSP1IF = 0U;

    for (i = 0; i < NUM_LEDS; i++)
    {
        g = 0; r = 0; b = 0;

        if (game_state == STATE_IDLE)
        {
            if (i == 0u && blink_on)
            {
                g = COL_IDLE; r = COL_IDLE; b = COL_IDLE;
            }
        }
        else if (game_state == STATE_PLAYING || blink_invaders)
        {
            cell = strip[i];
            switch (CELL_COLOR(cell))
            {
                case 1u: g = COL_RED_G; r = COL_RED_R; b = COL_RED_B; break;
                case 2u: g = COL_GRN_G; r = COL_GRN_R; b = COL_GRN_B; break;
                case 3u: g = COL_BLU_G; r = COL_BLU_R; b = COL_BLU_B; break;
                default: break;
            }
        }

        SPI_SendByte(g);
        SPI_SendByte(r);
        SPI_SendByte(b);
        SPI_SendByte(0);
    }

    SPI1_Close();
    __delay_us(100);
    ms_tick += RENDER_MS;
    INTERRUPT_GlobalInterruptEnable();
}

/* -------------------------------------------------------------------------
 * Render full strip in solid red (on=1) or all off (on=0).
 * Used for game-over flash.
 * ---------------------------------------------------------------------- */
static void WriteAllRed(uint8_t on)
{
    uint16_t i;
    uint8_t  g = 0, r = 0, b = 0;

    if (on) { r = COL_RED_R; }

    INTERRUPT_GlobalInterruptDisable();
    SPI1_Open(MSSP1_DEFAULT);
    PIR5bits.SSP1IF = 0U;

    for (i = 0; i < NUM_LEDS; i++)
    {
        SPI_SendByte(g);
        SPI_SendByte(r);
        SPI_SendByte(b);
        SPI_SendByte(0);
    }

    SPI1_Close();
    __delay_us(100);
    ms_tick += RENDER_MS;
    INTERRUPT_GlobalInterruptEnable();
}

/* -------------------------------------------------------------------------
 * Initialize / restart the game in the current game_mode.
 * ---------------------------------------------------------------------- */
static void Game_Init(void)
{
    uint16_t i;

    for (i = 0; i < NUM_LEDS; i++)
        strip[i] = CELL_EMPTY;

    for (i = INVADER_START_POS; i < NUM_LEDS; i++)
        strip[i] = Rng_NextColor();

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
 * Fire a shot — places shot cell at strip[0] if empty.
 * ---------------------------------------------------------------------- */
static void Fire_Shot(uint8_t shot_cell)
{
    if (strip[0] == CELL_EMPTY)
        strip[0] = shot_cell;
}

/* -------------------------------------------------------------------------
 * Check if all invaders have been destroyed (no invader cells remain).
 * Only meaningful in MODE_CLASSIC.
 * ---------------------------------------------------------------------- */
static uint8_t All_Invaders_Gone(void)
{
    uint16_t i;
    for (i = 0; i < NUM_LEDS; i++)
        if (CELL_IS_INV(strip[i]))
            return 0;
    return 1;
}

/* -------------------------------------------------------------------------
 * Shot runner: advance all shots one step toward the invaders.
 * Walk high-to-low so each shot moves at most one step per tick.
 * ---------------------------------------------------------------------- */
static void Shots_Run(void)
{
    uint16_t i;
    uint8_t  cell, next;

    for (i = NUM_LEDS - 2u; ; i--)
    {
        cell = strip[i];
        if (CELL_IS_SHOT(cell))
        {
            next = strip[i + 1u];

            if (next == CELL_EMPTY)
            {
                strip[i + 1u] = cell;
                strip[i]      = CELL_EMPTY;
            }
            else if (CELL_IS_INV(next))
            {
                if (CELL_COLOR(next) == CELL_COLOR(cell))
                {
                    if (game_mode == MODE_CLASSIC)
                    {
                        /* Classic: compact the group so it shrinks */
                        uint16_t j;
                        for (j = i + 1u; j < NUM_LEDS - 1u; j++)
                            strip[j] = strip[j + 1u];
                        strip[NUM_LEDS - 1u] = CELL_EMPTY;
                    }
                    else
                    {
                        /* Endless: just clear the invader in place —
                         * the gap will be filled by the next
                         * Invaders_Advance append at NUM_LEDS-1 */
                        strip[i + 1u] = CELL_EMPTY;
                    }
                }
                strip[i] = CELL_EMPTY;  /* shot consumed either way */
            }
            /* else next is another shot — leave both in place */
        }

        if (i == 0u) break;
    }
}

/* -------------------------------------------------------------------------
 * Invader advance: shift group one step toward the player.
 * In MODE_ENDLESS a new random invader is appended at the tail so the
 * group never shrinks — only growing and advancing.
 *
 * Collision during advance: before moving each invader one step left,
 * check whether the destination cell holds a shot.  If it does, resolve
 * the collision in place so that faster invader speeds cannot bypass the
 * detection in Shots_Run().
 *   - Colour match  : invader destroyed, shot consumed, cell stays empty.
 *   - Colour mismatch: shot consumed, invader moves through normally.
 * ---------------------------------------------------------------------- */
static void Invaders_Advance(void)
{
    uint16_t i;
    uint8_t  inv, dst;

    if (invader_head == 0u)
    {
        game_state = STATE_GAME_OVER;
        return;
    }

    /* Shift every invader one step toward the player */
    for (i = invader_head; i < NUM_LEDS; i++)
    {
        inv = strip[i];
        if (!CELL_IS_INV(inv))
            continue;

        dst = strip[i - 1u];

        if (CELL_IS_SHOT(dst))
        {
            strip[i] = CELL_EMPTY;
            if (CELL_COLOR(dst) == CELL_COLOR(inv))
                strip[i - 1u] = CELL_EMPTY;   /* match: both gone */
            else
                strip[i - 1u] = inv;           /* mismatch: invader moves through */
        }
        else
        {
            strip[i - 1u] = inv;
            strip[i]      = CELL_EMPTY;
        }
    }

    strip[NUM_LEDS - 1u] = (game_mode == MODE_ENDLESS)
                           ? Rng_NextColor()
                           : CELL_EMPTY;
    invader_head--;

    if (invader_head == 0u)
        game_state = STATE_GAME_OVER;
}

/* -------------------------------------------------------------------------
 * Main game logic update.
 * ---------------------------------------------------------------------- */
static void Game_Update(void)
{
    uint16_t now = Ms_Now();

    if ((uint16_t)(now - last_shot_ms) >= SHOT_STEP_MS)
    {
        last_shot_ms += SHOT_STEP_MS;
        Shots_Run();

        /* Win check: only in classic mode, after shots run */
        if (game_mode == MODE_CLASSIC && All_Invaders_Gone())
        {
            uint16_t wi;
            /* Pre-fill strip with repeating R/G/B pattern so the wave
             * is already visible on the first rendered frame */
            for (wi = 0u; wi < NUM_LEDS; wi++)
                strip[wi] = (uint8_t)((wi % 3u) + 1u);  /* 1,2,3,1,2,3,… */
            game_state     = STATE_WIN;
            anim_count     = RAINBOW_FRAMES;
            rainbow_offset = 1u;   /* next colour fed into index 0 */
            anim_ms        = Ms_Now();
            return;
        }
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

    CLBSWINLbits.CLBSWIN0 = 1;
    __delay_ms(1);

    btn_pb1.last_raw = 0; btn_pb1.stable = 0; btn_pb1.change_ms = 0;
    btn_pb1.press_ms = 0; btn_pb1.pressed_event = 0;
    btn_pb1.long_press_event = 0; btn_pb1.long_fired = 0;
    btn_pb2.last_raw = 0; btn_pb2.stable = 0; btn_pb2.change_ms = 0;
    btn_pb2.press_ms = 0; btn_pb2.pressed_event = 0;
    btn_pb2.long_press_event = 0; btn_pb2.long_fired = 0;
    btn_pb3.last_raw = 0; btn_pb3.stable = 0; btn_pb3.change_ms = 0;
    btn_pb3.press_ms = 0; btn_pb3.pressed_event = 0;
    btn_pb3.long_press_event = 0; btn_pb3.long_fired = 0;
    btn_pb4.last_raw = 0; btn_pb4.stable = 0; btn_pb4.change_ms = 0;
    btn_pb4.press_ms = 0; btn_pb4.pressed_event = 0;
    btn_pb4.long_press_event = 0; btn_pb4.long_fired = 0;

    {
        uint16_t i;
        for (i = 0; i < NUM_LEDS; i++)
            strip[i] = CELL_EMPTY;
    }

    game_state = STATE_IDLE;
    game_mode  = MODE_CLASSIC;
    ms_tick    = 0;
    blink_ms   = 0;
    blink_on   = 0;
    anim_count = 0;
    anim_ms    = 0;

    WriteLEDs(0);

    while (1)
    {
        Debounce_Update(&btn_pb1, (uint8_t)PB1_GetValue());
        Debounce_Update(&btn_pb2, (uint8_t)PB2_GetValue());
        Debounce_Update(&btn_pb3, (uint8_t)PB3_GetValue());
        Debounce_Update(&btn_pb4, (uint8_t)PB4_GetValue());

        /* -----------------------------------------------------------
         * STATE: IDLE
         * Short press PB1 → start the selected mode.
         * Long  press PB1 → cycle to the other mode.
         * --------------------------------------------------------- */
        if (game_state == STATE_IDLE)
        {
            if (btn_pb1.pressed_event)
            {
                Game_Init();
            }
            else if (btn_pb1.long_press_event)
            {
                /* Cycle mode */
                game_mode = (game_mode == MODE_CLASSIC)
                            ? MODE_ENDLESS : MODE_CLASSIC;
                blink_ms  = Ms_Now();
                blink_on  = 1;
                WriteLEDs(0);
            }
            else
            {
                uint16_t period = (game_mode == MODE_CLASSIC)
                                  ? BLINK_CLASSIC_MS : BLINK_ENDLESS_MS;
                if (Elapsed(blink_ms) >= period)
                {
                    blink_ms = Ms_Now();
                    blink_on = !blink_on;
                    WriteLEDs(0);
                }
            }
        }

        /* -----------------------------------------------------------
         * STATE: PLAYING
         * Long press PB1 → abort game and return to IDLE.
         * --------------------------------------------------------- */
        else if (game_state == STATE_PLAYING)
        {
            if (btn_pb1.long_press_event)
            {
                uint16_t ci;
                for (ci = 0; ci < NUM_LEDS; ci++)
                    strip[ci] = CELL_EMPTY;
                game_state = STATE_IDLE;
                blink_ms   = Ms_Now();
                blink_on   = 0;
                WriteLEDs(0);
            }
            if (btn_pb2.pressed_event) Fire_Shot(CELL_SHOT_RED);
            if (btn_pb3.pressed_event) Fire_Shot(CELL_SHOT_GREEN);
            if (btn_pb4.pressed_event) Fire_Shot(CELL_SHOT_BLUE);

            Game_Update();

            if (game_state == STATE_GAME_OVER)
            {
                anim_count = GAMEOVER_FLASHES;
                blink_on   = 1;
                anim_ms    = Ms_Now();
                LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh();
                WriteAllRed(1);
            }

            if (game_state == STATE_PLAYING)
                WriteLEDs(1);
        }

        /* -----------------------------------------------------------
         * STATE: WIN — rainbow wave animation.
         * Each tick: shift all cells one step toward index 299, then
         * feed the next colour in the R→G→B cycle into index 0.
         * WriteLEDs(1) renders strip[] exactly as during gameplay.
         * --------------------------------------------------------- */
        else if (game_state == STATE_WIN)
        {
            if (Elapsed(anim_ms) >= RAINBOW_FRAME_MS)
            {
                uint16_t i;
                anim_ms = Ms_Now();

                /* Shift strip toward the far end */
                for (i = NUM_LEDS - 1u; i > 0u; i--)
                    strip[i] = strip[i - 1u];

                /* Feed next colour into index 0 (cycles R→G→B→R…) */
                rainbow_offset++;
                if (rainbow_offset > 3u) rainbow_offset = 1u;
                strip[0] = rainbow_offset;

                WriteLEDs(1);

                anim_count--;
                if (anim_count == 0u)
                {
                    for (i = 0; i < NUM_LEDS; i++)
                        strip[i] = CELL_EMPTY;
                    game_state = STATE_IDLE;
                    blink_ms   = Ms_Now();
                    blink_on   = 0;
                    WriteLEDs(0);
                }
            }
        }

        /* -----------------------------------------------------------
         * STATE: GAME OVER — blink strip red, then return to IDLE
         * --------------------------------------------------------- */
        else if (game_state == STATE_GAME_OVER)
        {
            if (btn_pb1.pressed_event)
            {
                /* Early exit to idle */
                LED3_SetLow(); LED4_SetLow(); LED5_SetLow();
                WriteAllRed(0);
                game_state = STATE_IDLE;
                blink_ms   = Ms_Now();
                blink_on   = 0;
            }
            else if (Elapsed(anim_ms) >= GAMEOVER_BLINK_MS)
            {
                anim_ms  = Ms_Now();
                blink_on = !blink_on;
                WriteAllRed(blink_on);

                if (blink_on)  { LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh(); }
                else           { LED3_SetLow();  LED4_SetLow();  LED5_SetLow();  }

                if (anim_count > 0u)
                    anim_count--;

                if (anim_count == 0u)
                {
                    /* Animation done — back to idle */
                    LED3_SetLow(); LED4_SetLow(); LED5_SetLow();
                    game_state = STATE_IDLE;
                    blink_ms   = Ms_Now();
                    blink_on   = 0;
                    WriteLEDs(0);
                }
            }
        }
    }
}
