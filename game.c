#include "game.h"
#include "mode4.h"
#include "sfx.h"
#include "start.h"
#include "pause.h"
#include <stdio.h>
//#include "rocket.h"
//#include "powered_rocket.h"

// Module-level game state
static GameState state;

static Player player;

static Bullet bullets[MAX_BULLETS];       // object pool
static Asteroid asteroids[MAX_ASTEROIDS]; // object pool
static Star stars[MAX_STARS];             // background motion (array + polish)

// Game counters
static int score;
static int targetScore;
static int frameCount;
static int spawnTimer;
static int asteroidSpawnCount; // counts asteroid spawns for rare bomb-asteroid logic
static int screenShakeTimer;

// Forward declarations
static void initStars(void);
static void updateStars(void);
static void drawStars(void);

static void initPlayer(void);
static void updatePlayer(void);
static void drawPlayer(void);

static void initPools(void);
static void fireBullet(void);
static void updateBullets(void);
static void drawBullets(void);

static void spawnAsteroid(void);
static void updateAsteroids(void);
static void drawAsteroids(void);

static void handleCollisions(void);
static void useBomb(void);

static inline void safeSetPixel4(int x, int y, u8 colorIndex);

// Debug cheat latch (makes combos reliable even with weird timing)
static u16 cheatLatch = 0;
static int cheatFlashTimer = 0; // quick visual confirmation

// Prevents re-drawing heavy text screens every frame (reduces flicker).
static GameState lastRenderedState = -1;

// Mode 4 page-flip bookkeeping
static int fullRedrawRequested = 1;
static int hudDirty = 1;

// High score (persists across restarts within the same run)
static int highScore = 0;

// Scoreboard behavior
static GameState scoreboardReturnState = STATE_START; // where to return when leaving scoreboard
static int scoreboardShowCurrentScore = 0;            // only show current score when opened from PAUSE

// videoBuffer should already exist in mode4.c/mode4.h
extern u16* videoBuffer;

static void renderStaticToBothBuffers(void (*drawFn)(void)) {
    u16* saved = videoBuffer;

    // Draw into page 0
    videoBuffer = FRONTBUFFER;
    drawFn();

    // Draw into page 1
    videoBuffer = BACKBUFFER;
    drawFn();

    // Restore whatever back buffer the engine thinks we’re on
    videoBuffer = saved;
}

static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void cheatFlash(void) {
    cheatFlashTimer = 10; // 10 frames
}

// Clear queue (kept as-is: update never draws)
#define MAX_CLEARS 64

typedef struct {
    int x, y, w, h;
} ClearRect;

static ClearRect clearQueue[MAX_CLEARS];
static int clearCount = 0;

static void queueClear(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (clearCount < MAX_CLEARS) {
        clearQueue[clearCount++] = (ClearRect){ x, y, w, h };
    } else {
        fullRedrawRequested = 1;
        clearCount = 0;
    }
}

// Safe pixel for Mode 4: clips to screen and keeps pixels out of the HUD strip.
static inline void safeSetPixel4(int x, int y, u8 colorIndex) {
    if (x < 0 || x >= SCREENWIDTH) return;
    if (y < HUD_HEIGHT || y >= SCREENHEIGHT) return;
    setPixel4(x, y, colorIndex);
}

// Draw a rectangle clipped to the gameplay area (below HUD) and screen bounds.
static void drawRectPlayfield(int x, int y, int w, int h, u8 colorIndex) {
    if (w <= 0 || h <= 0) return;

    if (y < HUD_HEIGHT) {
        int cut = HUD_HEIGHT - y;
        y = HUD_HEIGHT;
        h -= cut;
    }
    if (h <= 0) return;

    drawRect4(x, y, w, h, colorIndex);
}

static void fillPlayfield(u8 colorIndex) {
    drawRect4(0, HUD_HEIGHT, SCREENWIDTH, SCREENHEIGHT - HUD_HEIGHT, colorIndex);
}

/* =========================
   Static Screen Draw Helpers
   ========================= */

