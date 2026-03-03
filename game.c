#include "game.h"
#include "mode4.h"
#include "sfx.h"
#include "start.h"
#include "pause.h"
#include <stdio.h>
#include "rocket.h"
#include "powered_rocket.h"

// Module-level game state
static GameState state;

// Single player entity (ship/rocket)
static Player player;

// Object pools: fixed-size arrays with active flags (no malloc)
static Bullet bullets[MAX_BULLETS];       // object pool
static Asteroid asteroids[MAX_ASTEROIDS]; // object pool
static Star stars[MAX_STARS];             // background motion (array + polish)

// Game counters / timers used to scale difficulty + effects
static int score;
static int targetScore;
static int frameCount;
static int spawnTimer;
static int asteroidSpawnCount; // counts asteroid spawns for rare bomb-asteroid logic
static int screenShakeTimer; // used to the screen shake effect when aquiring the bomb

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

// safe set pixel for mode 4
static inline void safeSetPixel4(int x, int y, u8 colorIndex);

// function for drawing the rocket/powered rocket with a transparent background
static void drawSprite4TransparentDMA(int x, int y, int w, int h,
                                      const u16* bitmap, u8 transparentIndex, u8 palBase);

// Debug cheat latch (makes combos reliable even with weird timing)
static u16 cheatLatch = 0;
static int cheatFlashTimer = 0; // quick visual confirmation

// Prevents re-drawing heavy text screens every frame (reduces flicker).
static GameState lastRenderedState = -1;

// Mode 4 page-flip bookkeeping
static int fullRedrawRequested = 1; // when set, we force a full playfield redraw
static int hudDirty = 1;            // when set, HUD will be redrawn (values changed)

// High score (persists across restarts within the same run)
static int highScore = 0;

// Scoreboard behavior
static GameState scoreboardReturnState = STATE_START; // where to return when leaving scoreboard
static int scoreboardShowCurrentScore = 0;            // only show current score when opened from PAUSE

// videoBuffer should already exist in mode4.c/mode4.h
extern u16* videoBuffer;

// Draw a static screen to BOTH pages so page-flipping never reveals an old screen underneath
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

// Utility clamp used for positions, spawn timers, and HUD display bounds
static int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Simple “cheat fired” feedback timer (you can use this to flash something if desired)
static void cheatFlash(void) {
    cheatFlashTimer = 10; // 10 frames
}

// Clear queue (kept as-is: update never draws)
// Idea: when objects move, we queue rectangles to clear, and if it overflows we fall back to full redraw
#define MAX_CLEARS 64

typedef struct {
    int x, y, w, h;
} ClearRect;

static ClearRect clearQueue[MAX_CLEARS];
static int clearCount = 0;

