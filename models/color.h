#ifndef WENA_MODEL_COLOR_H
#define WENA_MODEL_COLOR_H

#include <stddef.h>

#define WENA_COLOR_CAPACITY 16

typedef struct WenaColorContract {
    const char *name;
    const char *rgb;
} WenaColorContract;

/* The canonical 25 item colors. Names are case sensitive. */
const WenaColorContract *wena_colors(size_t *count);
/* All 25 board themes followed by all 25 item colors. */
const WenaColorContract *wena_color_contracts(size_t *count);
/* Item names, empty (white), or exactly #RRGGBB; no theme names. */
int wena_color_valid(const char *text);
/* Both functions leave the output unchanged on invalid input. */
int wena_color_rgb(const char *text, unsigned char rgb[3]);
/* Choose black/white by quantized sRGB relative luminance, without libm. */
int wena_color_foreground(const char *text, unsigned char rgb[3]);

#endif
