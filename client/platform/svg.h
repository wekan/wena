/* SPDX-License-Identifier: MIT */
#ifndef WENA_NATIVE_SVG_H
#define WENA_NATIVE_SVG_H
#include <stddef.h>
struct nk_context;
struct nk_command_buffer;
struct nk_rect;
struct nk_color;
/* Build-time SVG conversion, no runtime XML or file loading. Paint -1 is none,
 * -2 uses caller theme ink; nonnegative values are packed RGB. */
typedef struct WenaSvgShape {
    int kind; /* 1 rounded rect, 2 circle, 3 line */
    float geometry[5];
    long fill;
    long stroke;
    float stroke_width;
} WenaSvgShape;
typedef struct WenaSvgAsset {
    float width, height;
    size_t count;
    const WenaSvgShape *shapes;
} WenaSvgAsset;
const WenaSvgAsset *wena_svg_board(void);
/* Aspect-preserving centered conversion to native vectors. Bounded, validated
 * before drawing; no texture allocation or DPI-specific bitmap cache. */
int wena_svg_draw(struct nk_command_buffer *canvas, const WenaSvgAsset *asset,
    const struct nk_rect *bounds, const struct nk_color *ink);
/* Native board header title decorator; text remains live in the
 * existing UI rather than becoming glyph paths embedded in SVG artwork. */
void wena_svg_board_title(struct nk_context *context, const char *title);
#endif
