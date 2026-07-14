/*
 * buttons.c — button debounce and short/long press detection
 *
 * How it works:
 *
 *  1. RAW CHANGE: every time the raw GPIO value flips, record the timestamp.
 *
 *  2. DEBOUNCE: only accept the new level once it has been stable for
 *     DEBOUNCE_MS (20 ms).  This filters out contact bounce on release/press.
 *
 *  3. PRESS TIMING: when a stable press edge is confirmed, record that
 *     timestamp as press_ms.
 *
 *  4. LONG-PRESS DETECTION: on every call while the button is held, check
 *     if (now - press_ms) >= LONG_PRESS_MS.  Set long_fired the first time
 *     the threshold is crossed.
 *
 *  5. RELEASE CLASSIFICATION: on a stable release edge, generate either
 *     pressed_event (short) or long_press_event (long) depending on
 *     whether long_fired was set.
 *
 *  Events fire on release so the full hold duration is known before
 *  the decision is made.
 */

#include "buttons.h"
#include "mcc_generated_files/system/system.h"

/* ms_tick is owned by main.c; access it through the extern declared there */
extern volatile uint16_t ms_tick;

/* -------------------------------------------------------------------------
 * Local helpers — same atomic read and elapsed helpers as in main.c
 * ---------------------------------------------------------------------- */
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
 * Debounce_Update — call every main loop iteration
 * ---------------------------------------------------------------------- */
void Debounce_Update(button_t *b, uint8_t raw)
{
    /* Clear events from the previous call */
    b->pressed_event    = 0;
    b->long_press_event = 0;

    /* --- Step 1 & 2: detect a stable edge ----------------------------- */
    if (raw != b->last_raw)
    {
        /* Raw level just changed — start the debounce timer */
        b->last_raw  = raw;
        b->change_ms = Ms_Now();
    }
    else if (raw != b->stable && Elapsed(b->change_ms) >= DEBOUNCE_MS)
    {
        /* Level has been stable long enough — accept it */
        uint8_t prev = b->stable;
        b->stable    = raw;

        if (prev == 0u && raw == 1u)
        {
            /* --- Step 3: confirmed press — start hold timer ----------- */
            b->press_ms   = Ms_Now();
            b->long_fired = 0;
        }
        else if (prev == 1u && raw == 0u)
        {
            /* --- Step 5: confirmed release — classify the press ------- */
            if (b->long_fired)
                b->long_press_event = 1;
            else
                b->pressed_event    = 1;
        }
    }

    /* --- Step 4: long-press threshold check (runs while held) --------- */
    if (b->stable == 1u && !b->long_fired &&
        Elapsed(b->press_ms) >= LONG_PRESS_MS)
    {
        b->long_fired = 1;
    }
}
