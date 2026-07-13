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
 * Rng_NextInvader — returns a fresh random invader cell for the current mode.
 *
 * Easy modes: one of RED / GREEN / BLUE (one shot to kill).
 * Hard modes: one of CYAN / MAGENTA / YELLOW / WHITE (2-3 shots to kill).
 * ---------------------------------------------------------------------- */
static uint8_t Rng_NextInvader(void)
{
    rng_state = rng_state * 25173u + 13849u;
    if (MODE_IS_HARD(game_mode))
    {
        switch ((rng_state >> 8u) % 7u)
        {
            case 0u: return CELL_INV_RED;
            case 1u: return CELL_INV_GREEN;
            case 2u: return CELL_INV_BLUE;
            case 3u: return CELL_INV_CYAN;
            case 4u: return CELL_INV_MAGENTA;
            case 5u: return CELL_INV_YELLOW;
            default: return CELL_INV_WHITE;
        }
    }
    else
    {
        switch ((rng_state >> 8u) % 3u)
        {
            case 0u: return CELL_INV_RED;
            case 1u: return CELL_INV_GREEN;
            default: return CELL_INV_BLUE;
        }
    }
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
        strip[i] = Rng_NextInvader();

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
 * Adds a new invader at the tail if space is available, and speeds up.
 * ---------------------------------------------------------------------- */
static void Shot_Miss(void)
{
    if (strip[NUM_LEDS - 1u] == CELL_EMPTY)
        strip[NUM_LEDS - 1u] = Rng_NextInvader();

    if (game_tick_ms > GAME_TICK_MIN_MS + SPEED_UP_STEP_MS)
        game_tick_ms -= SPEED_UP_STEP_MS;
    else
        game_tick_ms = GAME_TICK_MIN_MS;
}

/* -------------------------------------------------------------------------
 * Invader_Destroy — remove invader at index pos from the strip.
 * Classic modes: compact the group (shift everything down, clear tail).
 * Endless modes: clear in place; Invaders_Advance will refill the tail.
 * ---------------------------------------------------------------------- */
static void Invader_Destroy(uint16_t pos)
{
    if (!MODE_IS_ENDLESS(game_mode))
    {
        uint16_t j;
        for (j = pos; j < NUM_LEDS - 1u; j++)
            strip[j] = strip[j + 1u];
        strip[NUM_LEDS - 1u] = CELL_EMPTY;
    }
    else
    {
        strip[pos] = CELL_EMPTY;
    }
}

/* -------------------------------------------------------------------------
 * Shot_Hit_Invader — apply one shot to an invader cell.
 *
 * Easy mode: any colour match destroys immediately.
 * Hard mode: the shot clears its corresponding bit from the invader's
 *            remaining-shots mask.  When the mask reaches 0 the invader
 *            is destroyed.  The display colour updates to reflect what
 *            shots are still needed:
 *              R only left  → RED
 *              G only left  → GREEN
 *              B only left  → BLUE
 *              R+G left     → YELLOW
 *              R+B left     → MAGENTA
 *              G+B left     → CYAN
 *              R+G+B left   → WHITE  (shouldn't happen post-first-hit)
 *
 * Returns 1 if the invader was destroyed, 0 if it survived.
 * inv_pos: index of the invader cell in strip[].
 * shot_col: CELL_SHOT_COLOR() of the incoming shot (1=R, 2=G, 3=B).
 * ---------------------------------------------------------------------- */
static uint8_t Shot_Hit_Invader(uint16_t inv_pos, uint8_t shot_col)
{
    uint8_t inv  = strip[inv_pos];
    uint8_t mask = CELL_INV_MASK(inv);
    uint8_t need_bit;

    /* Map shot colour to need bit */
    if      (shot_col == 1u) need_bit = INV_NEED_R;
    else if (shot_col == 2u) need_bit = INV_NEED_G;
    else                     need_bit = INV_NEED_B;

    /* Miss: this colour is not needed (wrong shot for this invader) */
    if (!(mask & need_bit))
        return 0u;   /* caller should apply miss penalty */

    /* Clear the bit */
    mask &= (uint8_t)(~need_bit);

    if (mask == 0u)
    {
        /* All required shots delivered — destroy the invader */
        Invader_Destroy(inv_pos);
        return 1u;
    }

    /* Update the cell with the new mask and recalculate display colour */
    {
        uint8_t new_col;
        switch (mask)
        {
            case INV_NEED_R:                       new_col = INV_COL_RED;     break;
            case INV_NEED_G:                       new_col = INV_COL_GREEN;   break;
            case INV_NEED_B:                       new_col = INV_COL_BLUE;    break;
            case INV_NEED_R | INV_NEED_G:          new_col = INV_COL_YELLOW;  break;
            case INV_NEED_R | INV_NEED_B:          new_col = INV_COL_MAGENTA; break;
            case INV_NEED_G | INV_NEED_B:          new_col = INV_COL_CYAN;    break;
            default:                               new_col = INV_COL_WHITE;   break;
        }
        strip[inv_pos] = MAKE_INV(new_col, mask);
    }
    return 0u;   /* survived but damaged */
}

/* -------------------------------------------------------------------------
 * Shots_Run — advance every in-flight shot one step toward the invaders.
 *
 * Walk from high index to low so each shot moves at most one step per call.
 *
 * Outcomes:
 *   Overshot (reached NUM_LEDS-1)  : miss penalty, shot consumed.
 *   Shot colour matches invader need: apply hit; destroy if mask = 0.
 *   Shot colour not in invader mask : miss penalty, shot consumed.
 * ---------------------------------------------------------------------- */
static void Shots_Run(void)
{
    uint16_t i;
    uint8_t  cell, next;

    /* Clear any shot that overshot all invaders */
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
                strip[i + 1u] = cell;
                strip[i]      = CELL_EMPTY;
            }
            else if (CELL_IS_INV(next))
            {
                uint8_t destroyed = Shot_Hit_Invader(i + 1u, CELL_SHOT_COLOR(cell));
                if (!destroyed)
                    Shot_Miss();   /* wrong colour or partial hit with no match */
                strip[i] = CELL_EMPTY;   /* shot always consumed on contact */
            }
            /* else: adjacent shot — leave both in place */
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
            /* Shot waiting in the destination — resolve collision now.
             * Use Shot_Hit_Invader so hard-mode multi-hit logic applies. */
            uint8_t destroyed = Shot_Hit_Invader(i, CELL_SHOT_COLOR(dst));
            strip[i - 1u] = CELL_EMPTY;   /* shot consumed */
            if (!destroyed)
            {
                /* Invader survived the hit — move it into the (now empty) dst */
                strip[i - 1u] = strip[i];
                Shot_Miss();
            }
            strip[i] = CELL_EMPTY;
        }
        else
        {
            /* Normal step */
            strip[i - 1u] = inv;
            strip[i]      = CELL_EMPTY;
        }
    }

    strip[NUM_LEDS - 1u] = MODE_IS_ENDLESS(game_mode)
                           ? Rng_NextInvader()
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

        /* Win check: only in Classic modes, checked after shots resolve */
        if (!MODE_IS_ENDLESS(game_mode) && All_Invaders_Gone())
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