// Enqueue an area to erase later; if too many, request a full redraw instead
static void queueClear(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (clearCount < MAX_CLEARS) {
        clearQueue[clearCount++] = (ClearRect){ x, y, w, h };
    } else {
        // Too many small clears is slower/complex, so switch to the safe full redraw path
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

    // Prevent overwriting the HUD by trimming anything that starts above HUD_HEIGHT
    if (y < HUD_HEIGHT) {
        int cut = HUD_HEIGHT - y;
        y = HUD_HEIGHT;
        h -= cut;
    }
    if (h <= 0) return;

    drawRect4(x, y, w, h, colorIndex);
}

// Fill only the playfield (leave HUD area to be drawn separately)
static void fillPlayfield(u8 colorIndex) {
    drawRect4(0, HUD_HEIGHT, SCREENWIDTH, SCREENHEIGHT - HUD_HEIGHT, colorIndex);
}


// Static Screen Draw Helpers

static void drawStartScreenStatic(void) {
    // Clear the page then DMA the full-screen start bitmap
    fillScreen4(0);
    drawFullScreenImage4(startBitmap);

    // String drwaings on top of the image
    // Instructions are drawn using a single palette index color (CI_YELLOW)
    drawString4(80, 100, "Earn 25 points", CI_YELLOW);
    drawString4(100, 110, "A: Shoot", CI_YELLOW);
    drawString4(100, 120, "B: Dash", CI_YELLOW);
    drawString4(100, 130, "L: Bomb", CI_YELLOW);

    // Scoreboard navigation
    drawString4(60, 145, "DOWN KEY: scoreboard", CI_YELLOW);
}

static void drawPauseScreenStatic(void) {
    // Load pause palette so pauseBitmap indices render correctly
    DMANow(3, pausePal, PALETTE, 256);

    // Clear using pause palette index 0, then draw pause screen image
    fillScreen4(0); // use index 0 of pausePal (usually black)
    drawFullScreenImage4(pauseBitmap);

    // String drawings on top of the image
    // Note: these use PAUSE palette indices (PAUSE_CI_YELLOW), not the start palette indices
    drawString4(70, 100, "Feeling Stressed?", PAUSE_CI_YELLOW);
    drawString4(50, 110, "It's ok to take a break :)", PAUSE_CI_YELLOW);

    // Pause controls
    drawString4(60, 130, "SELECT: restart game", PAUSE_CI_YELLOW);
    drawString4(60, 145, "DOWN KEY: scoreboard", PAUSE_CI_YELLOW);
}

// Win screen drawing
static void drawWinScreenStatic(void) {
    // Simple text-only screen (no bitmap) using base palette
    fillScreen4(CI_BLACK);

    // String text for winning screen
    drawString4(100, 70, "YOU WIN!", CI_YELLOW);
    drawString4(60, 90, "Press START for menu", CI_YELLOW);
}

// Lose screen drawing
static void drawLoseScreenStatic(void) {
    // Simple text-only screen (no bitmap) using base palette
    fillScreen4(CI_BLACK);

    // String text for losing screen
    drawString4(100, 70, "YOU LOSE!", CI_YELLOW);
    drawString4(60, 90, "Press START for menu", CI_YELLOW);
}

// Scoreboard screen drawing
static void drawScoreboardStatic(void) {
    fillScreen4(CI_BLACK);

    // String text for scoreboard 
    drawString4(20, 20, "SCOREBOARD", CI_YELLOW);

    char buf[32];

    // High score 
    sprintf(buf, "HIGH: %d", highScore);
    drawString4(20, 60, buf, CI_YELLOW);

    // Current score
    // Only shown if scoreboard was opened from PAUSE (not from START)
    if (scoreboardShowCurrentScore) {
        sprintf(buf, "CURRENT: %d", score);
        drawString4(20, 80, buf, CI_YELLOW);
    }

    // string text for go back
    drawString4(20, 100, "DOWN KEY: go back", CI_YELLOW);
}

// Public API
// Exposes current state to other files (ex: main.c)
GameState getState(void) {
    return state;
}

void initGame(void) {
    // Initialize sound effects system (timers/DMA/audio regs)
    sfxInit();

    // Win condition: reach this score
    targetScore = 25;

    // Start in the menu state
    goToStart();
}

// State Transitions

// goToStart 
void goToStart(void) {
    state = STATE_START;

    // Palette (keep as you had it)
    // Start palette supports start screen bitmap + standard UI colors
    DMANow(3, startPal, PALETTE, 256);

    // Force redraw
    // lastRenderedState=-1 ensures drawGame will redraw the static start screen
    lastRenderedState = -1;
}

// goToGame
void goToGame(void) {
    // If coming from START/WIN/LOSE, fully reset gameplay (but not highScore).
    // Also ensures palette is the gameplay/start palette again (after pause palette).
    DMANow(3, startPal, PALETTE, 256);

    // Patch both rocket palettes into free palette slots
    // This lets rocket pixels (bitmap indices) map into distinct palette entries
    DMANow(3, rocketPal, &PALETTE[ROCKET_PAL_BASE], 8);
    DMANow(3, powered_rocketPal, &PALETTE[POWERED_ROCKET_PAL_BASE], 8);

    // Only reset the whole world when starting a new run (not when resuming from pause)
    if (state == STATE_START || state == STATE_WIN || state == STATE_LOSE) {
        score = 0;
        frameCount = 0;
        spawnTimer = 45;
        asteroidSpawnCount = 0;
        screenShakeTimer = 0;

        initStars();
        initPlayer();
        initPools();

        // Force full draw and HUD draw on the first frame of gameplay
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

// goToPause
void goToPause(void) {
    state = STATE_PAUSE;

    // Force pause screen to redraw once on entry
    lastRenderedState = -1;

    // HUD values may have changed before pausing; mark dirty (even though pause screen is static)
    hudDirty = 1;

    // use pausePal not startPal
    // Pause bitmap was exported with a different palette so we must swap palettes here
    DMANow(3, pausePal, PALETTE, 256);
}

// goToWin
void goToWin(void) {
    state = STATE_WIN;
    lastRenderedState = -1;

    // Play win sound once on transition
    sfxWin();

    // reset high score if beat
    // High score persists even when restarting the game
    if (score > highScore) highScore = score;
}

// goToLose
void goToLose(void) {
    state = STATE_LOSE;
    lastRenderedState = -1;

    // Play lose sound once on transition
    sfxLose();

    // reset high score if beat
    if (score > highScore) highScore = score;
}

// goToScoreboard - From the Start Screen
void goToScoreboardFromStart(void) {
    // Scoreboard returns to start screen in this case
    scoreboardReturnState = STATE_START;

    // Hide current score when opened from start menu
    scoreboardShowCurrentScore = 0;

    state = STATE_SCOREBOARD;

    // Use start palette for scoreboard text colors
    DMANow(3, startPal, PALETTE, 256);

    // Force redraw on entry
    lastRenderedState = -1;
}

// goToScoreboard - From the Pause Screen
void goToScoreboardFromPause(void) {
    // Scoreboard returns to pause screen in this case
    scoreboardReturnState = STATE_PAUSE;

    // Show current score when opened mid-run
    scoreboardShowCurrentScore = 1;

    state = STATE_SCOREBOARD;

    // Use start palette for scoreboard text colors
    DMANow(3, startPal, PALETTE, 256);

    // Force redraw on entry
    lastRenderedState = -1;
}

// Game Update
void updateGame(void) {
    // State machine: only the currently active state handles input and updates
    switch (state) {
        case STATE_START:
            // START begins game
            if (BUTTON_PRESSED(BUTTON_START)) {
                goToGame();
            }

            // Scoreboard accessible ONLY from START
            if (BUTTON_PRESSED(BUTTON_DOWN)) {
                goToScoreboardFromStart();
            }
            break;

        case STATE_GAME: {
            // Debug cheats (hold SELECT + a cheat key)
            // Cheat system uses cheatLatch so a held combo triggers once instead of every frame
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

                // SELECT + B -> clear asteroids
                if (newlyPressed & BUTTON_B) {
                    for (int i = 0; i < MAX_ASTEROIDS; i++) {
                        if (asteroids[i].active) {
                            // Clear both old and current position rectangles for safety
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
                // This prevents normal gameplay input from happening while holding SELECT.
                break;
            } else {
                // If SELECT is not held, reset latch so the next cheat press can trigger
                cheatLatch = 0;
            }

            // Pause (START) only when not using SELECT-cheats
            if (BUTTON_PRESSED(BUTTON_START)) {
                goToPause();
                break;
            }

            // Update world (logic only; drawing happens in drawGame)
            frameCount++;
            updateStars();
            updatePlayer();
            updateBullets();

            // Spawn asteroids over time (spawnTimer shrinks as frameCount increases)
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

            // Screen shake effect timer counts down each frame
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
            // Scoreboard accessible ONLY from PAUSE
            else if (BUTTON_PRESSED(BUTTON_DOWN)) {
                goToScoreboardFromPause();
            }
            break;

        case STATE_WIN:
        case STATE_LOSE:
            // Return to menu
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


// Game Draw
static void drawHUD(void) {
    // Clamp values so HUD text stays single-digit / two-digit and doesn’t render weird characters
    int livesShown  = clamp(player.lives, 0, 9);
    int pointsShown = clamp(score, 0, 99);
    int bombsShown  = clamp(player.bombs, 0, 1);

    // HUD background bar (separate from playfield)
    drawRect4(0, 0, SCREENWIDTH, HUD_HEIGHT, CI_BLACK);

    // Build a tiny HUD string without sprintf for speed (and to avoid overhead each frame)
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
    // Static states: draw once on entry and render to BOTH buffers so page flipping is safe
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
    // If we just entered GAME from another state, force a full redraw
    if (state != lastRenderedState) {
        lastRenderedState = state;
        fullRedrawRequested = 1;
        hudDirty = 1;
        clearCount = 0;
    }

    // Full redraw path (your current approach)
    // Clears the entire playfield so no old sprites remain
    fillPlayfield(CI_BLACK);

    // Draw in back-to-front order: background -> player -> projectiles -> enemies -> HUD
    drawStars();
    drawPlayer();
    drawBullets();
    drawAsteroids();

    // HUD drawn last so it stays readable and isn’t overwritten by playfield drawing
    drawHUD();

    // Reset redraw bookkeeping
    fullRedrawRequested = 0;
    clearCount = 0;
}


// Stars
// initialize the stars
static void initStars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        // Deterministic spread pattern so stars look evenly distributed without RNG
        stars[i].x = (i * 13) % SCREENWIDTH;
        stars[i].y = HUD_HEIGHT + (i * 7) % (SCREENHEIGHT - HUD_HEIGHT);
        stars[i].oldx = stars[i].x;
        stars[i].oldy = stars[i].y;
        stars[i].speed = 1 + (i % 2); // slight variation in speed to add depth
    }
}

// update stars
static void updateStars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        // Save previous position (useful if you ever switch back to partial redraw/clears)
        stars[i].oldx = stars[i].x;
        stars[i].oldy = stars[i].y;

        // Stars drift downward to simulate motion through space
        stars[i].y += stars[i].speed;

        // Wrap stars to the top when they leave the screen
        if (stars[i].y >= SCREENHEIGHT) {
            stars[i].y = HUD_HEIGHT;
            stars[i].x = (stars[i].x + 53) % SCREENWIDTH;
        }
    }
}

// draw stars 
static void drawStars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        // Only draw inside playfield area
        if (stars[i].y >= HUD_HEIGHT && stars[i].y < SCREENHEIGHT) {
            setPixel4(stars[i].x, stars[i].y, CI_GRAY);
        }
    }
}


// Player
// initalize the player 
static void initPlayer(void) {
    // set the player width and height to the rocket width and height
    player.w = ROCKET_WIDTH;
    player.h = ROCKET_HEIGHT;

    // set player x and y (spawn near bottom center)
    player.x = (SCREENWIDTH / 2) - (player.w / 2);
    player.y = (SCREENHEIGHT - 20);

    // old position starts as current so first frame doesn’t “clear” random area
    player.oldx = player.x;
    player.oldy = player.y;

    // core stats
    player.speed = 2;
    player.lives = 3;
    player.invulnTimer = 0; // temporary invulnerability after a hit
    player.dashCooldown = 0; // prevents spamming dash
    player.bombs = 0; // powerup count (0 or 1)
}

// update the player
static void updatePlayer(void) {
    // Track old position (useful for clearing/damage checks)
    player.oldx = player.x;
    player.oldy = player.y;

    // Tick down timers each frame
    if (player.invulnTimer > 0) player.invulnTimer--;
    if (player.dashCooldown > 0) player.dashCooldown--;

    int spd = player.speed;

    // player controls (held movement)
    if (BUTTON_HELD(BUTTON_LEFT))  player.x -= spd;
    if (BUTTON_HELD(BUTTON_RIGHT)) player.x += spd;
    if (BUTTON_HELD(BUTTON_UP))    player.y -= spd;
    if (BUTTON_HELD(BUTTON_DOWN))  player.y += spd;

    // Clamp to screen bounds, and keep player below HUD strip
    player.x = clamp(player.x, 0, SCREENWIDTH - player.w);
    player.y = clamp(player.y, HUD_HEIGHT, SCREENHEIGHT - player.h);

    // bullet firing (edge-triggered)
    if (BUTTON_PRESSED(BUTTON_A)) {
        fireBullet();
    }

    // dash cooldown for the player
    // Dash is a short burst in a direction (or up by default) then goes on cooldown
    if (BUTTON_PRESSED(BUTTON_B) && player.dashCooldown == 0) {
        player.dashCooldown = 30;

        // Default dash direction is up; if a direction is held, dash that way instead
        int dx = 0, dy = -1;
        if (BUTTON_HELD(BUTTON_LEFT))  dx = -1, dy = 0;
        if (BUTTON_HELD(BUTTON_RIGHT)) dx =  1, dy = 0;
        if (BUTTON_HELD(BUTTON_DOWN))  dx =  0, dy = 1;
        if (BUTTON_HELD(BUTTON_UP))    dx =  0, dy = -1;

        // Apply burst movement and clamp again
        player.x = clamp(player.x + dx * 18, 0, SCREENWIDTH - player.w);
        player.y = clamp(player.y + dy * 18, HUD_HEIGHT, SCREENHEIGHT - player.h);
    }

    // use bomb (only works if player has one)
    if (BUTTON_PRESSED(BUTTON_LSHOULDER)) {
        useBomb();
    }

    // Final clamp after all movement
    player.x = clamp(player.x, 0, SCREENWIDTH - player.w);
    player.y = clamp(player.y, HUD_HEIGHT, SCREENHEIGHT - player.h);

    // force even x for Mode 4 DMA blit alignment
    // Mode 4 stores 2 pixels per u16, so word-aligned drawing prefers even x
    player.x &= ~1;
}

static void drawSprite4TransparentDMA(
    int x, int y,
    int w, int h,
    const u16* bitmap,
    u8 transparentIndex,
    u8 palBase
) {
    // DMA row-blitter for Mode 4 sprites with transparency:
    // - reads sprite bitmap words (2 pixels per u16)
    // - keeps background pixels where sprite pixels are transparentIndex
    // - adds palBase to remap sprite indices into a palette “bank”
    if (w <= 0 || h <= 0) return;

    // Mode 4 works best with even x; keep your behavior
    int alignedX = x & ~1;          // round down to even
    int alignedEndX = alignedX + w; // end is exclusive in pixels

    int startY = y;
    int endY = y + h;               // exclusive

    // Vertical clip (also prevents drawing over HUD)
    int clippedStartY = startY;
    int clippedEndY = endY;

    if (clippedStartY < HUD_HEIGHT) clippedStartY = HUD_HEIGHT;
    if (clippedStartY < 0) clippedStartY = 0;
    if (clippedEndY > SCREENHEIGHT) clippedEndY = SCREENHEIGHT;

    if (clippedEndY <= clippedStartY) return;

    // Horizontal clip
    int clippedStartX = alignedX;
    int clippedEndX = alignedEndX;

    if (clippedStartX < 0) clippedStartX = 0;
    if (clippedEndX > SCREENWIDTH) clippedEndX = SCREENWIDTH;

    // Keep word alignment (even pixel boundaries)
    clippedStartX &= ~1;
    clippedEndX &= ~1;

    if (clippedEndX <= clippedStartX) return;

    // Convert to words (2 pixels per u16)
    int wordsPerRowSrc = w / 2;
    int wordsToDraw = (clippedEndX - clippedStartX) / 2;

    // How far into the source sprite we start (in words)
    int srcWordOffset = (clippedStartX - alignedX) / 2;

    // How far into the source sprite we start (in rows)
    int srcRowOffset = clippedStartY - startY;

    // Temp row buffer (max 240px -> 120 words)
    // This avoids per-pixel VRAM writes; we DMA one contiguous row after blending
    static u16 rowBuf[SCREENWIDTH / 2];

    for (int r = 0; r < (clippedEndY - clippedStartY); r++) {
        int screenY = clippedStartY + r;

        // Destination in Mode 4 is addressed in u16 words (2 pixels each)
        u16* dst = videoBuffer + ((screenY * SCREENWIDTH + clippedStartX) / 2);

        // Source points into the bitmap's row data, offset for clipping
        const u16* src = bitmap
            + (srcRowOffset + r) * wordsPerRowSrc
            + srcWordOffset;

        // Blend row: keep destination bytes for transparent pixels
        // Each u16 contains two 8-bit color indices: low byte (pixel 0), high byte (pixel 1)
        for (int i = 0; i < wordsToDraw; i++) {
            u16 s = src[i];
            u16 d = dst[i];

            u8 s0 = (u8)(s & 0x00FF);
            u8 s1 = (u8)(s >> 8);

            u8 d0 = (u8)(d & 0x00FF);
            u8 d1 = (u8)(d >> 8);

            // If a sprite pixel is not transparent, remap it into the palette bank (palBase)
            if (s0 != transparentIndex) d0 = (u8)(s0 + palBase);
            if (s1 != transparentIndex) d1 = (u8)(s1 + palBase);

            rowBuf[i] = (u16)(d0 | (d1 << 8));
        }

        // DMA the blended row into VRAM in one shot
        DMANow(3, rowBuf, dst, wordsToDraw);
    }
}

static void drawPlayer(void) {
    // Flicker effect during invulnerability: skip draw on alternating frames
    if (player.invulnTimer > 0 && (player.invulnTimer / 4) % 2 == 0) {
        return;
    }

    // Rocket sprite swaps based on whether the player currently holds a bomb powerup
    if (player.bombs > 0) {
        // Powered rocket when holding a bomb
        drawSprite4TransparentDMA(
            player.x, player.y,
            POWERED_ROCKET_WIDTH, POWERED_ROCKET_HEIGHT,
            powered_rocketBitmap,
            POWERED_TRANSPARENT_INDEX,
            POWERED_ROCKET_PAL_BASE
        );
    } else {
        // Normal rocket
        drawSprite4TransparentDMA(
            player.x, player.y,
            ROCKET_WIDTH, ROCKET_HEIGHT,
            rocketBitmap,
            ROCKET_TRANSPARENT_INDEX,
            ROCKET_PAL_BASE
        );
    }
}

// Object Pools
// initalize pools
static void initPools(void) {
    // bullet pool
    // bullets reuse fixed slots; “active” determines whether the slot is currently in play
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].w = 2;
        bullets[i].h = 2;
        bullets[i].x = bullets[i].y = 0;
        bullets[i].oldx = bullets[i].oldy = 0;
        bullets[i].dx = 0;
        bullets[i].dy = -4; // upward motion
    }

    // asteroid pool
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].active = 0;
        asteroids[i].size = 8;
        asteroids[i].x = asteroids[i].y = 0;
        asteroids[i].oldx = 0;
        asteroids[i].oldy = 0;
        asteroids[i].dx = 0;
        asteroids[i].dy = 1;
        asteroids[i].hp = 1;     // larger asteroids can take 2 hits
        asteroids[i].isBomb = 0; // special asteroid that grants a bomb when destroyed
    }
}

