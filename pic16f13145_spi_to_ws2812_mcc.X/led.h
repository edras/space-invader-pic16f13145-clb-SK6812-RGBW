/*
 * led.h — LED strip driver for Space Invaders 1D
 *
 * Owns the strip[] array (300-cell game board), the cell encoding
 * constants, the colour definitions, and the render functions.
 *
 * ---------------------------------------------------------------------------
 * CELL ENCODING — one uint8_t per LED position
 * ---------------------------------------------------------------------------
 *
 *  Bit 7 = 1  →  SHOT
 *    bits 1-0  = shot colour  (1=R  2=G  3=B)
 *    Use CELL_IS_SHOT(c) and CELL_SHOT_COLOR(c).
 *
 *  Bit 7 = 0, value != 0  →  INVADER
 *    bits 6-4  = display colour  (INV_COL_* constants below)
 *    bits 2-0  = remaining-shots bitmask
 *                  bit 0 = red   shot still needed
 *                  bit 1 = green shot still needed
 *                  bit 2 = blue  shot still needed
 *    Use CELL_IS_INV(c), CELL_INV_COL(c), CELL_INV_MASK(c), MAKE_INV(col,mask).
 *
 *  Value 0x00  →  EMPTY
 *
 * Easy-mode colours (one shot each):
 *   RED     MAKE_INV(INV_COL_RED,     INV_NEED_R)        = 0x11
 *   GREEN   MAKE_INV(INV_COL_GREEN,   INV_NEED_G)        = 0x22
 *   BLUE    MAKE_INV(INV_COL_BLUE,    INV_NEED_B)        = 0x34
 *
 * Hard-mode colours (two or three shots):
 *   CYAN    MAKE_INV(INV_COL_CYAN,    INV_NEED_G|INV_NEED_B) = 0x46
 *   MAGENTA MAKE_INV(INV_COL_MAGENTA, INV_NEED_R|INV_NEED_B) = 0x55
 *   YELLOW  MAKE_INV(INV_COL_YELLOW,  INV_NEED_R|INV_NEED_G) = 0x63
 *   WHITE   MAKE_INV(INV_COL_WHITE,   INV_NEED_R|INV_NEED_G|INV_NEED_B) = 0x77
 *
 * A hit removes the matching bit from the mask.  When the mask reaches 0
 * the invader is destroyed.  The display colour updates as bits are cleared
 * so the player can see which shots are still needed.
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * LED strip type selection
 *   LED_TYPE_RGBW — SK6812 RGBW (default, 4 bytes per LED: G R B W)
 *   LED_TYPE_RGB  — WS2812 / NeoPixel RGB (3 bytes per LED: G R B)
 * ---------------------------------------------------------------------- */
#define LED_TYPE_RGBW
/* #define LED_TYPE_RGB */

/* -------------------------------------------------------------------------
 * Strip size
 * ---------------------------------------------------------------------- */
#define NUM_LEDS  300u

/* -------------------------------------------------------------------------
 * Brightness cap (~35% of full power to limit current draw)
 * ---------------------------------------------------------------------- */
#define BRIGHT(x)  ((uint8_t)(((uint16_t)(x) * 89u) / 255u))

/* -------------------------------------------------------------------------
 * GRB colour values for each display colour
 * ---------------------------------------------------------------------- */
#define COL_RED_G      0x00
#define COL_RED_R      BRIGHT(0xFF)
#define COL_RED_B      0x00

#define COL_GRN_G      BRIGHT(0xFF)
#define COL_GRN_R      0x00
#define COL_GRN_B      0x00

#define COL_BLU_G      0x00
#define COL_BLU_R      0x00
#define COL_BLU_B      BRIGHT(0xFF)

#define COL_CYN_G      BRIGHT(0xFF)   /* Cyan    = Green + Blue  */
#define COL_CYN_R      0x00
#define COL_CYN_B      BRIGHT(0xFF)

#define COL_MAG_G      0x00           /* Magenta = Red + Blue    */
#define COL_MAG_R      BRIGHT(0xFF)
#define COL_MAG_B      BRIGHT(0xFF)

#define COL_YEL_G      BRIGHT(0xFF)   /* Yellow  = Red + Green   */
#define COL_YEL_R      BRIGHT(0xFF)
#define COL_YEL_B      0x00

#define COL_WHT_G      BRIGHT(0xFF)   /* White   = Red+Green+Blue */
#define COL_WHT_R      BRIGHT(0xFF)
#define COL_WHT_B      BRIGHT(0xFF)

