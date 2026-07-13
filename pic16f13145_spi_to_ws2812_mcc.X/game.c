/*
 * game.c — game logic for Space Invaders 1D
 *
 * Contains:
 *   Game_Init()       reset everything and start a new game
 *   Fire_Shot()       player fires a coloured shot from index 0
 *   Shots_Run()       advance all in-flight shots one step per tick
 *   Invaders_Advance() shift the invader group one step toward the player
 *   All_Invaders_Gone() check if the player has cleared all invaders
 *   Game_Update()     master tick — calls shots and invaders on their timers
 */

#include "game.h"
#include "led.h"
#include "mcc_generated_files/system/system.h"

/* ms_tick is owned by main.c */
extern volatile uint16_t ms_tick;

/* -------------------------------------------------------------------------
 * Game state variables — declared extern in game.h so main.c can read them
 * ---------------------------------------------------------------------- */
game_state_t game_state;
game_mode_t  game_mode;
uint8_t      anim_count;
uint16_t     anim_ms;

/* Private game variables */
static uint16_t invader_head;    /* index of the first (closest) invader     */
static uint16_t game_tick_ms;    /* current invader advance period (ms)      */
static uint16_t tick_counter;    /* counts advance ticks for speed-up        */
static uint16_t last_invader_ms; /* timestamp of last invader advance        */
static uint16_t last_shot_ms;    /* timestamp of last shot advance           */
static uint16_t rng_state = 0xACE1u; /* LCG PRNG seed                       */

/* -------------------------------------------------------------------------
 * Timing helpers
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
 * Rng_NextColor — LCG pseudo-random number generator.
 * Returns 1 (RED), 2 (GREEN), or 3 (BLUE) with roughly equal probability.
 * ---------------------------------------------------------------------- */
static uint8_t Rng_NextColor(void)
{
    rng_state = rng_state * 25173u + 13849u;
    return (uint8_t)((rng_state >> 8u) % 3u) + 1u;
}

/* -------------------------------------------------------------------------
 * Game_Init — clear the strip, place 20 random invaders at the far end,
 * reset all counters, and start the game.
 * ---------------------------------------------------------------------- */
void Game_Init(void)
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
 * Fire_Shot — place a shot at index 0 (player position) if that cell is
 * empty.  Only one shot can occupy the player cell at a time.
 * ---------------------------------------------------------------------- */
void Fire_Shot(uint8_t shot_cell)
{
    if (strip[0] == CELL_EMPTY)
        strip[0] = shot_cell;
}

/* -------------------------------------------------------------------------
 * All_Invaders_Gone — scan the full strip for any surviving invader.
 * Must scan all 300 cells because Shot_Miss() can append new invaders
 * beyond invader_head.
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
 * Shot_Miss — penalty for a missed shot (overshot or wrong colour).
 * Adds a new invader at the tail if there is a free cell there, and
 * speeds up the invader tick by one step (both modes).
 * ---------------------------------------------------------------------- */
static void Shot_Miss(void)
{
    if (strip[NUM_LEDS - 1u] == CELL_EMPTY)
        strip[NUM_LEDS - 1u] = Rng_NextColor();

    if (game_tick_ms > GAME_TICK_MIN_MS + SPEED_UP_STEP_MS)
        game_tick_ms -= SPEED_UP_STEP_MS;
    else
        game_tick_ms = GAME_TICK_MIN_MS;
}

/* -------------------------------------------------------------------------
 * Shots_Run — advance every in-flight shot one step toward the invaders.
 *
 * Walk the strip from high index to low so each shot moves at most one
 * position per call (prevents a shot from skipping over multiple cells).
 *
 * Outcomes:
 *   Shot reaches NUM_LEDS-1 (overshot)  : miss penalty, shot consumed.
 *   Colour match, Classic mode          : invader destroyed, group compacted.
 *   Colour match, Endless mode          : invader cleared in place.
 *   Colour mismatch                     : miss penalty, shot consumed.
 * ---------------------------------------------------------------------- */
static void Shots_Run(void)
{
    uint16_t i;
    uint8_t  cell, next;

    /* Clear any shot that reached the last cell (overshot all invaders) */
    if (CELL_IS_SHOT(strip[NUM_LEDS - 1u]))
    {
        strip[NUM_LEDS - 1u] = CELL_EMPTY;
        Shot_Miss();
    }

    for (i = NUM_LEDS - 2u; ; i--)
    {
        cell = strip[i];
        if (CELL_IS_SHOT(cell))
        {
            next = strip[i + 1u];

            if (next == CELL_EMPTY)
            {
                /* Empty cell ahead — move the shot forward */
                strip[i + 1u] = cell;
                strip[i]      = CELL_EMPTY;
            }
            else if (CELL_IS_INV(next))
            {
                if (CELL_COLOR(next) == CELL_COLOR(cell))
                {
                    /* Colour match — destroy the invader */
                    if (game_mode == MODE_CLASSIC)
                    {
                        uint16_t j;
                        for (j = i + 1u; j < NUM_LEDS - 1u; j++)
                            strip[j] = strip[j + 1u];
                        strip[NUM_LEDS - 1u] = CELL_EMPTY;
                    }
                    else
                    {
                        /* Endless: clear in place; Invaders_Advance refills */
                        strip[i + 1u] = CELL_EMPTY;
                    }
                }
                else
                {
                    /* Wrong colour — miss penalty */
                    Shot_Miss();
                }
                strip[i] = CELL_EMPTY;  /* shot consumed either way */
            }
            /* else: next cell is another shot — leave both in place */
        }

        if (i == 0u) break;
    }
}

