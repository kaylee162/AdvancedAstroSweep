#include "gba.h"
#include "mode4.h"
#include "game.h"
#include "start.h"

// Buttons
u16 buttons;
u16 oldButtons;

static void initialize(void);

// Main
int main(void) {
    // Initialize and Initialize Game
    initialize();
    initGame();

    while (1) {
        oldButtons = buttons;
        buttons = REG_BUTTONS;

    // Load the fullscreen start image palette (256 colors), then overwrite
    // the first few entries with known UI/game colors.
    DMANow(3, startPal, PALETTE, 256);

        // Game logic update stays the same
        updateGame();

        // Draw during VBlank to reduce flicker/tearing
        waitForVBlank();
        drawGame();
        flipPage();
    }
}

static void initialize(void) {
    // Set mode and background
    REG_DISPCTL = MODE(4) | BG_ENABLE(2);

    // Start with front buffer displayed, but draw to back buffer.
    // (flipPage() will swap which page is displayed and set videoBuffer for off-screen drawing.)
    videoBuffer = BACKBUFFER;

    // Set buttons
    oldButtons = 0;
    buttons = REG_BUTTONS;

    // Load the fullscreen start image palette (256 colors), then overwrite
    // the first few entries with known UI/game colors.
    DMANow(3, startPal, PALETTE, 256);
    
    // Clear once at startup
    waitForVBlank();
    fillScreen4(CI_BLACK);
}