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
 *   STATE_WIN      ──(animation done / PB1)──► STATE_IDLE
 *   STATE_GAME_OVER──(animation done / PB1)──► STATE_IDLE
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
    MODE_CLASSIC = 0,   /* destroy all 20 invaders to win */
    MODE_ENDLESS  = 1   /* new invader added every step; survive as long as possible */
} game_mode_t;

/* -------------------------------------------------------------------------
 * Game constants — timings and speed schedule
 * ---------------------------------------------------------------------- */
#define INVADER_START_POS   280u    /* index where the group spawns           */
#define GAME_TICK_INIT_MS   500u    /* invader advance period at game start   */
#define GAME_TICK_MIN_MS    100u    /* fastest the invaders can move          */
#define SPEED_UP_EVERY      10u     /* advance ticks between speed increases  */
#define SPEED_UP_STEP_MS    30u     /* ms removed from tick period each step  */
#define SHOT_STEP_MS        25u     /* ms between shot advances               */

/* Win animation */
#define RAINBOW_FRAMES      90u     /* number of frames in the win animation  */
#define RAINBOW_FRAME_MS    33u     /* ms between animation frames (~30 fps)  */

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
extern uint8_t      rainbow_offset;/* colour counter for the win animation   */

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

#endif /* GAME_H */