/* -------------------------------------------------------------------------
 * Invaders_Advance — shift the entire invader group one step toward the
 * player (index 0).
 *
 * Collision during advance: if an invader is about to step into a cell
 * that already holds a shot, the collision is resolved here.  This catches
 * cases where the invaders move fast enough that Shots_Run() hasn't had a
 * chance to reach the contact point yet.
 *
 * After the shift, the trailing cell (NUM_LEDS-1) is:
 *   Endless mode : filled with a new random-colour invader.
 *   Classic mode : left empty (group shrinks from the tail as you score).
 * ---------------------------------------------------------------------- */
static void Invaders_Advance(void)
{
    uint16_t i;
    uint8_t  inv, dst;

    for (i = (invader_head > 0u ? invader_head : 1u); i < NUM_LEDS; i++)
    {
        inv = strip[i];
        if (!CELL_IS_INV(inv))
            continue;

        dst = strip[i - 1u];   /* the cell this invader is moving into */

        if (CELL_IS_SHOT(dst))
        {
            /* Shot waiting in the destination — resolve collision now */
            strip[i] = CELL_EMPTY;
            if (CELL_COLOR(dst) == CELL_COLOR(inv))
                strip[i - 1u] = CELL_EMPTY;  /* colour match: both gone   */
            else
                strip[i - 1u] = inv;          /* mismatch: invader moves through */
        }
        else
        {
            /* Normal step */
            strip[i - 1u] = inv;
            strip[i]      = CELL_EMPTY;
        }
    }

    strip[NUM_LEDS - 1u] = (game_mode == MODE_ENDLESS)
                           ? Rng_NextColor()
                           : CELL_EMPTY;

    if (invader_head > 0u)
        invader_head--;

    if (CELL_IS_INV(strip[0]))
        game_state = STATE_GAME_OVER;
}

/* -------------------------------------------------------------------------
 * Gameover_Init — set up the red-blink animation counters and show the
 * first red frame immediately.
 * ---------------------------------------------------------------------- */
void Gameover_Init(void)
{
    anim_count = GAMEOVER_FLASHES;
    anim_ms    = Ms_Now();
    LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh();
    WriteAllRed(1);
}

/* -------------------------------------------------------------------------
 * Gameover_Step — blink the strip red GAMEOVER_FLASHES times, then
 * transition to STATE_IDLE.
 * ---------------------------------------------------------------------- */
void Gameover_Step(void)
{
    if ((uint16_t)(ms_tick - anim_ms) < GAMEOVER_BLINK_MS)
        return;

    anim_ms  = Ms_Now();
    anim_count--;
    WriteAllRed(anim_count & 1u);   /* odd counts = on, even = off */

    if (anim_count & 1u) { LED3_SetHigh(); LED4_SetHigh(); LED5_SetHigh(); }
    else                 { LED3_SetLow();  LED4_SetLow();  LED5_SetLow();  }

    if (anim_count == 0u)
    {
        LED3_SetLow(); LED4_SetLow(); LED5_SetLow();
        game_state = STATE_IDLE;
    }
}

/* -------------------------------------------------------------------------
 * Game_Update — called every main loop iteration while STATE_PLAYING.
 *
 * Shots and invaders run on independent timers (last_shot_ms,
 * last_invader_ms) so their speeds are decoupled.  The invader speed
 * increases every SPEED_UP_EVERY advances until GAME_TICK_MIN_MS.
 * ---------------------------------------------------------------------- */
void Game_Update(void)
{
    uint16_t now = Ms_Now();

    /* --- Shot tick ---------------------------------------------------- */
    if ((uint16_t)(now - last_shot_ms) >= SHOT_STEP_MS)
    {
        last_shot_ms += SHOT_STEP_MS;
        Shots_Run();

        /* Win check: only in Classic mode, checked after shots resolve */
        if (game_mode == MODE_CLASSIC && All_Invaders_Gone())
        {
            game_state = STATE_WIN;   /* Rainbow_Init() called by main.c */
            return;
        }
    }

    /* --- Invader tick ------------------------------------------------- */
    if ((uint16_t)(now - last_invader_ms) >= game_tick_ms)
    {
        last_invader_ms += game_tick_ms;
        Invaders_Advance();

        if (game_state == STATE_GAME_OVER)
            return;

        /* Speed-up schedule */
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