static void drawStartScreenStatic(void) {
    fillScreen4(0);
    drawFullScreenImage4(startBitmap);
    

    drawString4(80, 100, "Earn 25 points", CI_YELLOW);
    drawString4(100, 110, "A: Shoot", CI_YELLOW);
    drawString4(100, 120, "B: Dash", CI_YELLOW);
    drawString4(100, 130, "L: Bomb", CI_YELLOW);

    // Optional hint text (keeps your existing start art, just overlays text)
    drawString4(60, 145, "DOWN KEY: scoreboard", CI_YELLOW);
}

static void drawPauseScreenStatic(void) {
    // Load pause palette so pauseBitmap indices render correctly
    DMANow(3, pausePal, PALETTE, 256);

    fillScreen4(0); // use index 0 of pausePal (usually black)
    drawFullScreenImage4(pauseBitmap);

    drawString4(70, 100, "Feeling Stressed?", PAUSE_CI_YELLOW);
    drawString4(50, 110, "It's ok to take a break :)", PAUSE_CI_YELLOW);

    drawString4(60, 145, "DOWN KEY: scoreboard", PAUSE_CI_YELLOW);
}

static void drawWinScreenStatic(void) {
    fillScreen4(CI_BLACK);
    drawString4(100, 70, "YOU WIN!", CI_YELLOW);
    drawString4(60, 90, "Press START for menu", CI_YELLOW);
}

static void drawLoseScreenStatic(void) {
    fillScreen4(CI_BLACK);
    drawString4(100, 70, "YOU LOSE!", CI_YELLOW);
    drawString4(60, 90, "Press START for menu", CI_YELLOW);
}

static void drawScoreboardStatic(void) {
    fillScreen4(CI_BLACK);

    drawString4(20, 20, "SCOREBOARD", CI_YELLOW);

    char buf[32];

    sprintf(buf, "HIGH: %d", highScore);
    drawString4(20, 60, buf, CI_YELLOW);

    if (scoreboardShowCurrentScore) {
        sprintf(buf, "CURRENT: %d", score);
        drawString4(20, 80, buf, CI_YELLOW);
    }

    drawString4(20, 100, "DOWN KEY: go back", CI_YELLOW);
}

/* ==========
   Public API
   ========== */

GameState getState(void) {
    return state;
}

void initGame(void) {
    sfxInit();
    targetScore = 25;

    // IMPORTANT: do NOT reset highScore here; it must persist between restarts.
    goToStart();
}

/* =================
   State Transitions
   ================= */

void goToStart(void) {
    state = STATE_START;

    // Palette (keep as you had it)
    DMANow(3, startPal, PALETTE, 256);

    // Force redraw
    lastRenderedState = -1;
}

void goToGame(void) {
    // If coming from START/WIN/LOSE, fully reset gameplay (but not highScore).
    DMANow(3, startPal, PALETTE, 256);
    if (state == STATE_START || state == STATE_WIN || state == STATE_LOSE) {
        score = 0;
        frameCount = 0;
        spawnTimer = 45;
        asteroidSpawnCount = 0;
        screenShakeTimer = 0;

        initStars();
        initPlayer();
        initPools();

        fullRedrawRequested = 1;
        hudDirty = 1;
    }

    // If resuming from PAUSE, remove the pause overlay with a one-time redraw
    if (state == STATE_PAUSE) {
        fullRedrawRequested = 1;
        hudDirty = 1;
    }

    state = STATE_GAME;
}

void goToPause(void) {
    state = STATE_PAUSE;
    lastRenderedState = -1;
    hudDirty = 1;

    DMANow(3, pausePal, PALETTE, 256);
}

void goToWin(void) {
    state = STATE_WIN;
    lastRenderedState = -1;
    sfxWin();

    if (score > highScore) highScore = score;
}

void goToLose(void) {
    state = STATE_LOSE;
    lastRenderedState = -1;
    sfxLose();

    if (score > highScore) highScore = score;
}

void goToScoreboardFromStart(void) {
    scoreboardReturnState = STATE_START;
    scoreboardShowCurrentScore = 0;

    state = STATE_SCOREBOARD;
    DMANow(3, startPal, PALETTE, 256);
    lastRenderedState = -1;
}

void goToScoreboardFromPause(void) {
    scoreboardReturnState = STATE_PAUSE;
    scoreboardShowCurrentScore = 1;

    state = STATE_SCOREBOARD;
    DMANow(3, startPal, PALETTE, 256);
    lastRenderedState = -1;
}

