#include "font.h"
#include <stddef.h>
#include "nuklear_options.h"
#include <nuklear.h>
#include "font_data.h"
static struct nk_font *wena_font_add(struct nk_font_atlas *atlas, float height,
                                     const unsigned char *data, size_t size)
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
    return nk_font_atlas_add_from_memory(atlas, (void *)data, size, height, &config);
}

struct nk_font *wena_native_font_add(struct nk_font_atlas *atlas, float height)
{
    return wena_font_add(atlas, height, wena_font_data, sizeof(wena_font_data));
}

struct nk_font *wena_native_font_add_bold(struct nk_font_atlas *atlas, float height)
{
    return wena_font_add(atlas, height, wena_font_bold_data, sizeof(wena_font_bold_data));
}
