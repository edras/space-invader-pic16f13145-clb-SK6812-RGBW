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
#include "mcc_generated_files/system/system.h"

/* -------------------------------------------------------------------------
 * The game board — one byte per LED position.
 * Placed in BIGRAM so the 300-byte array fits in contiguous SRAM.
 * ---------------------------------------------------------------------- */
uint8_t strip[NUM_LEDS];

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
 * show_cells = 1 : render every cell in strip[] using its colour ID.
 * show_cells = 0 : all LEDs off, except LED 0 if idle_blink_on = 1
 *                  (used to show the mode-select blink in IDLE state).
 * ---------------------------------------------------------------------- */
void WriteLEDs(uint8_t show_cells, uint8_t idle_blink_on)
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
            /* Render the cell colour stored in strip[i] */
            cell = strip[i];
            switch (CELL_COLOR(cell))
            {
                case 1u: g = COL_RED_G; r = COL_RED_R; b = COL_RED_B; break;
                case 2u: g = COL_GRN_G; r = COL_GRN_R; b = COL_GRN_B; break;
                case 3u: g = COL_BLU_G; r = COL_BLU_R; b = COL_BLU_B; break;
                default: break;   /* CELL_EMPTY → stay black */
            }
        }
        else if (i == 0u && idle_blink_on)
        {
            /* Idle blink: only LED 0 is lit, dimly white */
            g = COL_IDLE; r = COL_IDLE; b = COL_IDLE;
        }

        /* SK6812 byte order: G R B W */
        SPI_SendByte(g);
        SPI_SendByte(r);
        SPI_SendByte(b);
        SPI_SendByte(0);   /* W channel unused */
    }

    SPI1_Close();
    __delay_us(100);          /* latch pulse > 80 µs */
    ms_tick += RENDER_MS;     /* compensate for time spent with IRQs off */
    INTERRUPT_GlobalInterruptEnable();
}

/* -------------------------------------------------------------------------
 * WriteAllRed
 *
 * Colour is fixed before the loop — zero computation between bytes —
 * so all 300 LEDs are reached without any reset gap.
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
