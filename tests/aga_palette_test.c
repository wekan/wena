/* The AGA build's palette: WeKan's colors exact, every color near. */
#include "../client/platform/aga_palette.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long rgb_of(const WenaAgaPalette *p, int i)
{
    return ((long)p->rgb[i][0] << 16) | ((long)p->rgb[i][1] << 8) | p->rgb[i][2];
}

static int channel_error(long a, long b)
{
    int shift, worst = 0;
    for (shift = 0; shift <= 16; shift += 8) {
        int d = (int)((a >> shift) & 255) - (int)((b >> shift) & 255);
        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }
    return worst;
}

int main(void)
{
    /* WeKan's header, body, list header, card text, a label, and the
     * header again (a duplicate). */
    static const long wekan[] = {0x2980b9L, 0xdededeL, 0xe4e4e4L, 0x4d4d4dL, 0xeb4646L, 0x2980b9L,
                                 -1L, 0x1000000L};
    static WenaAgaPalette palette, crowded;
    static long many[300];
    unsigned int pixels[3][5];
    unsigned char out[3][8];
    size_t i;
    int cell, entry;

    wena_aga_palette_init(&palette, wekan, sizeof(wekan) / sizeof(wekan[0]));
    assert(palette.count > 150 && palette.count <= WENA_AGA_COLORS);
    /* Exact first, in order, the duplicate and the out-of-range ones skipped. */
    assert(rgb_of(&palette, 0) == 0x2980b9L && rgb_of(&palette, 1) == 0xdededeL);
    assert(rgb_of(&palette, 4) == 0xeb4646L && rgb_of(&palette, 5) == 0x000000L);
    /* WeKan's colors come out exactly; so does every entry of its own. */
    for (i = 0; i < 5; ++i)
        assert(rgb_of(&palette, wena_aga_palette_index(&palette, wekan[i])) == wekan[i]);
    assert(rgb_of(&palette, wena_aga_palette_index(&palette, 0xffffffL)) == 0xffffffL);
    assert(rgb_of(&palette, wena_aga_palette_index(&palette, 0x000000L)) == 0x000000L);
    /* Near colors go to the near WeKan color. */
    assert(rgb_of(&palette, wena_aga_palette_index(&palette, 0x2a81baL)) == 0x2980b9L);
    /* Every one of the 32768 cells is near: within the cube's half step,
     * plus what the eye-weighted distance trades between channels (a dark
     * blue may go to a grey). */
    for (cell = 0; cell < WENA_AGA_CELLS; ++cell) {
        long color = ((long)((cell >> 10) & 31) * 8 + 4) << 16 | ((long)((cell >> 5) & 31) * 8 + 4) << 8 |
                     ((cell & 31) * 8 + 4);
        entry = wena_aga_palette_index(&palette, color);
        assert(entry < palette.count);
        assert(channel_error(rgb_of(&palette, entry), color) <= 60);
    }
    /* Rows of ARGB with alpha, a padded source pitch and a padded target. */
    memset(out, 0xee, sizeof(out));
    for (i = 0; i < 3; ++i) {
        pixels[i][0] = 0xff2980b9U; pixels[i][1] = 0xff2980b9U; pixels[i][2] = 0x00dededeU;
        pixels[i][3] = 0x80ffffffU; pixels[i][4] = 0xdeadbeefU;
    }
    wena_aga_palette_convert(&palette, pixels, sizeof(pixels[0]), 4, 3, &out[0][0], sizeof(out[0]));
    for (i = 0; i < 3; ++i) {
        assert(out[i][0] == 0 && out[i][1] == 0 && out[i][2] == 1);
        assert(rgb_of(&palette, out[i][3]) == 0xffffffL);
        assert(out[i][4] == 0xee && out[i][7] == 0xee); /* past the width: untouched */
    }
    /* Negative: no WeKan colors still gives greys and the cube; too many
     * are capped so the greys and cube keep their room. */
    wena_aga_palette_init(&palette, NULL, 0);
    assert(rgb_of(&palette, 0) == 0x000000L && palette.count > 150);
    for (i = 0; i < 300; ++i) many[i] = (long)(i * 7919L) & 0xffffffL;
    wena_aga_palette_init(&crowded, many, 300);
    assert(crowded.count > 240 && crowded.count <= WENA_AGA_COLORS);
    assert(rgb_of(&crowded, wena_aga_palette_index(&crowded, 0xffffffL)) == 0xffffffL);
    assert(rgb_of(&crowded, wena_aga_palette_index(&crowded, 0xff0000L)) == 0xff0000L);
    puts("AGA palette tests passed");
    return 0;
}
