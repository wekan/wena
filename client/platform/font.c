#include "font.h"
#define NK_INCLUDE_FONT_BAKING
#include <nuklear.h>
#include "font_data.h"
struct nk_font *wena_native_font_add(struct nk_font_atlas *atlas, float height)
{
    struct nk_font_config config;
    if (!atlas || !atlas->temporary.alloc || !atlas->temporary.free ||
        !atlas->permanent.alloc || !atlas->permanent.free ||
        !(height >= 8.0f && height <= 64.0f)) return 0;
    config = nk_font_config(height);
    config.range = wena_font_ranges;
    config.fallback_glyph = '?';
    config.oversample_h = 1;
    config.oversample_v = 1;
    return nk_font_atlas_add_from_memory(atlas, (void *)wena_font_data,
        sizeof(wena_font_data), height, &config);
}