// Firing bullet function
static void fireBullet(void) {
    // Find the first inactive bullet slot and “activate” it
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;

            // Spawn from the player’s center-top
            bullets[i].x = player.x + player.w / 2;
            bullets[i].y = player.y;
            bullets[i].oldx = bullets[i].x;
            bullets[i].oldy = bullets[i].y;

            sfxShoot(); // shooting sound 
            return;
        }
    }
}

// Update bullet
static void updateBullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;

        // Save old position for clearing / collision logic
        bullets[i].oldx = bullets[i].x;
        bullets[i].oldy = bullets[i].y;

        // Move bullet upward
        bullets[i].y += bullets[i].dy;

        // avoid the HUD (the lives and points box)
        // If a bullet enters the HUD area, despawn it so it never overwrites HUD pixels
        if (bullets[i].y < HUD_HEIGHT) {
            bullets[i].active = 0;
            queueClear(bullets[i].oldx, bullets[i].oldy, bullets[i].w, bullets[i].h);
            continue;
        }

        // Offscreen check (extra safety)
        if (bullets[i].y < 0) {
            bullets[i].active = 0;
            queueClear(bullets[i].oldx, bullets[i].oldy, bullets[i].w, bullets[i].h);
        }
    }
}

// Draw bullets
static void drawBullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        // Simple 2x2 rectangles in the playfield
        drawRectPlayfield(bullets[i].x, bullets[i].y, bullets[i].w, bullets[i].h, CI_YELLOW);
    }
}


