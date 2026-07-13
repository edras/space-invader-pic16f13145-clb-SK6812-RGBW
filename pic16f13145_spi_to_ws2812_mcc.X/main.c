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
 *   WIN      ──(animation ends)──► IDLE
 *   GAME_OVER──(animation ends)──► IDLE
 */

#include "mcc_generated_files/system/system.h"
#include "led.h"
#include "buttons.h"
#include "game.h"

/* -------------------------------------------------------------------------
 * Software millisecond timer — incremented every 1 ms by the ISR.
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
 * Four buttons
 * ---------------------------------------------------------------------- */
static button_t btn_pb1, btn_pb2, btn_pb3, btn_pb4;

static void Button_Init(button_t *b)
{
    b->last_raw = 0; b->stable = 0; b->change_ms = 0;
    b->press_ms = 0; b->long_fired = 0;
    b->pressed_event = 0; b->long_press_event = 0;
}

/* -------------------------------------------------------------------------
 * Timer_1ms_Callback — TMR0 ISR, called every 1 ms
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

    WriteLEDs(0, 0);

    /* ===================================================================
     * Main loop
     * ================================================================= */
    while (1)
    {
        Debounce_Update(&btn_pb1, (uint8_t)PB1_GetValue());
        Debounce_Update(&btn_pb2, (uint8_t)PB2_GetValue());
        Debounce_Update(&btn_pb3, (uint8_t)PB3_GetValue());
        Debounce_Update(&btn_pb4, (uint8_t)PB4_GetValue());

        /* -----------------------------------------------------------------
         * STATE: IDLE
         *   Short press PB1 → start the game.
         *   Long  press PB1 → toggle Classic / Endless mode.
         *   Otherwise       → blink LED 0 at the mode-specific rate.
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
         *   PB2/PB3/PB4  → fire red/green/blue shot; light matching LED.
         *   Long press PB1 → abort and return to IDLE.
         *   Game_Update()  → advance shots and invaders.
         *   On transition to WIN      → Rainbow_Init().
         *   On transition to GAME_OVER→ Gameover_Init().
         * --------------------------------------------------------------- */
        else if (game_state == STATE_PLAYING)
        {
            if (btn_pb1.long_press_event)
            {
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

            if (btn_pb2.stable) LED3_SetHigh(); else LED3_SetLow();
            if (btn_pb3.stable) LED4_SetHigh(); else LED4_SetLow();
            if (btn_pb4.stable) LED5_SetHigh(); else LED5_SetLow();

            Game_Update();

            if      (game_state == STATE_WIN)       Rainbow_Init();
            else if (game_state == STATE_GAME_OVER)  Gameover_Init();
            else                                     WriteLEDs(1, 0);
        }

        /* -----------------------------------------------------------------
         * STATE: WIN — rainbow wave, auto-returns to IDLE when done.
         * --------------------------------------------------------------- */
        else if (game_state == STATE_WIN)
        {
            Rainbow_Step();

            if (game_state == STATE_IDLE)
            {
                blink_ms = ms_tick;
                blink_on = 0;
                WriteLEDs(0, 0);
            }
        }

        /* -----------------------------------------------------------------
         * STATE: GAME OVER — red blink, auto-returns to IDLE when done.
         * --------------------------------------------------------------- */
        else if (game_state == STATE_GAME_OVER)
        {
            Gameover_Step();

            if (game_state == STATE_IDLE)
            {
                blink_ms = ms_tick;
                blink_on = 0;
                WriteLEDs(0, 0);
            }
        }
    }
}
