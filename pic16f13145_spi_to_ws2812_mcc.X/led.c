/*
 * led.c — LED strip driver for Space Invaders 1D
 *
 * The SK6812 strip is driven through the on-chip CLB, which converts the
 * MSSP1 SPI bitstream into WS2812-compatible pulse widths at 800 kHz.
 * Byte order per LED: G, R, B, W  (W is always 0 — we use RGB only).
 *
 * KEY TIMING RULE:
 *   SPI1_ByteExchange() polls until the byte finishes transmitting, then
 *   returns.  Any code between two SPI_SendByte() calls is a gap on the
 *   wire.  The SK6812 resets on any gap > 80 µs, restarting from LED 0.
 *
 *   Solution: compute all colours BEFORE opening SPI (WriteAllRed), or
 *   keep the per-LED computation inside the loop fast enough — a simple
 *   switch statement is only a few instructions and stays well under 80 µs
 *   at 32 MHz (WriteLEDs).
 */

#include "led.h"
#include "game.h"
#include "mcc_generated_files/system/system.h"

/* -------------------------------------------------------------------------
 * The game board — one byte per LED position.
 * Placed in BIGRAM so the 300-byte array fits in contiguous SRAM.
 * ---------------------------------------------------------------------- */
uint8_t strip[NUM_LEDS];

/* ms_tick is owned by main.c */
extern volatile uint16_t ms_tick;

/* How long one full SPI frame takes (used to compensate ms_tick) */
#define RENDER_MS  16u

/* Internal helper — send one byte and wait for it to finish */
static void SPI_SendByte(uint8_t byte)
{
    (void)SPI1_ByteExchange(byte);
}

/* -------------------------------------------------------------------------
 * WriteLEDs
 *
 * show_cells   = 1 : render every invader/shot cell in strip[].
 * show_cells   = 0 : all off, except LED 0 if idle_blink_on = 1.
 * idle_red         : 1 = use red idle colour (hard mode), 0 = white.
 *
 * Invader display colour comes from CELL_INV_COL() — bits 6-4 of the cell.
 * Shot colour comes from CELL_SHOT_COLOR() — bits 1-0.
 * ---------------------------------------------------------------------- */
void WriteLEDs(uint8_t show_cells, uint8_t idle_blink_on, uint8_t idle_red)
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

        if (show_cells)
        {
            cell = strip[i];
            if (CELL_IS_SHOT(cell))
            {
                switch (CELL_SHOT_COLOR(cell))
                {
                    case 1u: g = COL_RED_G; r = COL_RED_R; b = COL_RED_B; break;
                    case 2u: g = COL_GRN_G; r = COL_GRN_R; b = COL_GRN_B; break;
                    case 3u: g = COL_BLU_G; r = COL_BLU_R; b = COL_BLU_B; break;
                    default: break;
                }
            }
            else if (CELL_IS_INV(cell))
            {
                switch (CELL_INV_COL(cell))
                {
                    case INV_COL_RED:     g = COL_RED_G; r = COL_RED_R; b = COL_RED_B; break;
                    case INV_COL_GREEN:   g = COL_GRN_G; r = COL_GRN_R; b = COL_GRN_B; break;
                    case INV_COL_BLUE:    g = COL_BLU_G; r = COL_BLU_R; b = COL_BLU_B; break;
                    case INV_COL_CYAN:    g = COL_CYN_G; r = COL_CYN_R; b = COL_CYN_B; break;
                    case INV_COL_MAGENTA: g = COL_MAG_G; r = COL_MAG_R; b = COL_MAG_B; break;
                    case INV_COL_YELLOW:  g = COL_YEL_G; r = COL_YEL_R; b = COL_YEL_B; break;
                    case INV_COL_WHITE:   g = COL_WHT_G; r = COL_WHT_R; b = COL_WHT_B; break;
                    default: break;
                }
            }
        }
        else if (i == 0u && idle_blink_on)
        {
            if (idle_red)
            { g = 0; r = COL_IDLE_RED; b = 0; }
            else
            { g = COL_IDLE; r = COL_IDLE; b = COL_IDLE; }
        }

        /* SK6812 byte order: G R B W */
        SPI_SendByte(g);
        SPI_SendByte(r);
        SPI_SendByte(b);
        SPI_SendByte(0);   /* W channel unused */
    }

    SPI1_Close();
    __delay_us(100);
    ms_tick += RENDER_MS;
    INTERRUPT_GlobalInterruptEnable();
}

/* -------------------------------------------------------------------------
 * WriteAllRed — solid red (on=1) or off (on=0).
 * ---------------------------------------------------------------------- */
void WriteAllRed(uint8_t on)
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
 * Rainbow animation — private state
 * ---------------------------------------------------------------------- */
static uint8_t rainbow_offset;

void Rainbow_Init(void)
{
    uint16_t i;
    for (i = 0u; i < NUM_LEDS; i++)
    {
        /* Cycle display colour through R/G/B; mask=0 (no hit needed,
         * animation-only cells are never shot at) */
        strip[i] = MAKE_INV((i % 3u) + 1u, 0u);
    }
    anim_count     = RAINBOW_FRAMES;
    rainbow_offset = 1u;
    anim_ms        = ms_tick;
}

void Rainbow_Step(void)
{
    uint16_t i;

    if ((uint16_t)(ms_tick - anim_ms) < RAINBOW_FRAME_MS)
        return;

    anim_ms = ms_tick;

    for (i = NUM_LEDS - 1u; i > 0u; i--)
        strip[i] = strip[i - 1u];

    rainbow_offset++;
    if (rainbow_offset > 3u) rainbow_offset = 1u;

    strip[0] = MAKE_INV(rainbow_offset, 0u);   /* colour only, no hit mask */

    WriteLEDs(1, 0, 0);

    anim_count--;
    if (anim_count == 0u)
    {
        for (i = 0u; i < NUM_LEDS; i++)
            strip[i] = CELL_EMPTY;
        game_state = STATE_IDLE;
    }
}
