#ifndef GAME_H
#define GAME_H

#include "gba.h"

// Mode 4 palette indices we will use throughout the game.
// These indices are initialized in main.c by writing PALETTE[].
// Map “CI_*” color indices to your Usenti startPal indices.
// Adjust these if you want different colors.
#undef CI_BLACK
#undef CI_WHITE
#undef CI_GRAY
#undef CI_YELLOW
#undef CI_RED
#undef CI_GREEN
#undef CI_CYAN
#undef CI_MAGENTA
#undef CI_BROWN

#define CI_BLACK    0   // startPal[0]  = 0x0000
#define CI_WHITE    48  // startPal[48] = 0x7FFF  (TRUE white)
#define CI_GRAY     36  // startPal[36] = 0x4631
#define CI_YELLOW   50  // startPal[50] = 0x171D  (yellow-ish)
#define CI_RED      18  // startPal[18] = 0x20BE  (red-ish)
#define CI_GREEN    68  // startPal[68] = 0x260E  (green-ish)
#define CI_CYAN     25  // startPal[25] = 0x5604  (cyan/teal)
#define CI_MAGENTA  18  // startPal[18] = 0x20BE  (closest available)
#define CI_BROWN    72  // startPal[72] = 0x2E71


#define PAUSE_CI_BLACK   0   // 0x0000
#define PAUSE_CI_WHITE   47  // 0x7FFF
#define PAUSE_CI_GRAY    48  // 0x39CE (nice gray)
#define PAUSE_CI_YELLOW  52  // 0x171D (yellow-ish)

// Game tuning
#define MAX_BULLETS   16   // object pool
#define MAX_ASTEROIDS 12   // object pool
#define MAX_STARS     24

#define PLAYER_W 8
#define PLAYER_H 8

// Gamestates
typedef enum {
    STATE_START,
    STATE_GAME,
    STATE_PAUSE,
    STATE_WIN,
    STATE_LOSE,
    STATE_SCOREBOARD
} GameState;

// Player struct
typedef struct {
    int x, y;
    int oldx, oldy;
    int w, h;
    int speed;

    int lives;
    int invulnTimer;   // brief invulnerability after getting hit
    int dashCooldown;  // B dash cooldown

    int bombs;         // L shoulder: “Nova Bomb” (extra mechanic)
} Player;

// Bullet Struct
typedef struct {
    int x, y;
    int oldx, oldy;
    int w, h;

    int dx, dy;
    int active;
} Bullet;

// Asteroid Struct
typedef struct {
    int x, y;
    int oldx, oldy;
    int size;

    int dx, dy;
    int active;

    int hp;           // larger asteroids can take more hits
    int isBomb;        // 1 if this is the rare "bomb" asteroid (power-up)
} Asteroid;

// Star Struct
typedef struct {
    int x, y;
    int oldx, oldy;

    int speed;
} Star;

// Public game API (main.c calls these)
void initGame(void);
void updateGame(void);
void drawGame(void);

// State transitions
void goToStart(void);
void goToGame(void);
void goToPause(void);
void goToWin(void);
void goToLose(void);
void goToScoreboardFromStart(void);
void goToScoreboardFromPause(void);

// State getter (so main can switch)
GameState getState(void);

#endif