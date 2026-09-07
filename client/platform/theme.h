#ifndef WENA_NATIVE_THEME_H
#define WENA_NATIVE_THEME_H

struct nk_context;
struct nk_color;

/* Resolve exact shared WeKan color names; failures leave output unchanged. */
int wena_native_theme_color(const char *name, struct nk_color *color);

/* Apply a bounded light native palette using the shared canonical colors.
 * Call once after Nuklear context initialization. Does not change the font.
 * This is a native default, not full web-theme or component-style parity. */
int wena_native_theme_apply(struct nk_context *context);

#endif