// Asteroids
static void spawnAsteroid(void) {
    // Find the first inactive asteroid slot and activate it
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) {
            asteroids[i].active = 1;

            // Every N spawns create a special “bomb asteroid”
            asteroidSpawnCount++;
            asteroids[i].isBomb = (asteroidSpawnCount % 15 == 0);

            // Bomb asteroids are small and 1 HP; normal asteroids scale with time
            if (asteroids[i].isBomb) {
                asteroids[i].size = 8;
                asteroids[i].hp = 1;
            } else {
                asteroids[i].size = 6 + ((frameCount / 180) % 7);
                asteroids[i].hp = (asteroids[i].size >= 10) ? 2 : 1;
            }

            // Spawn across the top area (just above HUD line)
            asteroids[i].x = (i * 29 + frameCount * 3) % (SCREENWIDTH - asteroids[i].size);
            asteroids[i].y = HUD_HEIGHT - asteroids[i].size;

            // Initialize old position for clean clearing/collision
            asteroids[i].oldx = asteroids[i].x;
            asteroids[i].oldy = asteroids[i].y;

            // Horizontal drift varies by slot index; vertical speed ramps slowly over time
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

        // Save old position for clearing and collision cleanup
        asteroids[i].oldx = asteroids[i].x;
        asteroids[i].oldy = asteroids[i].y;

        // Apply velocity
        asteroids[i].x += asteroids[i].dx;
        asteroids[i].y += asteroids[i].dy;

        // Bounce off left/right edges
        if (asteroids[i].x <= 0 || asteroids[i].x >= SCREENWIDTH - asteroids[i].size) {
            asteroids[i].dx = -asteroids[i].dx;
            asteroids[i].x = clamp(asteroids[i].x, 0, SCREENWIDTH - asteroids[i].size);
        }

        // If it moves past the bottom edge, despawn and queue clear of its last visible spot
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

        // Color encodes asteroid type/HP:
        // - bomb asteroid: magenta
        // - 2 HP asteroid: brown
        // - 1 HP asteroid: gray
        u8 c = asteroids[i].isBomb ? CI_MAGENTA : ((asteroids[i].hp == 2) ? CI_BROWN : CI_GRAY);
        drawRectPlayfield(asteroids[i].x, asteroids[i].y, asteroids[i].size, asteroids[i].size, c);
    }
}

