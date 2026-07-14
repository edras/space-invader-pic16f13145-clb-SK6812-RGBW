/*
 * buttons.h — button debounce and short/long press detection
 *
 * Each physical button is tracked by a button_t struct.
 * Call Debounce_Update() every main loop iteration with the raw GPIO value.
 *
 * Events fire on RELEASE so the full hold duration is known before
 * classifying the press:
 *
 *   pressed_event     = 1 after a SHORT press (held < 1 second)
 *   long_press_event  = 1 after a LONG  press (held >= 1 second)
 *
 * Both flags are cleared automatically at the start of the next call.
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

/* Minimum time (ms) the signal must be stable to count as a real edge */
#define DEBOUNCE_MS    20u

/* Hold duration (ms) that separates a short press from a long press */
#define LONG_PRESS_MS  1000u

/* -------------------------------------------------------------------------
 * Button state — one instance per physical button
 * ---------------------------------------------------------------------- */
typedef struct
{
    uint8_t  last_raw;        /* raw GPIO value from the previous call       */
    uint8_t  stable;          /* debounced (accepted) level                  */
    uint16_t change_ms;       /* timestamp of the last raw-level change      */
    uint16_t press_ms;        /* timestamp when a stable press was confirmed */
    uint8_t  long_fired;      /* 1 once the hold exceeds LONG_PRESS_MS       */
    uint8_t  pressed_event;   /* 1 for one call after a short press release  */
    uint8_t  long_press_event;/* 1 for one call after a long press release   */
} button_t;

/* -------------------------------------------------------------------------
 * Debounce_Update — call every main loop iteration.
 *
 *   b   : pointer to the button_t for this button
 *   raw : current raw GPIO reading (0 = not pressed, 1 = pressed)
 * ---------------------------------------------------------------------- */
void Debounce_Update(button_t *b, uint8_t raw);

#endif /* BUTTONS_H */
