/*
 * game.h — game logic for Space Invaders 1D
 *
 * Owns all game state variables and exposes the functions that main.c
 * calls to run the game loop.
 *
 * Game state machine:
 *
 *   STATE_IDLE  ──(short press PB1)──► STATE_PLAYING
 *   STATE_IDLE  ──(long  press PB1)──► STATE_IDLE   (mode toggled)
 *
 *   STATE_PLAYING ──(all invaders gone, Classic)──► STATE_WIN
 *   STATE_PLAYING ──(invader reaches index 0)──────► STATE_GAME_OVER
 *   STATE_PLAYING ──(long press PB1)───────────────► STATE_IDLE
 *
 *   STATE_WIN      ──(animation done)──► STATE_IDLE
 *   STATE_GAME_OVER──(animation done)──► STATE_IDLE
 *
 * Modes cycled by long press PB1 in IDLE:
 *   CLASSIC → ENDLESS → CLASSIC_HARD → ENDLESS_HARD → CLASSIC …
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Game state and mode
 * ---------------------------------------------------------------------- */
typedef enum {
    STATE_IDLE,
    STATE_PLAYING,
    STATE_WIN,
    STATE_GAME_OVER
} game_state_t;

typedef enum {
    MODE_CLASSIC      = 0,  /* RGB invaders, destroy all 20 to win          */
    MODE_ENDLESS      = 1,  /* RGB invaders, new one every step              */
    MODE_CLASSIC_HARD = 2,  /* CMYW invaders (multi-hit), destroy all to win */
    MODE_ENDLESS_HARD = 3   /* CMYW invaders, new one every step             */
} game_mode_t;

/* Convenience predicates */
#define MODE_IS_HARD(m)    ((m) >= MODE_CLASSIC_HARD)
#define MODE_IS_ENDLESS(m) ((m) == MODE_ENDLESS || (m) == MODE_ENDLESS_HARD)

/* -------------------------------------------------------------------------
 * Game constants — timings and speed schedule
 * ---------------------------------------------------------------------- */
#define INVADER_START_POS   280u    /* index where the group spawns           */
#define GAME_TICK_INIT_MS   500u    /* invader advance period at game start   */
#define GAME_TICK_MIN_MS    100u    /* fastest the invaders can move          */
#define SPEED_UP_EVERY      10u     /* advance ticks between speed increases  */
#define SPEED_UP_STEP_MS    30u     /* ms removed from tick period each step  */
#define SHOT_STEP_MS        25u     /* ms between shot advances               */

/* Game-over animation */
#define GAMEOVER_FLASHES    6u      /* 3 on + 3 off = 3 full red blinks       */
#define GAMEOVER_BLINK_MS   200u    /* ms per half-blink                      */

/* -------------------------------------------------------------------------
 * Shared game state — read by main.c to drive the state machine and
 * the win/lose animations.
 * ---------------------------------------------------------------------- */
extern game_state_t game_state;
extern game_mode_t  game_mode;
extern uint8_t      anim_count;    /* animation frames remaining             */
extern uint16_t     anim_ms;       /* timestamp of last animation tick       */

/* -------------------------------------------------------------------------
 * Functions
 * ---------------------------------------------------------------------- */

/*
 * Game_Init — reset the strip and all game variables, seed the invaders,
 * and set state to STATE_PLAYING.
 */
void Game_Init(void);

/*
 * Fire_Shot — place a shot cell at index 0 (player position) if it is empty.
 * shot_cell must be one of CELL_SHOT_RED / GREEN / BLUE.
 */
void Fire_Shot(uint8_t shot_cell);

/*
 * Game_Update — advance shots and invaders according to their independent
 * tick timers.  Handles speed-up schedule and win/lose detection.
 * Call this every iteration of the main loop while in STATE_PLAYING.
 */
void Game_Update(void);

/*
 * Gameover_Init — called once when the game transitions to STATE_GAME_OVER.
 * Sets up the animation counters and sends the first red frame.
 */
void Gameover_Init(void);

/*
 * Gameover_Step — called every iteration of the main loop while in
 * STATE_GAME_OVER.  Blinks the strip red GAMEOVER_FLASHES times, then
 * transitions to STATE_IDLE automatically.
 */
void Gameover_Step(void);

#endif /* GAME_H */
