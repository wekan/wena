#ifndef WENA_NATIVE_FONT_H
#define WENA_NATIVE_FONT_H
struct nk_font;
struct nk_font_atlas;
/* Trusted embedded static Roboto only; no file paths or untrusted font bytes.
 * Call after atlas_begin; NULL permits caller's default-font fallback.
 * Selected Latin/Greek/Cyrillic glyphs, not universal coverage or shaping. */
struct nk_font *wena_native_font_add(struct nk_font_atlas *atlas, float height);
/* The same, in Roboto Bold: WeKan's titles, list names and menu items. */
struct nk_font *wena_native_font_add_bold(struct nk_font_atlas *atlas, float height);
#endif
