#ifndef GBA_MODE4_H
#define GBA_MODE4_H

#include "gba.h"

// Mode 4 uses 2 page buffers in VRAM.
// Each u16 contains two pixels (low byte = even x, high byte = odd x).
#define FRONTBUFFER ((u16*)0x06000000) // page 0
#define BACKBUFFER  ((u16*)0x0600A000) // page 1

// Display control bit for page flipping (REG_DISPCTL)
#define DISP_BACKBUFFER (1 << 4)

// Page flipping
void flipPage(void);

// Core drawing
void setPixel4(int x, int y, u8 colorIndex);
void drawRect4(int x, int y, int width, int height, u8 colorIndex);
void fillScreen4(u8 colorIndex);

// Images (8bpp, packed 2 pixels per u16)
void drawImage4(int x, int y, int width, int height, const u16 *image);
void drawFullscreenImage4(const u16 *image);

// Draw a sub-rectangle from a larger 8bpp image (same packed format).
// Useful when you only have a fullscreen image but need a "sprite" too.
void drawSubImage4(int x, int y, int width, int height,
                   const u16 *image, int imageWidth,
                   int srcX, int srcY);

// Text
void drawChar4(int x, int y, char ch, u8 colorIndex);
void drawString4(int x, int y, char *str, u8 colorIndex);

#endif
