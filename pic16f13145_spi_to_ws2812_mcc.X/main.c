/*
 * main.c — Space Invaders 1D
 * PIC16F13145 + SK6812 GRBW 300-LED strip
 *
 * This file contains only:
 *   - Hardware initialisation
 *   - The 1 ms timer interrupt service routine (ISR)
 *   - The main loop state machine (IDLE / PLAYING / WIN / GAME OVER)
 *
 * All game logic lives in game.c / game.h.
 * All LED rendering lives in led.c  / led.h.
 * Button debounce lives in  buttons.c / buttons.h.
 *
 * State machine:
 *
 *   IDLE ──(short press PB1)──► PLAYING
 *   IDLE ──(long  press PB1)──► IDLE  (mode toggled: Classic ↔ Endless)
 *
 *   PLAYING ──(all invaders gone, Classic)──► WIN
 *   PLAYING ──(invader reaches index 0)─────► GAME_OVER
 *   PLAYING ──(long  press PB1)─────────────► IDLE  (abort)
 *
 *   WIN      ──(animation ends / PB1 skip)──► IDLE
 *   GAME_OVER──(animation ends / PB1 skip)──► IDLE
 */

#include "mcc_generated_files/system/system.h"
#include "led.h"
#include "buttons.h"
#include "game.h"

/* -------------------------------------------------------------------------
 * Software millisecond timer.
 * Incremented by Timer_1ms_Callback() every 1 ms.
 * Declared volatile because the ISR writes it and the main loop reads it.
 * ---------------------------------------------------------------------- */
volatile uint16_t ms_tick;

/* -------------------------------------------------------------------------
 * Idle blink state
 * ---------------------------------------------------------------------- */
#define BLINK_CLASSIC_MS  500u   /* half-period for Classic mode (1 s total) */
#define BLINK_ENDLESS_MS   75u   /* half-period for Endless mode (150 ms)    */

static uint16_t blink_ms;
static uint8_t  blink_on;

/* -------------------------------------------------------------------------
 * Four buttons — each gets its own button_t instance
 * ---------------------------------------------------------------------- */
static button_t btn_pb1, btn_pb2, btn_pb3, btn_pb4;

/* -------------------------------------------------------------------------
 * Button_Init — zero-initialise a button_t (all fields to 0 = released)
 * ---------------------------------------------------------------------- */
static void Button_Init(button_t *b)
{
    b->last_raw         = 0;
    b->stable           = 0;
    b->change_ms        = 0;
    b->press_ms         = 0;
    b->long_fired       = 0;
    b->pressed_event    = 0;
    b->long_press_event = 0;
}

/* -------------------------------------------------------------------------
 * Timer_1ms_Callback — called by the TMR0 ISR every 1 ms.
 * Keeps ms_tick as the global time reference for everything in the project.
 * ---------------------------------------------------------------------- */
