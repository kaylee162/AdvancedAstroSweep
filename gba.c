#include "gba.h"
#include "font.h"

// Pointer to Mode 3 VRAM framebuffer
u16* videoBuffer = (u16*)0x06000000;

// Faster drawRect function: DMA one scanline per row
void drawRectangleClipped(int x, int y, int width, int height, u16 color) {
    if (width <= 0 || height <= 0) return;

    // Clip to screen bounds 
    // Clip left
    if (x < 0) {
        width += x;  // x is negative, so this shrinks width
        x = 0;
    }
    // Clip top
    if (y < 0) {
        height += y;
        y = 0;
    }

    // Clip right/bottom
    if (x + width > SCREENWIDTH)  width = SCREENWIDTH - x;
    if (y + height > SCREENHEIGHT) height = SCREENHEIGHT - y;

    if (width <= 0 || height <= 0) return;

    // DMA each row
    for (int row = 0; row < height; row++) {
        volatile u16* dest = (volatile u16*)&videoBuffer[OFFSET(x, y + row, SCREENWIDTH)];
        DMANow(3, &color, dest, width | DMA_SOURCE_FIXED | DMA_16);
    }
}

// Keep drawRectangle for compatibility, but make it safe (clipped)
void drawRectangle(int x, int y, int width, int height, volatile unsigned short color) {
    drawRectangleClipped(x, y, width, height, (u16)color);
}

// Faster fill screen function: DMA fixed-source color across full VRAM buffer
void fillScreen(volatile u16 color) {
    DMANow(3,
           (volatile void*)&color,
           (volatile void*)videoBuffer,
           (SCREENWIDTH * SCREENHEIGHT) | DMA_SOURCE_FIXED | DMA_DESTINATION_INCREMENT | DMA_16);
}

// Draw a single 6x8 character using the provided font bitmap
void drawChar(int x, int y, char ch, u16 color) {
    unsigned char code = (unsigned char)ch;
    if (code > 127) return;

    int glyphIndex = code * 48; // 6*8 = 48 bytes per glyph

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 6; c++) {
            unsigned char pixel = fontdata[glyphIndex + r * 6 + c];
            if (pixel) {
                setPixel(x + c, y + r, color);
            }
        }
    }
}

// Draw a null-terminated string. Supports '\n' newlines.
void drawString(int x, int y, const char* str, u16 color) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += 10;
            cx = x;
        } else {
            drawChar(cx, y, *str, color);
            cx += 6; // advance
        }
        str++;
    }
}

// Waits for the start of VBlank
void waitForVBlank() {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

// Axis-aligned rectangle collision
int collision(int xA, int yA, int widthA, int heightA,
              int xB, int yB, int widthB, int heightB) {
    return yA <= yB + heightB - 1
        && yA + heightA - 1 >= yB
        && xA <= xB + widthB - 1
        && xA + widthA - 1 >= xB;
}

// Immediately begins a DMA transfer using parameters
//void DMANow(int channel, volatile void* src, volatile void* dest, unsigned int ctrl) {
//  DMA[channel].ctrl = 0;      // turn off first
//  DMA[channel].src  = src;
//  DMA[channel].dest = dest;
//  DMA[channel].ctrl = ctrl | DMA_ON;
//}

void DMANow(int channel, const void* src, volatile void* dest, unsigned int ctrl) {
    DMA[channel].ctrl = 0;      // turn off first
    DMA[channel].src = src;
    DMA[channel].dest = dest;
    DMA[channel].ctrl = ctrl | DMA_ON;
}