/* ==========
   Game Update
   ========== */

void updateGame(void) {
    switch (state) {
        case STATE_START:
            // START begins game
            if (BUTTON_PRESSED(BUTTON_START)) {
                goToGame();
            }

            // Scoreboard accessible ONLY from START (use SELECT)
            if (BUTTON_PRESSED(BUTTON_DOWN)) {
                goToScoreboardFromStart();
            }
            break;

        case STATE_GAME: {
            // Debug cheats (hold SELECT + a cheat key)
            if (BUTTON_HELD(BUTTON_SELECT)) {
                u16 pressedNow = (~buttons);
                u16 cheatKeys = (BUTTON_START | BUTTON_A | BUTTON_B | BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN);
                u16 comboNow = pressedNow & cheatKeys;

                u16 newlyPressed = (comboNow & (~cheatLatch));
                cheatLatch = comboNow;

                // SELECT + START -> WIN
                if (newlyPressed & BUTTON_START) {
                    goToWin();
                    cheatLatch = 0;
                    break;
                }

                // SELECT + LEFT -> LOSE
                if (newlyPressed & BUTTON_LEFT) {
                    goToLose();
                    cheatLatch = 0;
                    break;
                }

                // (Removed: scoreboard access from GAME; scoreboard must be START/PAUSE only)

                // SELECT + B -> clear asteroids
                if (newlyPressed & BUTTON_B) {
                    for (int i = 0; i < MAX_ASTEROIDS; i++) {
                        if (asteroids[i].active) {
                            queueClear(asteroids[i].oldx, asteroids[i].oldy,
                                       asteroids[i].size, asteroids[i].size);
                            queueClear(asteroids[i].x, asteroids[i].y,
                                       asteroids[i].size, asteroids[i].size);
                            asteroids[i].active = 0;
                        }
                    }
                    cheatFlash();
                    break;
                }

                // SELECT + A -> reset score + restore lives
                if (newlyPressed & BUTTON_A) {
                    score = 0;
                    player.lives = 3;
                    player.invulnTimer = 0;
                    player.bombs = 0;
                    hudDirty = 1;
                    cheatFlash();
                    break;
                }

                // SELECT + UP -> restore lives only
                if (newlyPressed & BUTTON_UP) {
                    player.lives = 3;
                    player.invulnTimer = 0;
                    hudDirty = 1;
                    cheatFlash();
                    break;
                }

                // SELECT + RIGHT -> give bomb
                if (newlyPressed & BUTTON_RIGHT) {
                    player.bombs = 1;
                    hudDirty = 1;
                    cheatFlash();
                    break;
                }

                // If SELECT is held but no cheat fired, do nothing else this frame.
                break;
            } else {
                cheatLatch = 0;
            }

            // Pause (START) only when not using SELECT-cheats
            if (BUTTON_PRESSED(BUTTON_START)) {
                goToPause();
                break;
            }

            // Update world
            frameCount++;
            updateStars();
            updatePlayer();
            updateBullets();

            // Spawn asteroids over time
            spawnTimer--;
            if (spawnTimer <= 0) {
                spawnAsteroid();
                spawnTimer = clamp(60 - (frameCount / 240), 18, 60);
            }

            updateAsteroids();
            handleCollisions();

            // Win/lose checks
            if (score >= targetScore) {
                goToWin();
            }
            if (player.lives <= 0) {
                goToLose();
            }

            if (screenShakeTimer > 0) screenShakeTimer--;
            break;
        }

        case STATE_PAUSE:
            // START: resume
            if (BUTTON_PRESSED(BUTTON_START)) {
                goToGame();
            }
            // SELECT: menu
            else if (BUTTON_PRESSED(BUTTON_SELECT)) {
                goToStart();
            }
            // Scoreboard accessible ONLY from PAUSE (use L shoulder)
            else if (BUTTON_PRESSED(BUTTON_DOWN)) {
                goToScoreboardFromPause();
            }
            break;

        case STATE_WIN:
        case STATE_LOSE:
            if (BUTTON_PRESSED(BUTTON_START)) {
                goToStart();
            }
            break;

        case STATE_SCOREBOARD:
            // Always return to where we came from
            if (BUTTON_PRESSED(BUTTON_DOWN)) {
                if (scoreboardReturnState == STATE_PAUSE) {
                    goToPause();
                } else {
                    goToStart();
                }
            }
            break;
    }
}

