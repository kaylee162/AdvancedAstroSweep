#include "mode4.h"
#include "font.h"

// NOTE: We reuse gba.c's `videoBuffer` global as the "current draw buffer" pointer.
// In Mode 4, `videoBuffer` points to either FRONTBUFFER or BACKBUFFER, and each u16
// holds two 8bpp pixels.
extern u16* videoBuffer;

// Swap the displayed page and update videoBuffer to the other page (so we always draw off-screen).
void flipPage(void) {
    // If we're currently displaying the front buffer, switch to back; otherwise switch to front.
    if (REG_DISPCTL & DISP_BACKBUFFER) {
        // Back is currently displayed -> draw to back? no, we want to draw to the other page (front).
        REG_DISPCTL &= ~DISP_BACKBUFFER; // display front
        videoBuffer = BACKBUFFER;        // draw to back (off-screen)
    } else {
        // Front is currently displayed -> show back, draw to front
        REG_DISPCTL |= DISP_BACKBUFFER;  // display back
        videoBuffer = FRONTBUFFER;       // draw to front (off-screen)
    }
}

// Sets a pixel in Mode 4 (8bpp palette index)
void setPixel4(int x, int y, u8 colorIndex) {
    int index = OFFSET(x, y, SCREENWIDTH) >> 1; // /2 because 2 pixels per u16
    u16 pixelData = videoBuffer[index];

    if (x & 1) {
        // odd x -> high byte
        pixelData = (pixelData & 0x00FF) | (colorIndex << 8);
    } else {
        // even x -> low byte
        pixelData = (pixelData & 0xFF00) | colorIndex;
    }

    videoBuffer[index] = pixelData;
}

// Draws a rectangle in Mode 4 using DMA per scanline.
void drawRect4(int x, int y, int width, int height, u8 colorIndex) {
    if (width <= 0 || height <= 0) return;

    // Quick reject if fully off-screen (clipping is handled by callers for most game objects,
    // but this keeps accidental off-screen rects from writing outside the visible area).
    if (x >= SCREENWIDTH || y >= SCREENHEIGHT) return;
    if (x + width <= 0 || y + height <= 0) return;

    // Clip to screen bounds.
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > SCREENWIDTH) width = SCREENWIDTH - x;
    if (y + height > SCREENHEIGHT) height = SCREENHEIGHT - y;
    if (width <= 0 || height <= 0) return;

    // One u16 packs two identical pixels.
    u16 packed = colorIndex | (colorIndex << 8);

    for (int r = 0; r < height; r++) {
        int row = y + r;

        // Handle awkward alignments (because pixels are packed).
        if (width == 1) {
            setPixel4(x, row, colorIndex);
        } else if (width == 2) {
            setPixel4(x, row, colorIndex);
            setPixel4(x + 1, row, colorIndex);
        } else if ((x & 1) && (width & 1)) {
            // odd x, odd width: set first pixel, DMA the rest (ends on even)
            setPixel4(x, row, colorIndex);
            DMANow(3, &packed, &videoBuffer[OFFSET(x + 1, row, SCREENWIDTH) >> 1],
                   DMA_SOURCE_FIXED | ((width - 1) >> 1));
        } else if (width & 1) {
            // even x, odd width: DMA most, then set last pixel
            DMANow(3, &packed, &videoBuffer[OFFSET(x, row, SCREENWIDTH) >> 1],
                   DMA_SOURCE_FIXED | ((width - 1) >> 1));
            setPixel4(x + width - 1, row, colorIndex);
        } else if (x & 1) {
            // odd x, even width: set first and last, DMA middle
            setPixel4(x, row, colorIndex);
            DMANow(3, &packed, &videoBuffer[OFFSET(x + 1, row, SCREENWIDTH) >> 1],
                   DMA_SOURCE_FIXED | ((width - 2) >> 1));
            setPixel4(x + width - 1, row, colorIndex);
        } else {
            // even x, even width: perfect DMA
            DMANow(3, &packed, &videoBuffer[OFFSET(x, row, SCREENWIDTH) >> 1],
                   DMA_SOURCE_FIXED | (width >> 1));
        }
    }
}

// Fills the whole screen in Mode 4 using DMA.
void fillScreen4(u8 colorIndex) {
    u16 packed = colorIndex | (colorIndex << 8);
    DMANow(3, &packed, videoBuffer, DMA_SOURCE_FIXED | ((SCREENWIDTH * SCREENHEIGHT) >> 1));
}

// Draw an 8bpp image in Mode 4. `image` is packed (2 pixels per u16) row-major.
void drawImage4(int x, int y, int width, int height, const u16 *image) {
    if (width <= 0 || height <= 0) return;
    for (int r = 0; r < height; r++) {
        DMANow(3,
               &image[OFFSET(0, r, width) >> 1],
               &videoBuffer[OFFSET(x, y + r, SCREENWIDTH) >> 1],
               (width >> 1));
    }
}

// Draw a fullscreen 240x160 image in Mode 4.
void drawFullscreenImage4(const u16 *image) {
    DMANow(3, image, videoBuffer, (SCREENWIDTH * SCREENHEIGHT) >> 1);
}

// Draw a sub-rectangle from a larger packed 8bpp image.
void drawSubImage4(int x, int y, int width, int height,
                   const u16 *image, int imageWidth,
                   int srcX, int srcY) {
    if (width <= 0 || height <= 0) return;

    // srcX should be even for perfect alignment; if it's odd, we still work,
    // but the source packing becomes messy. Keep it simple: force even.
    if (srcX & 1) srcX--;

    int srcWordsPerRow = imageWidth >> 1;
    int dstWordsPerRow = SCREENWIDTH >> 1;
    int copyWords = width >> 1;

    for (int r = 0; r < height; r++) {
        const u16* src = &image[(srcY + r) * srcWordsPerRow + (srcX >> 1)];
        u16* dst = &videoBuffer[(y + r) * dstWordsPerRow + (x >> 1)];
        DMANow(3, src, dst, copyWords);
    }
}

// Text drawing (slow; avoid calling every frame)
void drawChar4(int x, int y, char ch, u8 colorIndex) {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 6; c++) {
            if (fontdata[48 * ch + OFFSET(c, r, 6)]) {
                setPixel4(x + c, y + r, colorIndex);
            }
        }
    }
}

void drawString4(int x, int y, char *str, u8 colorIndex) {
    while (*str) {
        drawChar4(x, y, *str, colorIndex);
        x += 6;
        str++;
    }
}
