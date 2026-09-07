#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/platform/font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct nk_font_atlas atlas, empty;
    struct nk_font *font;
    const struct nk_font_glyph *glyph;
    const void *pixels;
    int width, height;
    unsigned int i;
    static const nk_rune samples[] = {65, 0xc4, 0xe4, 0xd6, 0xf6,
        0xc5, 0xe5, 0x3a9, 0x3b1, 0x416, 0x44f, 0x52f, 0x20ac};
    memset(&empty, 0, sizeof(empty));
    assert(wena_native_font_add(0, 14) == 0);
    assert(wena_native_font_add(&empty, 14) == 0);
    nk_font_atlas_init_default(&atlas);
    nk_font_atlas_begin(&atlas);
    assert(wena_native_font_add(&atlas, 0) == 0);
    assert(wena_native_font_add(&atlas, 65) == 0);
    assert(atlas.font_num == 0);
    font = wena_native_font_add(&atlas, 14);
    assert(font && atlas.font_num == 1);
    pixels = nk_font_atlas_bake(&atlas, &width, &height, NK_FONT_ATLAS_ALPHA8);
    assert(pixels && width > 0 && height > 0 && width <= 4096 && height <= 4096);
    nk_font_atlas_end(&atlas, nk_handle_id(1), 0);
    for (i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        glyph = nk_font_find_glyph(font, samples[i]);
        assert(glyph && glyph->codepoint == samples[i] && glyph->xadvance > 0);
    }
    assert(nk_font_find_glyph(font, 0x4e2d)->codepoint == '?');
    assert(nk_font_find_glyph(font, 0x627)->codepoint == '?');
    assert(font->handle.width(font->handle.userdata, 14, "\303\204\316\251\320\226", 6) > 0);
    nk_font_atlas_clear(&atlas);
    puts("native font: real atlas bake, Latin/Greek/Cyrillic glyphs, bounds and fallback passed");
    return 0;
}