/* ==========
   Game Draw
   ========== */

static void drawHUD(void) {
    int livesShown  = clamp(player.lives, 0, 9);
    int pointsShown = clamp(score, 0, 99);
    int bombsShown  = clamp(player.bombs, 0, 1);

    drawRect4(0, 0, SCREENWIDTH, HUD_HEIGHT, CI_BLACK);

    char hud[24];
    int idx = 0;

    hud[idx++] = 'L'; hud[idx++] = ':'; hud[idx++] = (char)('0' + livesShown); hud[idx++] = ' ';
    hud[idx++] = 'P'; hud[idx++] = ':'; hud[idx++] = (char)('0' + (pointsShown / 10));
    hud[idx++] = (char)('0' + (pointsShown % 10)); hud[idx++] = ' ';
    hud[idx++] = 'B'; hud[idx++] = ':'; hud[idx++] = (char)('0' + bombsShown);
    hud[idx] = '\0';

    drawString4(2, 2, hud, CI_WHITE);
}

void drawGame(void) {
    // 1) START / WIN / LOSE: draw once per state change, but to BOTH buffers
    if (state == STATE_START || state == STATE_WIN || state == STATE_LOSE) {
        if (state == lastRenderedState) return;
        lastRenderedState = state;

        if (state == STATE_START) {
            renderStaticToBothBuffers(drawStartScreenStatic);
        } else if (state == STATE_WIN) {
            renderStaticToBothBuffers(drawWinScreenStatic);
        } else {
            renderStaticToBothBuffers(drawLoseScreenStatic);
        }
        return;
    }

    // 2) PAUSE: draw once on entry, but to BOTH buffers
    if (state == STATE_PAUSE) {
        if (state == lastRenderedState) return;
        lastRenderedState = state;

        renderStaticToBothBuffers(drawPauseScreenStatic);
        return;
    }

    // 3) SCOREBOARD: draw once on entry, but to BOTH buffers
    if (state == STATE_SCOREBOARD) {
        if (state == lastRenderedState) return;
        lastRenderedState = state;

        renderStaticToBothBuffers(drawScoreboardStatic);
        return;
    }

    // 4) GAME: normal rendering (HUD always last)
    if (state != lastRenderedState) {
        lastRenderedState = state;
        fullRedrawRequested = 1;
        hudDirty = 1;
        clearCount = 0;
    }

    // Full redraw path (your current approach)
    fillPlayfield(CI_BLACK);

    drawStars();
    drawPlayer();
    drawBullets();
    drawAsteroids();

    drawHUD();

    fullRedrawRequested = 0;
    clearCount = 0;
}

/* ==========
   Stars
   ========== */

static void initStars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = (i * 13) % SCREENWIDTH;
        stars[i].y = HUD_HEIGHT + (i * 7) % (SCREENHEIGHT - HUD_HEIGHT);
        stars[i].oldx = stars[i].x;
        stars[i].oldy = stars[i].y;
        stars[i].speed = 1 + (i % 2);
    }
}

static void updateStars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].oldx = stars[i].x;
        stars[i].oldy = stars[i].y;

        stars[i].y += stars[i].speed;

        if (stars[i].y >= SCREENHEIGHT) {
            stars[i].y = HUD_HEIGHT;
            stars[i].x = (stars[i].x + 53) % SCREENWIDTH;
        }
    }
}

static void drawStars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        if (stars[i].y >= HUD_HEIGHT && stars[i].y < SCREENHEIGHT) {
            setPixel4(stars[i].x, stars[i].y, CI_GRAY);
        }
    }
}

/* ==========
   Player
   ========== */

static void initPlayer(void) {
    player.w = PLAYER_W;
    player.h = PLAYER_H;

    player.x = (SCREENWIDTH / 2) - (player.w / 2);
    player.y = (SCREENHEIGHT - 20);

    player.oldx = player.x;
    player.oldy = player.y;

    player.speed = 2;
    player.lives = 3;
    player.invulnTimer = 0;
    player.dashCooldown = 0;
    player.bombs = 0;
}