/* ===================
   Collisions + Bomb
   =================== */

static void handleCollisions(void) {
    // Bullet vs asteroid collisions
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;

        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;

            // AABB collision check using your helper
            if (collision(bullets[b].x, bullets[b].y, bullets[b].w, bullets[b].h,
                          asteroids[a].x, asteroids[a].y, asteroids[a].size, asteroids[a].size)) {

                // Bullet despawns on hit
                bullets[b].active = 0;
                queueClear(bullets[b].oldx, bullets[b].oldy, bullets[b].w, bullets[b].h);

                // Asteroid loses HP; if it reaches 0, despawn and award reward
                asteroids[a].hp--;
                if (asteroids[a].hp <= 0) {
                    // Clear both previous and current asteroid rect to avoid leftover pixels
                    queueClear(asteroids[a].oldx, asteroids[a].oldy,
                               asteroids[a].size, asteroids[a].size);
                    queueClear(asteroids[a].x, asteroids[a].y,
                               asteroids[a].size, asteroids[a].size);

                    asteroids[a].active = 0;

                    // Bomb asteroid grants a bomb powerup; normal asteroid grants a point
                    if (asteroids[a].isBomb) {
                        player.bombs = 1;
                        playSfxPreset(SFXP_POWERUP);
                    } else {
                        score++;
                        sfxHit();
                    }

                    // HUD needs update because score/lives/bombs changed
                    hudDirty = 1;
                }

                // Stop checking other asteroids for this bullet (bullet is gone)
                break;
            }
        }
    }

    // Player vs asteroid collisions (only if not invulnerable)
    if (player.invulnTimer == 0) {
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;

            if (collision(player.x, player.y, player.w, player.h,
                          asteroids[a].x, asteroids[a].y, asteroids[a].size, asteroids[a].size)) {

                // Player takes damage and becomes invulnerable briefly
                player.lives--;
                player.invulnTimer = 45;
                hudDirty = 1;

                // Despawn the asteroid that hit the player
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
    // Bomb only works if the player currently has one
    if (player.bombs <= 0) return;

    // Consume bomb
    player.bombs--;

    // Clear all active asteroids immediately
    int cleared = 0;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            // Clear both old and new rectangles to ensure no leftover pixels
            queueClear(asteroids[i].oldx, asteroids[i].oldy,
                       asteroids[i].size, asteroids[i].size);
            queueClear(asteroids[i].x, asteroids[i].y,
                       asteroids[i].size, asteroids[i].size);
            asteroids[i].active = 0;
            cleared++;
        }
    }

    // Reward: bomb gives points, with a small bonus if you cleared a lot
    score += (cleared >= 3) ? 2 : 1;
    hudDirty = 1;

    // Start screen shake effect and play bomb sound
    screenShakeTimer = 10;
    sfxBomb();
}