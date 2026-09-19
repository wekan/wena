#include "../../client/features/labels/component.h"
#include "../../models/color.h"
#include <nuklear.h>

/* Semantic fake only; real Nuklear tests inspect the actual drawn colors. */
int wena_label_badge_render(struct nk_context *context, const char *name,
    const char *color)
{
    if (!context || !name || !wena_color_valid(color)) return 0;
    nk_label(context, name, NK_TEXT_LEFT);
    return nk_button_label(context, name);
}
