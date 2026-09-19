#include "component.h"
#include "../../../models/color.h"
#include <nuklear.h>

int wena_label_badge_render(struct nk_context *context, const char *name,
    const char *color)
{
    struct nk_style_button style;
    struct nk_color background, foreground;
    unsigned char rgb[3], text[3];
    if (!context || !name || !wena_color_rgb(color, rgb) ||
        !wena_color_foreground(color, text)) return 0;
    background = nk_rgb(rgb[0], rgb[1], rgb[2]);
    foreground = nk_rgb(text[0], text[1], text[2]);
    style = context->style.button;
    style.normal = nk_style_item_color(background);
    style.hover = nk_style_item_color(background);
    style.active = nk_style_item_color(background);
    style.text_background = background;
    style.text_normal = foreground;
    style.text_hover = foreground;
    style.text_active = foreground;
    return nk_button_label_styled(context, &style, name);
}
