#include "aga_palette.h"

#include <string.h>

/* An ARGB8888 pixel is read as one unsigned int: 32 bits on every system
 * Wena builds for. */
typedef char wena_aga_pixel_is_32_bits[sizeof(unsigned int) == 4 ? 1 : -1];

#define EXACT_LIMIT (WENA_AGA_COLORS - 160)

static int cell_of(long rgb)
{
    return (int)(((rgb >> 9) & 0x7c00L) | ((rgb >> 6) & 0x3e0L) | ((rgb >> 3) & 0x1fL));
}

static int add(WenaAgaPalette *palette, long rgb)
{
    int i;
    unsigned char r = (unsigned char)((rgb >> 16) & 255), g = (unsigned char)((rgb >> 8) & 255),
                  b = (unsigned char)(rgb & 255);
    if (palette->count >= WENA_AGA_COLORS) return 0;
    for (i = 0; i < palette->count; ++i)
        if (palette->rgb[i][0] == r && palette->rgb[i][1] == g && palette->rgb[i][2] == b) return 0;
    palette->rgb[palette->count][0] = r;
    palette->rgb[palette->count][1] = g;
    palette->rgb[palette->count][2] = b;
    ++palette->count;
    return 1;
}

static long blend(long from, long to, int part, int parts)
{
    long out = 0;
    int shift;
    for (shift = 16; shift >= 0; shift -= 8) {
        long a = (from >> shift) & 255, b = (to >> shift) & 255;
        out |= ((a * (parts - part) + b * part + parts / 2) / parts) << shift;
    }
    return out;
}

void wena_aga_palette_init(WenaAgaPalette *palette, const long *exact, size_t exact_count)
{
    static const int cube[5] = {0, 64, 128, 191, 255};
    size_t i;
    int r, g, b, step;
    long first = -1;
    memset(palette, 0, sizeof(*palette));
    for (i = 0; exact != NULL && i < exact_count && palette->count < EXACT_LIMIT; ++i) {
        if (exact[i] < 0 || exact[i] > 0xffffffL) continue;
        if (add(palette, exact[i]) && first < 0) first = exact[i];
    }
    /* Greys for anti-aliased text on WeKan's grey and white. */
    for (step = 0; step <= 32; ++step) add(palette, (long)(step * 255 / 32) * 0x010101L);
    /* White text on the first exact color - the header - fades through it. */
    if (first >= 0)
        for (step = 1; step < 4; ++step) add(palette, blend(first, 0xffffffL, step, 4));
    for (r = 0; r < 5; ++r)
        for (g = 0; g < 5; ++g)
            for (b = 0; b < 5; ++b) add(palette, ((long)cube[r] << 16) | ((long)cube[g] << 8) | cube[b]);
    /* Each entry owns its cell, the earlier (WeKan's) first. */
    for (r = 0; r < palette->count; ++r) {
        int cell = cell_of(((long)palette->rgb[r][0] << 16) | ((long)palette->rgb[r][1] << 8) | palette->rgb[r][2]);
        if (palette->known[cell >> 3] & (1 << (cell & 7))) continue;
        palette->known[cell >> 3] = (unsigned char)(palette->known[cell >> 3] | (1 << (cell & 7)));
        palette->map[cell] = (unsigned char)r;
    }
}

/* Nearest by weighted distance (green counts most, as the eye sees it),
 * from the middle of the cell. */
static unsigned char nearest(const WenaAgaPalette *palette, int cell)
{
    long r = ((cell >> 10) & 31) * 8 + 4, g = ((cell >> 5) & 31) * 8 + 4, b = (cell & 31) * 8 + 4;
    long best = -1;
    int i, chosen = 0;
    for (i = 0; i < palette->count; ++i) {
        long dr = r - palette->rgb[i][0], dg = g - palette->rgb[i][1], db = b - palette->rgb[i][2];
        long distance = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
        if (best < 0 || distance < best) { best = distance; chosen = i; }
    }
    return (unsigned char)chosen;
}

static unsigned char lookup(WenaAgaPalette *palette, int cell)
{
    if (!(palette->known[cell >> 3] & (1 << (cell & 7)))) {
        palette->map[cell] = nearest(palette, cell);
        palette->known[cell >> 3] = (unsigned char)(palette->known[cell >> 3] | (1 << (cell & 7)));
    }
    return palette->map[cell];
}

unsigned char wena_aga_palette_index(WenaAgaPalette *palette, long rgb)
{
    return lookup(palette, cell_of(rgb & 0xffffffL));
}

void wena_aga_palette_convert(WenaAgaPalette *palette, const void *pixels, size_t pitch,
                              int width, int height, unsigned char *out, size_t out_pitch)
{
    int x, y;
    for (y = 0; y < height; ++y) {
        const unsigned int *row = (const unsigned int *)(const void *)((const unsigned char *)pixels + (size_t)y * pitch);
        unsigned char *target = out + (size_t)y * out_pitch;
        unsigned int last = 0;
        unsigned char last_index = lookup(palette, 0);
        for (x = 0; x < width; ++x) {
            unsigned int pixel = row[x] & 0xffffffU;
            /* Flat fills repeat one color: no lookup for the run. */
            if (pixel != last) {
                last = pixel;
                last_index = lookup(palette, cell_of((long)pixel));
            }
            target[x] = last_index;
        }
    }
}
