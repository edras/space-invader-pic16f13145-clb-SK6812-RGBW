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
 * Invaders march from index 299 toward 0. The group starts at LEDs
 * 280-299 and accelerates over time.
 *
 * Shots travel from index 0 toward 299. Firing writes a shot cell into
 * strip[0] if it is empty; multiple shots coexist naturally as separate
 * cells. Each runner tick moves every shot one step forward. If a shot
 * lands on an invader of the same colour both are removed; otherwise
 * only the shot is removed.
 *
 * Game over when any invader reaches index 0. PB1 restarts.
 *
 * RAM: strip[300] is placed in BIGRAM (program-memory-mapped SRAM,
 * 0x2000-0x23EF) — the only contiguous 300-byte region on PIC16F13145.
 * BIGRAM is true SRAM; writes do NOT touch flash.
 *
 * Interrupt / timing note:
 * Global interrupts are disabled for the entire WriteLEDs() call.
 * SK6812 strips treat any gap > ~80 us between bytes as a RESET,
 * so the 1 ms TMR0 interrupt must be masked during SPI streaming.
 * At 800 kHz, 1200 bytes take ~15 ms. WriteLEDs() adds RENDER_MS
 * back to ms_tick after re-enabling interrupts to compensate.
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
 * Cell encoding
 * ---------------------------------------------------------------------- */
#define CELL_EMPTY      0x00u
#define CELL_INV_RED    0x01u
#define CELL_INV_GREEN  0x02u
#define CELL_INV_BLUE   0x03u
#define CELL_SHOT_RED   0x81u
#define CELL_SHOT_GREEN 0x82u
#define CELL_SHOT_BLUE  0x83u

#define CELL_SHOT_FLAG  0x80u               /* bit set = shot, clear = invader */
#define CELL_COLOR(c)   ((uint8_t)((c) & 0x03u))  /* 1=red 2=green 3=blue     */
#define CELL_IS_SHOT(c) ((uint8_t)((c) & CELL_SHOT_FLAG))
#define CELL_IS_INV(c)  ((uint8_t)((c) != CELL_EMPTY && !CELL_IS_SHOT(c)))

/* -------------------------------------------------------------------------
 * Game constants
 * ---------------------------------------------------------------------- */
#define INVADER_START_POS   280u    /* first invader index at game start    */
#define GAME_TICK_INIT_MS   500u    /* initial ms between invader advances  */
#define GAME_TICK_MIN_MS    100u    /* fastest tick period                  */
#define SPEED_UP_EVERY      10u     /* ticks between speed increases        */
#define SPEED_UP_STEP_MS    30u     /* ms shaved off per speed-up           */
#define SHOT_STEP_MS        50u     /* ms between shot runner ticks         */

/* Time (ms) to clock out one full frame: 300 LEDs * 4 bytes * 10 bits
 * at 800 kHz = 15 ms. Rounded up to 16 to account for SPI overhead. */
#define RENDER_MS           16u

/* -------------------------------------------------------------------------
 * Game state
 * ---------------------------------------------------------------------- */
typedef enum {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_GAME_OVER
} game_state_t;

/* Unified strip array — holds both invaders and shots as encoded cells.
 * 300 bytes placed in BIGRAM by the linker (only contiguous region). */
static uint8_t strip[NUM_LEDS];

/* Timing / counters */
static volatile uint16_t ms_tick;
static game_state_t      game_state;
static uint16_t          invader_head;   /* player-side edge of invader group */
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
 * LCG pseudo-random — returns CELL_INV_RED/GREEN/BLUE
 * ---------------------------------------------------------------------- */
static uint8_t Rng_NextColor(void)
{
    rng_state = rng_state * 25173u + 13849u;
    return (uint8_t)((rng_state >> 8u) % 3u) + 1u;  /* 1, 2, or 3 */
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
 * ---------------------------------------------------------------------- */
static void SPI_SendByte(uint8_t byte)
{
    (void)SPI1_ByteExchange(byte);
}

/* -------------------------------------------------------------------------
 * Stream the entire strip to the SK6812 via SPI.
 *
 * Each cell value is mapped directly to GRB colour bytes — both invaders
 * and shots use the same colour bits (bits 1-0). The shot flag (bit 7)
 * is masked out before colour lookup.
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
            /* Idle: only LED 0 blinks dim white */
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
        SPI_SendByte(0);    /* W channel = 0 (SK6812 GRBW, white unused) */
    }

    SPI1_Close();
    __delay_us(100);
    ms_tick += RENDER_MS;
    INTERRUPT_GlobalInterruptEnable();
}

/* -------------------------------------------------------------------------
 * Initialize / restart the game
 * ---------------------------------------------------------------------- */