static void updatePlayer(void) {
    player.oldx = player.x;
    player.oldy = player.y;

    if (player.invulnTimer > 0) player.invulnTimer--;
    if (player.dashCooldown > 0) player.dashCooldown--;

    int spd = player.speed;

    if (BUTTON_HELD(BUTTON_LEFT))  player.x -= spd;
    if (BUTTON_HELD(BUTTON_RIGHT)) player.x += spd;
    if (BUTTON_HELD(BUTTON_UP))    player.y -= spd;
    if (BUTTON_HELD(BUTTON_DOWN))  player.y += spd;

    player.x = clamp(player.x, 0, SCREENWIDTH - player.w);
    player.y = clamp(player.y, HUD_HEIGHT, SCREENHEIGHT - player.h);

    if (BUTTON_PRESSED(BUTTON_A)) {
        fireBullet();
    }

    if (BUTTON_PRESSED(BUTTON_B) && player.dashCooldown == 0) {
        player.dashCooldown = 30;

        int dx = 0, dy = -1;
        if (BUTTON_HELD(BUTTON_LEFT))  dx = -1, dy = 0;
        if (BUTTON_HELD(BUTTON_RIGHT)) dx =  1, dy = 0;
        if (BUTTON_HELD(BUTTON_DOWN))  dx =  0, dy = 1;
        if (BUTTON_HELD(BUTTON_UP))    dx =  0, dy = -1;

        player.x = clamp(player.x + dx * 18, 0, SCREENWIDTH - player.w);
        player.y = clamp(player.y + dy * 18, HUD_HEIGHT, SCREENHEIGHT - player.h);
    }

    if (BUTTON_PRESSED(BUTTON_LSHOULDER)) {
        useBomb();
    }
}

static void drawPlayer(void) {
    if (player.invulnTimer > 0 && (player.invulnTimer / 4) % 2 == 0) {
        return;
    }

    drawRect4(player.x, player.y, player.w, player.h, CI_CYAN);
    safeSetPixel4(player.x + 3, player.y + 2, CI_WHITE);
}

/* ==========
   Pools
   ========== */

static void initPools(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].w = 2;
        bullets[i].h = 2;
        bullets[i].x = bullets[i].y = 0;
        bullets[i].oldx = bullets[i].oldy = 0;
        bullets[i].dx = 0;
        bullets[i].dy = -4;
    }

    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].active = 0;
        asteroids[i].size = 8;
        asteroids[i].x = asteroids[i].y = 0;
        asteroids[i].oldx = asteroids[i].oldy = 0;
        asteroids[i].dx = 0;
        asteroids[i].dy = 1;
        asteroids[i].hp = 1;
        asteroids[i].isBomb = 0;
    }
}

static void fireBullet(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;

            bullets[i].x = player.x + player.w / 2;
            bullets[i].y = player.y;
            bullets[i].oldx = bullets[i].x;
            bullets[i].oldy = bullets[i].y;

            sfxShoot();
            return;
        }
    }
}

static void updateBullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;

        bullets[i].oldx = bullets[i].x;
        bullets[i].oldy = bullets[i].y;

        bullets[i].y += bullets[i].dy;

        if (bullets[i].y < HUD_HEIGHT) {
            bullets[i].active = 0;
            queueClear(bullets[i].oldx, bullets[i].oldy, bullets[i].w, bullets[i].h);
            continue;
        }

        if (bullets[i].y < 0) {
            bullets[i].active = 0;
            queueClear(bullets[i].oldx, bullets[i].oldy, bullets[i].w, bullets[i].h);
        }
    }
}

static void drawBullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        drawRectPlayfield(bullets[i].x, bullets[i].y, bullets[i].w, bullets[i].h, CI_YELLOW);
    }
}

/* ==========
   Asteroids
   ========== */