#define COL_IDLE       BRIGHT(0x20)   /* dim white for idle blink */
#define COL_IDLE_RED   BRIGHT(0x40)   /* dim red for hard-mode idle blink */

/* -------------------------------------------------------------------------
 * Invader display colour IDs  (stored in bits 6-4 of the cell byte)
 * ---------------------------------------------------------------------- */
#define INV_COL_RED      1u
#define INV_COL_GREEN    2u
#define INV_COL_BLUE     3u
#define INV_COL_CYAN     4u
#define INV_COL_MAGENTA  5u
#define INV_COL_YELLOW   6u
#define INV_COL_WHITE    7u

/* -------------------------------------------------------------------------
 * Remaining-shot bitmask bits  (stored in bits 2-0 of the cell byte)
 * ---------------------------------------------------------------------- */
#define INV_NEED_R  0x01u   /* red   shot still required  */
#define INV_NEED_G  0x02u   /* green shot still required  */
#define INV_NEED_B  0x04u   /* blue  shot still required  */

/* -------------------------------------------------------------------------
 * Cell helpers
 * ---------------------------------------------------------------------- */
#define CELL_EMPTY        0x00u
#define CELL_SHOT_FLAG    0x80u

/* Build a fresh invader cell from a display colour ID and a shot mask */
#define MAKE_INV(col, mask)    ((uint8_t)(((uint8_t)(col) << 4u) | (uint8_t)(mask)))

/* Ready-made easy-mode invader constants */
#define CELL_INV_RED     MAKE_INV(INV_COL_RED,     INV_NEED_R)
#define CELL_INV_GREEN   MAKE_INV(INV_COL_GREEN,   INV_NEED_G)
#define CELL_INV_BLUE    MAKE_INV(INV_COL_BLUE,    INV_NEED_B)

/* Ready-made hard-mode invader constants */
#define CELL_INV_CYAN    MAKE_INV(INV_COL_CYAN,    INV_NEED_G | INV_NEED_B)
#define CELL_INV_MAGENTA MAKE_INV(INV_COL_MAGENTA, INV_NEED_R | INV_NEED_B)
#define CELL_INV_YELLOW  MAKE_INV(INV_COL_YELLOW,  INV_NEED_R | INV_NEED_G)
#define CELL_INV_WHITE   MAKE_INV(INV_COL_WHITE,   INV_NEED_R | INV_NEED_G | INV_NEED_B)

/* Shot cell constants (bit 7 set, colour in bits 1-0) */
#define CELL_SHOT_RED    0x81u
#define CELL_SHOT_GREEN  0x82u
#define CELL_SHOT_BLUE   0x83u

/* Predicates */
#define CELL_IS_SHOT(c)     ((uint8_t)((c) & CELL_SHOT_FLAG))
#define CELL_IS_INV(c)      ((uint8_t)((c) != CELL_EMPTY && !CELL_IS_SHOT(c)))

/* Extract fields */
#define CELL_SHOT_COLOR(c)  ((uint8_t)((c) & 0x03u))       /* shot colour 1-3   */
#define CELL_INV_COL(c)     ((uint8_t)(((c) >> 4u) & 0x07u)) /* display colour 1-7 */
#define CELL_INV_MASK(c)    ((uint8_t)((c) & 0x07u))        /* remaining mask    */

/* -------------------------------------------------------------------------
 * The game board — 300 bytes in BIGRAM.
 * ---------------------------------------------------------------------- */
extern uint8_t strip[NUM_LEDS];

/* -------------------------------------------------------------------------
 * Rainbow animation constants
 * ---------------------------------------------------------------------- */
#define RAINBOW_FRAMES    90u
#define RAINBOW_FRAME_MS  33u

/* -------------------------------------------------------------------------
 * Functions
 * ---------------------------------------------------------------------- */

void Rainbow_Init(void);
void Rainbow_Step(void);

/*
 * WriteLEDs — render strip[] to the physical LED strip.
 *   show_cells   = 1 : render all cell colours.
 *   show_cells   = 0 : all off, except LED 0 if idle_blink_on = 1.
 *   idle_blink_on    : controls the idle blink LED when show_cells = 0.
 *   idle_red         : 1 = use red idle colour (hard mode), 0 = white.
 */
void WriteLEDs(uint8_t show_cells, uint8_t idle_blink_on, uint8_t idle_red);

/*
 * WriteAllRed — solid red (on=1) or off (on=0).
 */
void WriteAllRed(uint8_t on);

#endif /* LED_H */