static void Game_Init(void)
{
    uint16_t i;

    for (i = 0; i < NUM_LEDS; i++)
        strip[i] = CELL_EMPTY;

    /* Seed 20 invaders at LEDs 280-299 */
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
 * Fire a shot of the given colour (shot_cell = CELL_SHOT_RED/GREEN/BLUE).
 * Places the shot at index 0 only if that cell is empty, so the player
 * cannot overwrite a shot already at the muzzle.
 * ---------------------------------------------------------------------- */
static void Fire_Shot(uint8_t shot_cell)
{
    if (strip[0] == CELL_EMPTY)
        strip[0] = shot_cell;
}

/* -------------------------------------------------------------------------
 * Runner: advance all shots one step toward the invaders (index 0 → 299).
 *
 * Walk from index NUM_LEDS-2 down to 0 (high-to-low so a shot does not
 * move more than one step per tick even if it was just placed).
 * For each shot cell at index i:
 *   - If strip[i+1] is an invader of the same colour: both vanish.
 *   - If strip[i+1] is an invader of a different colour: shot vanishes.
 *   - If strip[i+1] is empty: move shot to i+1, clear i.
 *   - If strip[i+1] is another shot: leave both in place (shots don't
 *     collide with each other).
 * ---------------------------------------------------------------------- */
static void Shots_Run(void)
{
    uint16_t i;
    uint8_t  cell;
    uint8_t  next;

    for (i = NUM_LEDS - 2u; ; i--)
    {
        cell = strip[i];
        if (CELL_IS_SHOT(cell))
        {
            next = strip[i + 1u];

            if (next == CELL_EMPTY)
            {
                /* Move shot forward */
                strip[i + 1u] = cell;
                strip[i]      = CELL_EMPTY;
            }
            else if (CELL_IS_INV(next))
            {
                /* Hit an invader */
                if (CELL_COLOR(next) == CELL_COLOR(cell))
                {
                    /* Colour match: both vanish, compact group */
                    uint16_t j;
                    for (j = i + 1u; j < NUM_LEDS - 1u; j++)
                        strip[j] = strip[j + 1u];
                    strip[NUM_LEDS - 1u] = CELL_EMPTY;
                    /* invader_head stays: next invader slides into i+1 */
                }
                /* Shot consumed regardless of colour match */
                strip[i] = CELL_EMPTY;
            }
            /* else next is another shot — leave both in place */
        }

        if (i == 0u) break;   /* avoid uint16_t wrap-around */
    }
}

/* -------------------------------------------------------------------------
 * Advance invaders one step toward the player (index 0).
 * Shifts the occupied window [invader_head .. NUM_LEDS-1] down by one.
 * Any shots inside the window are carried along with it (they are already
 * in strip[] and shift naturally).
 * ---------------------------------------------------------------------- */
static void Invaders_Advance(void)
{
    uint16_t i;

    if (invader_head == 0u)
    {
        game_state = STATE_GAME_OVER;
        return;
    }

    for (i = invader_head; i < NUM_LEDS; i++)
        strip[i - 1u] = strip[i];

    strip[NUM_LEDS - 1u] = CELL_EMPTY;
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

    btn_pb1.last_raw = 0; btn_pb1.stable = 0;
    btn_pb1.change_ms = 0; btn_pb1.pressed_event = 0;
    btn_pb2.last_raw = 0; btn_pb2.stable = 0;
    btn_pb2.change_ms = 0; btn_pb2.pressed_event = 0;
    btn_pb3.last_raw = 0; btn_pb3.stable = 0;
    btn_pb3.change_ms = 0; btn_pb3.pressed_event = 0;
    btn_pb4.last_raw = 0; btn_pb4.stable = 0;
    btn_pb4.change_ms = 0; btn_pb4.pressed_event = 0;

    {
        uint16_t i;
        for (i = 0; i < NUM_LEDS; i++)
            strip[i] = CELL_EMPTY;
    }

    game_state = STATE_IDLE;
    ms_tick    = 0;
    blink_ms   = 0;
    blink_on   = 0;

    WriteLEDs(0);

    while (1)
    {
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
            else if (Elapsed(blink_ms) >= 500u)
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
            if (btn_pb2.pressed_event) Fire_Shot(CELL_SHOT_RED);
            if (btn_pb3.pressed_event) Fire_Shot(CELL_SHOT_GREEN);
            if (btn_pb4.pressed_event) Fire_Shot(CELL_SHOT_BLUE);

            Game_Update();

            if (game_state == STATE_GAME_OVER)
            {
                blink_ms = Ms_Now();
                blink_on = 1;
                LED3_SetHigh();
                LED4_SetHigh();
                LED5_SetHigh();
            }

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