static void spawnAsteroid(void) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) {
            asteroids[i].active = 1;

            asteroidSpawnCount++;
            asteroids[i].isBomb = (asteroidSpawnCount % 15 == 0);

            if (asteroids[i].isBomb) {
                asteroids[i].size = 8;
                asteroids[i].hp = 1;
            } else {
                asteroids[i].size = 6 + ((frameCount / 180) % 7);
                asteroids[i].hp = (asteroids[i].size >= 10) ? 2 : 1;
            }

            asteroids[i].x = (i * 29 + frameCount * 3) % (SCREENWIDTH - asteroids[i].size);
            asteroids[i].y = HUD_HEIGHT - asteroids[i].size;

            asteroids[i].oldx = asteroids[i].x;
            asteroids[i].oldy = asteroids[i].y;

            asteroids[i].dx = ((i % 3) - 1);
            asteroids[i].dy = 1 + (frameCount / 600);
            asteroids[i].dy = clamp(asteroids[i].dy, 1, 3);

            return;
        }
    }
}

static void updateAsteroids(void) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;

        asteroids[i].oldx = asteroids[i].x;
        asteroids[i].oldy = asteroids[i].y;

        asteroids[i].x += asteroids[i].dx;
        asteroids[i].y += asteroids[i].dy;

        if (asteroids[i].x <= 0 || asteroids[i].x >= SCREENWIDTH - asteroids[i].size) {
            asteroids[i].dx = -asteroids[i].dx;
            asteroids[i].x = clamp(asteroids[i].x, 0, SCREENWIDTH - asteroids[i].size);
        }

        if (asteroids[i].oldy < SCREENHEIGHT && asteroids[i].y >= SCREENHEIGHT) {
            queueClear(asteroids[i].oldx, asteroids[i].oldy,
                       asteroids[i].size, asteroids[i].size);
            asteroids[i].active = 0;
        }
    }
}

static void drawAsteroids(void) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;

        u8 c = asteroids[i].isBomb ? CI_MAGENTA : ((asteroids[i].hp == 2) ? CI_BROWN : CI_GRAY);
        drawRectPlayfield(asteroids[i].x, asteroids[i].y, asteroids[i].size, asteroids[i].size, c);
    }
}

/* ===================
   Collisions + Bomb
   =================== */

static void handleCollisions(void) {
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;

        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;

            if (collision(bullets[b].x, bullets[b].y, bullets[b].w, bullets[b].h,
                          asteroids[a].x, asteroids[a].y, asteroids[a].size, asteroids[a].size)) {

                bullets[b].active = 0;
                queueClear(bullets[b].oldx, bullets[b].oldy, bullets[b].w, bullets[b].h);

                asteroids[a].hp--;
                if (asteroids[a].hp <= 0) {
                    queueClear(asteroids[a].oldx, asteroids[a].oldy,
                               asteroids[a].size, asteroids[a].size);
                    queueClear(asteroids[a].x, asteroids[a].y,
                               asteroids[a].size, asteroids[a].size);

                    asteroids[a].active = 0;

                    if (asteroids[a].isBomb) {
                        player.bombs = 1;
                        playSfxPreset(SFXP_POWERUP);
                    } else {
                        score++;
                        sfxHit();
                    }

                    hudDirty = 1;
                }

                break;
            }
        }
    }

    if (player.invulnTimer == 0) {
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;

            if (collision(player.x, player.y, player.w, player.h,
                          asteroids[a].x, asteroids[a].y, asteroids[a].size, asteroids[a].size)) {

                player.lives--;
                player.invulnTimer = 45;
                hudDirty = 1;

                queueClear(asteroids[a].oldx, asteroids[a].oldy,
                           asteroids[a].size, asteroids[a].size);
                queueClear(asteroids[a].x, asteroids[a].y,
                           asteroids[a].size, asteroids[a].size);

                asteroids[a].active = 0;
                sfxHit();
                break;
            }
        }
    }
}

static void useBomb(void) {
    if (player.bombs <= 0) return;
    player.bombs--;

    int cleared = 0;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            queueClear(asteroids[i].oldx, asteroids[i].oldy,
                       asteroids[i].size, asteroids[i].size);
            queueClear(asteroids[i].x, asteroids[i].y,
                       asteroids[i].size, asteroids[i].size);
            asteroids[i].active = 0;
            cleared++;
        }
    }

    score += (cleared >= 3) ? 2 : 1;
    hudDirty = 1;

    screenShakeTimer = 10;
    sfxBomb();
}