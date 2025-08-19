#ifndef FONT_H
#define FONT_H

#include <gba_types.h>

// 8x8 Bitmap Font System for GBA
// Each character is represented as 8 bytes, where each byte represents a row of pixels
// Bit 7 (0x80) is the leftmost pixel, bit 0 (0x01) is unused for 8-pixel width

// Simple 8x8 bitmap font data for printable ASCII characters (32-126)
extern const u8 font_data[95][8];

// Renders text to a 16-bit color buffer using the bitmap font
// Parameters:
//   buffer: Pointer to 16-bit video buffer (240x160 pixels)
//   x, y: Starting position for text (top-left corner)
//   text: Null-terminated string to render
//   color: 16-bit color value (RGB555 format)
void draw_text(u16* buffer, int x, int y, const char* text, u16 color);

#endif // FONT_H