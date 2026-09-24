#include "theme.h"
#include "svg_theme_data.h"

#include "../../imports/ui/page_contract.h"

#include <nuklear.h>
#include <string.h>

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int wena_native_theme_color(const char *name, struct nk_color *color)
{
    const WenaUiColorContract *colors;
    const char *rgb;
    size_t count, i, channel;
    int high, low;
    unsigned char bytes[3];
    if (name == NULL || color == NULL) return 0;
    colors = wena_ui_colors(&count);
    for (i = 0; i < count; ++i) {
        if (strcmp(colors[i].name, name) != 0) continue;
        rgb = colors[i].rgb;
        if (rgb == NULL || strlen(rgb) != 7 || rgb[0] != '#') return 0;
        for (channel = 0; channel < 3; ++channel) {
            high = hex_digit(rgb[1 + channel * 2]);
            low = hex_digit(rgb[2 + channel * 2]);
            if (high < 0 || low < 0) return 0;
            bytes[channel] = (unsigned char)(high * 16 + low);
        }
        *color = nk_rgba(bytes[0], bytes[1], bytes[2], 255);
        return 1;
    }
    return 0;
}

int wena_native_theme_apply(struct nk_context *context)
{
    struct nk_color palette[NK_COLOR_COUNT];
    struct nk_color accent, ink, paper, panel, navy, border;
    int i;
    if (context == NULL ||
        !wena_native_theme_color(WENA_SVG_THEME_ACCENT, &accent) ||
        !wena_native_theme_color(WENA_SVG_THEME_INK, &ink) ||
        !wena_native_theme_color(WENA_SVG_THEME_PAPER, &paper) ||
        !wena_native_theme_color(WENA_SVG_THEME_PANEL, &panel) ||
        !wena_native_theme_color(WENA_SVG_THEME_NAVY, &navy) ||
        !wena_native_theme_color(WENA_SVG_THEME_BORDER, &border)) return 0;
    /* The pinned WeKan boardColors.css supplies the blue accent and pale
       selected surface. White controls and dark text remain legible inside
       Nuklear's shared window/group background model. */
    for (i = 0; i < NK_COLOR_COUNT; ++i) palette[i] = paper;
    palette[NK_COLOR_TEXT] = ink;
    palette[NK_COLOR_WINDOW] = panel;
    palette[NK_COLOR_HEADER] = navy;
    palette[NK_COLOR_BORDER] = border;
    palette[NK_COLOR_BUTTON_HOVER] = panel;
    palette[NK_COLOR_BUTTON_ACTIVE] = panel;
    palette[NK_COLOR_TOGGLE_HOVER] = panel;
    palette[NK_COLOR_TOGGLE_CURSOR] = navy;
    palette[NK_COLOR_SELECT_ACTIVE] = navy;
    palette[NK_COLOR_SLIDER] = border;
    palette[NK_COLOR_SLIDER_CURSOR] = accent;
    palette[NK_COLOR_SLIDER_CURSOR_HOVER] = navy;
    palette[NK_COLOR_SLIDER_CURSOR_ACTIVE] = navy;
    palette[NK_COLOR_EDIT_CURSOR] = ink;
    palette[NK_COLOR_CHART_COLOR] = accent;
    palette[NK_COLOR_CHART_COLOR_HIGHLIGHT] = navy;
    palette[NK_COLOR_SCROLLBAR] = paper;
    palette[NK_COLOR_SCROLLBAR_CURSOR] = accent;
    palette[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = navy;
    palette[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = navy;
    palette[NK_COLOR_TAB_HEADER] = panel;
    palette[NK_COLOR_KNOB_CURSOR] = accent;
    palette[NK_COLOR_KNOB_CURSOR_HOVER] = navy;
    palette[NK_COLOR_KNOB_CURSOR_ACTIVE] = navy;
    nk_style_from_table(context, palette);
    context->style.window.header.label_normal = paper;
    context->style.window.header.label_hover = paper;
    context->style.window.header.label_active = paper;
    context->style.window.header.close_button.text_normal = paper;
    context->style.window.header.close_button.text_hover = paper;
    context->style.window.header.close_button.text_active = paper;
    context->style.window.header.minimize_button.text_normal = paper;
    context->style.window.header.minimize_button.text_hover = paper;
    context->style.window.header.minimize_button.text_active = paper;
    context->style.selectable.text_normal_active = paper;
    context->style.selectable.text_hover_active = paper;
    context->style.selectable.text_pressed_active = paper;
    context->style.window.padding = nk_vec2(WENA_SVG_THEME_WINDOW_PADDING_X, WENA_SVG_THEME_WINDOW_PADDING_Y);
    context->style.window.group_padding = nk_vec2(WENA_SVG_THEME_GROUP_PADDING_X, WENA_SVG_THEME_GROUP_PADDING_Y);
    context->style.window.spacing = nk_vec2(WENA_SVG_THEME_SPACING_X, WENA_SVG_THEME_SPACING_Y);
    context->style.button.padding = nk_vec2(WENA_SVG_THEME_BUTTON_PADDING_X, WENA_SVG_THEME_BUTTON_PADDING_Y);
    return 1;
}
