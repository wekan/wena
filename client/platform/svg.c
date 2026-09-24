/* SPDX-License-Identifier: MIT */
#include "svg.h"
#include <nuklear.h>
#include <string.h>
#include "svg_data.h"

const WenaSvgAsset *wena_svg_board(void) { return &wena_board_svg; }

static int finite_bound(float value, float low, float high)
{
    return value >= low && value <= high;
}
static int paint_valid(long paint)
{
    return paint >= -2L && paint <= 16777215L;
}
static struct nk_color paint_color(long paint, struct nk_color ink)
{
    if (paint == -2L) return ink;
    return nk_rgb((int)((paint >> 16) & 255L), (int)((paint >> 8) & 255L),
        (int)(paint & 255L));
}
int wena_svg_draw(struct nk_command_buffer *canvas, const WenaSvgAsset *asset,
    const struct nk_rect *bounds, const struct nk_color *ink)
{
    float scale, x, y, left, top, right, bottom, half;
    size_t index, field;
    const WenaSvgShape *shape;
    struct nk_rect r;
    if (!canvas || !asset || !bounds || !ink || !asset->shapes ||
        !asset->count || asset->count > 128u ||
        !finite_bound(asset->width, 0.01f, 4096.0f) ||
        !finite_bound(asset->height, 0.01f, 4096.0f) ||
        !finite_bound(bounds->x, -16000.0f, 16000.0f) ||
        !finite_bound(bounds->y, -16000.0f, 16000.0f) ||
        !finite_bound(bounds->w, 0.01f, 4096.0f) ||
        !finite_bound(bounds->h, 0.01f, 4096.0f)) return 0;
    /* Validate the complete command list before publishing any drawing. */
    for (index = 0; index < asset->count; ++index) {
        shape = &asset->shapes[index];
        if (shape->kind < 1 || shape->kind > 3 ||
            !paint_valid(shape->fill) || !paint_valid(shape->stroke) ||
            !finite_bound(shape->stroke_width, 0.01f, 4096.0f)) return 0;
        for (field = 0; field < 5; ++field)
            if (!finite_bound(shape->geometry[field], 0.0f, 4096.0f)) return 0;
        if (shape->kind != 3 && (shape->geometry[2] <= 0 || shape->geometry[3] <= 0 ||
            shape->geometry[0] + shape->geometry[2] > asset->width ||
            shape->geometry[1] + shape->geometry[3] > asset->height ||
            shape->geometry[4] > shape->geometry[2] / 2 ||
            shape->geometry[4] > shape->geometry[3] / 2)) return 0;
        if (shape->kind == 3 && (shape->geometry[0] > asset->width ||
            shape->geometry[2] > asset->width || shape->geometry[1] > asset->height ||
            shape->geometry[3] > asset->height)) return 0;
        if (shape->kind == 2 && shape->geometry[2] != shape->geometry[3]) return 0;
        left = shape->geometry[0]; top = shape->geometry[1];
        right = shape->kind == 3 ? shape->geometry[2] : left + shape->geometry[2];
        bottom = shape->kind == 3 ? shape->geometry[3] : top + shape->geometry[3];
        if (left > right) { half = left; left = right; right = half; }
        if (top > bottom) { half = top; top = bottom; bottom = half; }
        if (shape->stroke != -1L) {
            half = shape->stroke_width * 0.5f;
            if (left < half || top < half || right + half > asset->width ||
                bottom + half > asset->height) return 0;
        }
    }
    scale = bounds->w / asset->width;
    if (bounds->h / asset->height < scale) scale = bounds->h / asset->height;
    x = bounds->x + (bounds->w - asset->width * scale) * 0.5f;
    y = bounds->y + (bounds->h - asset->height * scale) * 0.5f;
    for (index = 0; index < asset->count; ++index) {
        shape = &asset->shapes[index];
        r = nk_rect(x + shape->geometry[0]*scale, y + shape->geometry[1]*scale,
            shape->geometry[2]*scale, shape->geometry[3]*scale);
        if (shape->kind == 3) {
            if (shape->stroke != -1L) nk_stroke_line(canvas, r.x, r.y,
                x + r.w, y + r.h, shape->stroke_width*scale, paint_color(shape->stroke, *ink));
        } else {
            if (shape->fill != -1L) {
                if (shape->kind == 1) nk_fill_rect(canvas, r, shape->geometry[4]*scale,
                    paint_color(shape->fill, *ink));
                else nk_fill_circle(canvas, r, paint_color(shape->fill, *ink));
            }
            if (shape->stroke != -1L) {
                if (shape->kind == 1) nk_stroke_rect(canvas, r, shape->geometry[4]*scale,
                    shape->stroke_width*scale, paint_color(shape->stroke, *ink));
                else nk_stroke_circle(canvas, r, shape->stroke_width*scale,
                    paint_color(shape->stroke, *ink));
            }
        }
    }
    return 1;
}

void wena_svg_board_title(struct nk_context *context, const char *title)
{
    struct nk_rect bounds, icon;
    struct nk_command_buffer *canvas;
    float size;
    if (!context || !title || !context->style.font) return;
    if (!nk_widget(&bounds, context)) return;
    canvas = nk_window_get_canvas(context);
    size = context->style.font->height * 1.5f;
    if (size > bounds.h) size = bounds.h;
    if (bounds.w > size * 2) {
        icon = nk_rect(bounds.x, bounds.y + (bounds.h-size)*0.5f, size, size);
        (void)wena_svg_draw(canvas, wena_svg_board(), &icon, &context->style.text.color);
        bounds.x += size + 6; bounds.w -= size + 6;
    }
    bounds.y += (bounds.h - context->style.font->height) * 0.5f;
    bounds.h = context->style.font->height;
    nk_draw_text(canvas, bounds, title, (int)strlen(title), context->style.font,
        context->style.window.background, context->style.text.color);
}
