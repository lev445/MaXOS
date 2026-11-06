/* DO NOT EDIT */
#ifndef MAXOS_FONT_H
#define MAXOS_FONT_H
#include <stdint.h>
/* 8x16 bitmap font, 256 glyphs, row-major, MSB = leftmost pixel */
extern const uint8_t font8x16[256][16];
#define FONT_W 8
#define FONT_H 16
#endif
