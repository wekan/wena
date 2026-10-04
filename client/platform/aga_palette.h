#ifndef WENA_AGA_PALETTE_H
#define WENA_AGA_PALETTE_H
/* The AmigaOS 3 AGA build's 256 colors, and its 32-bit frame mapped onto
 * them. AGA shows 8 bits per pixel from a palette; Wena draws in 32 bits.
 *
 * The palette is WeKan's own colors first, exact - the header blue, the
 * greys of lists and cards, label and board colors - then a grey ramp for
 * anti-aliased text, then a 5x5x5 color cube for everything else.
 *
 * A pixel maps through its 15-bit color (5 bits a channel) to the nearest
 * entry, found the first time that color is seen and remembered: working
 * out all 32768 at the start would take seconds on a 68040. Every palette
 * color owns its own 15-bit cell, WeKan's first, so a flat UI color always
 * comes out exactly as it went in. */
#include <stddef.h>

#define WENA_AGA_COLORS 256
#define WENA_AGA_CELLS 32768

typedef struct WenaAgaPalette {
    unsigned char rgb[WENA_AGA_COLORS][3];
    int count;                              /* entries used; the rest black */
    unsigned char map[WENA_AGA_CELLS];
    unsigned char known[WENA_AGA_CELLS / 8];
} WenaAgaPalette;

/* exact: 0xRRGGBB colors to keep exactly, in priority order (duplicates
 * are skipped); at most WENA_AGA_COLORS - 160 of them are taken. */
void wena_aga_palette_init(WenaAgaPalette *palette, const long *exact, size_t exact_count);
/* The entry for 0xRRGGBB. */
unsigned char wena_aga_palette_index(WenaAgaPalette *palette, long rgb);
/* Rows of ARGB8888 pixels (32-bit, native order, `pitch` bytes apart) into
 * rows of entries (`out_pitch` bytes apart). */
void wena_aga_palette_convert(WenaAgaPalette *palette, const void *pixels, size_t pitch,
                              int width, int height, unsigned char *out, size_t out_pitch);

#endif