void Timer_1ms_Callback(void)
{
    ms_tick++;
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(void)
{
    uint16_t i;

    /* --- Hardware init ------------------------------------------------- */
    SYSTEM_Initialize();
    TMR0_PeriodMatchCallbackRegister(Timer_1ms_Callback);
    Timer0_Start();
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();

    /* Enable the CLB output that drives the LED strip */
    CLBSWINLbits.CLBSWIN0 = 1;
    __delay_ms(1);

    /* --- Software init ------------------------------------------------- */
    Button_Init(&btn_pb1);
    Button_Init(&btn_pb2);
    Button_Init(&btn_pb3);
    Button_Init(&btn_pb4);

    for (i = 0; i < NUM_LEDS; i++)
        strip[i] = CELL_EMPTY;

    game_state = STATE_IDLE;
    game_mode  = MODE_CLASSIC;
    ms_tick    = 0;
    blink_ms   = 0;
    blink_on   = 0;
    anim_count = 0;
    anim_ms    = 0;

    WriteLEDs(0, 0);

    /* ===================================================================
     * Main loop
     * ================================================================= */
    while (1)
    {
        /* Read and debounce all buttons every iteration */
        Debounce_Update(&btn_pb1, (uint8_t)PB1_GetValue());
        Debounce_Update(&btn_pb2, (uint8_t)PB2_GetValue());
        Debounce_Update(&btn_pb3, (uint8_t)PB3_GetValue());
        Debounce_Update(&btn_pb4, (uint8_t)PB4_GetValue());

        /* -----------------------------------------------------------------
         * STATE: IDLE
         *   Short press PB1 → start the game in the selected mode.
         *   Long  press PB1 → toggle between Classic and Endless mode.
         *   Otherwise       → blink LED 0 at the rate for the current mode.
         * --------------------------------------------------------------- */
        if (game_state == STATE_IDLE)
        {
            if (btn_pb1.pressed_event)
            {
                Game_Init();
            }
            else if (btn_pb1.long_press_event)
            {
                game_mode = (game_mode == MODE_CLASSIC)
                            ? MODE_ENDLESS : MODE_CLASSIC;
                blink_ms  = ms_tick;
                blink_on  = 1;
                WriteLEDs(0, blink_on);
            }
            else
            {
                uint16_t period = (game_mode == MODE_CLASSIC)
                                  ? BLINK_CLASSIC_MS : BLINK_ENDLESS_MS;
                if ((uint16_t)(ms_tick - blink_ms) >= period)
                {
                    blink_ms = ms_tick;
                    blink_on = !blink_on;
                    WriteLEDs(0, blink_on);
                }
            }
        }

        /* -----------------------------------------------------------------
         * STATE: PLAYING
         *   PB2 / PB3 / PB4 → fire a red / green / blue shot.
         *   Long press PB1   → abort and return to IDLE.
         *   Game_Update()    → advance shots and invaders on their timers.
         * --------------------------------------------------------------- */
        else if (game_state == STATE_PLAYING)
        {
            if (btn_pb1.long_press_event)
            {
                /* Abort — clear the strip and go back to idle */
                for (i = 0; i < NUM_LEDS; i++)
                    strip[i] = CELL_EMPTY;
                game_state = STATE_IDLE;
                blink_ms   = ms_tick;
                blink_on   = 0;
                WriteLEDs(0, 0);
            }

            if (btn_pb2.pressed_event) Fire_Shot(CELL_SHOT_RED);
            if (btn_pb3.pressed_event) Fire_Shot(CELL_SHOT_GREEN);
            if (btn_pb4.pressed_event) Fire_Shot(CELL_SHOT_BLUE);

            Game_Update();

            if (game_state == STATE_GAME_OVER)
            {
                /* Start the game-over red-blink animation */
                anim_count = GAMEOVER_FLASHES;
                blink_on   = 1;
                anim_ms    = ms_tick;
                LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh();
                WriteAllRed(1);
            }

            if (game_state == STATE_PLAYING)
                WriteLEDs(1, 0);
        }

        /* -----------------------------------------------------------------
         * STATE: WIN — rainbow wave animation
         *   Each tick: shift all strip cells one step toward the far end,
         *   feed the next R/G/B colour into index 0.
         *   WriteLEDs(1, 0) renders strip[] just like during gameplay.
         * --------------------------------------------------------------- */
        else if (game_state == STATE_WIN)
        {
            if ((uint16_t)(ms_tick - anim_ms) >= RAINBOW_FRAME_MS)
            {
                anim_ms = ms_tick;

                /* Shift the rainbow wave toward the far end */
                for (i = NUM_LEDS - 1u; i > 0u; i--)
                    strip[i] = strip[i - 1u];

                /* Feed the next colour into the player end */
                rainbow_offset++;
                if (rainbow_offset > 3u) rainbow_offset = 1u;
                strip[0] = rainbow_offset;

                WriteLEDs(1, 0);

                anim_count--;
                if (anim_count == 0u)
                {
                    for (i = 0; i < NUM_LEDS; i++)
                        strip[i] = CELL_EMPTY;
                    game_state = STATE_IDLE;
                    blink_ms   = ms_tick;
                    blink_on   = 0;
                    WriteLEDs(0, 0);
                }
            }

            /* PB1 skips the animation */
            if (btn_pb1.pressed_event)
            {
                for (i = 0; i < NUM_LEDS; i++)
                    strip[i] = CELL_EMPTY;
                game_state = STATE_IDLE;
                blink_ms   = ms_tick;
                blink_on   = 0;
                WriteLEDs(0, 0);
            }
        }

        /* -----------------------------------------------------------------
         * STATE: GAME OVER — blink the whole strip red, then return to IDLE
         *   PB1 skips the animation immediately.
         * --------------------------------------------------------------- */
        else if (game_state == STATE_GAME_OVER)
        {
            if (btn_pb1.pressed_event)
            {
                LED3_SetLow(); LED4_SetLow(); LED5_SetLow();
                WriteAllRed(0);
                game_state = STATE_IDLE;
                blink_ms   = ms_tick;
                blink_on   = 0;
            }
            else if ((uint16_t)(ms_tick - anim_ms) >= GAMEOVER_BLINK_MS)
            {
                anim_ms  = ms_tick;
                blink_on = !blink_on;
                WriteAllRed(blink_on);

                if (blink_on) { LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh(); }
                else          { LED3_SetLow();  LED4_SetLow();  LED5_SetLow();  }

                if (anim_count > 0u)
                    anim_count--;

                if (anim_count == 0u)
                {
                    LED3_SetLow(); LED4_SetLow(); LED5_SetLow();
                    game_state = STATE_IDLE;
                    blink_ms   = ms_tick;
                    blink_on   = 0;
                    WriteLEDs(0, 0);
                }
            }
        }
    }
}
