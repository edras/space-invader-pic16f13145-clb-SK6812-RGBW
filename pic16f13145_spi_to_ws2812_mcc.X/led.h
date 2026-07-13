/*
 * led.h — LED strip driver for Space Invaders 1D
 *
 * Owns the strip[] array (300-cell game board), the cell encoding
 * constants, the colour definitions, and the two render functions.
 *
 * Cell encoding — one uint8_t per LED position:
 *
 *   Bit 7 = 0 → invader (or empty when bits 1-0 are also 0)
 *   Bit 7 = 1 → shot
 *   Bits 1-0  → colour:  1 = RED,  2 = GREEN,  3 = BLUE
 *
 *   Value   Meaning
 *   0x00    Empty
 *   0x01    Red   invader
 *   0x02    Green invader
 *   0x03    Blue  invader
 *   0x81    Red   shot
 *   0x82    Green shot
 *   0x83    Blue  shot
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Strip size
 * ---------------------------------------------------------------------- */
#define NUM_LEDS  300u

/* -------------------------------------------------------------------------
 * Brightness cap (~35% of full power to limit current draw)
 * ---------------------------------------------------------------------- */
#define BRIGHT(x)  ((uint8_t)(((uint16_t)(x) * 89u) / 255u))

/* -------------------------------------------------------------------------
 * GRB colour values for each game colour
 * ---------------------------------------------------------------------- */
#define COL_RED_G   0x00
#define COL_RED_R   BRIGHT(0xFF)
#define COL_RED_B   0x00

#define COL_GRN_G   BRIGHT(0xFF)
#define COL_GRN_R   0x00
#define COL_GRN_B   0x00

#define COL_BLU_G   0x00
#define COL_BLU_R   0x00
#define COL_BLU_B   BRIGHT(0xFF)

#define COL_IDLE    BRIGHT(0x20)   /* dim white used for the idle blink */

/* -------------------------------------------------------------------------
 * Cell constants and helper macros
 * ---------------------------------------------------------------------- */
#define CELL_EMPTY       0x00u
#define CELL_INV_RED     0x01u
#define CELL_INV_GREEN   0x02u
#define CELL_INV_BLUE    0x03u
#define CELL_SHOT_RED    0x81u
#define CELL_SHOT_GREEN  0x82u
#define CELL_SHOT_BLUE   0x83u

#define CELL_SHOT_FLAG   0x80u
#define CELL_COLOR(c)    ((uint8_t)((c) & 0x03u))
#define CELL_IS_SHOT(c)  ((uint8_t)((c) & CELL_SHOT_FLAG))
#define CELL_IS_INV(c)   ((uint8_t)((c) != CELL_EMPTY && !CELL_IS_SHOT(c)))

/* -------------------------------------------------------------------------
 * The game board — 300 bytes in BIGRAM (program-memory-mapped SRAM).
 * Declared extern so all modules share the same array.
 * ---------------------------------------------------------------------- */
extern uint8_t strip[NUM_LEDS];

/* -------------------------------------------------------------------------
 * Rainbow animation constants
 * ---------------------------------------------------------------------- */
#define RAINBOW_FRAMES    90u   /* total frames in the win animation         */
#define RAINBOW_FRAME_MS  33u   /* ms between frames (~30 fps)               */

/* -------------------------------------------------------------------------
 * Functions
 * ---------------------------------------------------------------------- */

/*
 * Rainbow_Init — call once when entering STATE_WIN.
 * Pre-fills strip[] with a repeating R/G/B pattern so the first frame
 * already shows a full rainbow across all 300 LEDs, and resets counters.
 */
void Rainbow_Init(void);

/*
 * Rainbow_Step — call every main loop iteration while in STATE_WIN.
 * Each RAINBOW_FRAME_MS tick: shifts the wave one step toward the far end,
 * feeds the next colour into index 0, and calls WriteLEDs().
 * Sets game_state = STATE_IDLE automatically when the animation finishes.
 */
void Rainbow_Step(void);

/*
 * WriteLEDs — send the current strip[] state to the physical LED strip.
 *
 *   show_cells   = 1 : render all cell colours (game, win animation).
 *   show_cells   = 0 : all LEDs off, except LED 0 if idle_blink_on = 1.
 *   idle_blink_on    : when show_cells = 0, controls the idle blink LED.
 */
void WriteLEDs(uint8_t show_cells, uint8_t idle_blink_on);

/*
 * WriteAllRed — fill the entire strip with solid red (on=1) or off (on=0).
 * Used for the game-over blink animation.
 */
void WriteAllRed(uint8_t on);

#endif /* LED_H */
