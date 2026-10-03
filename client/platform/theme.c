#include "theme.h"
#include "../components/common/wekan_look.h"
#include "svg_theme_data.h"

#include "../../imports/ui/page_contract.h"

#include "nuklear_options.h"
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

static struct nk_color wekan(WenaWekanColor which)
{
    int rgb = wena_wekan_rgb(which);
    return nk_rgb((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

int wena_native_theme_apply(struct nk_context *context)
{
    struct nk_color palette[NK_COLOR_COUNT];
    struct nk_color ink, paper, panel, border, button, header, header_text, scroll, scroll_hover, icon;
    int i;
    if (context == NULL) return 0;
    /* WeKan's board look (client/components/common/wekan_look.c, measured
     * from WeKan): panels #f7f7f7 like card details and the sidebar, title
     * bars like its popups, black text, white fields, slim gray scrollbars.
     * Buttons take WeKan's darker blue, its composer's "Add": white text on
     * its lighter #2980b9 would read at only 4:1. */
    ink = wekan(WENA_WEKAN_TEXT);
    paper = wekan(WENA_WEKAN_POPUP);
    panel = wekan(WENA_WEKAN_PANEL);
    border = wekan(WENA_WEKAN_POPUP_BORDER);
    button = wekan(WENA_WEKAN_BUTTON_ADD);
    header = wekan(WENA_WEKAN_POPUP_HEADER);
    header_text = wekan(WENA_WEKAN_POPUP_HEADER_TEXT);
    scroll = wekan(WENA_WEKAN_LIST_BORDER);
    scroll_hover = wekan(WENA_WEKAN_ICON);
    icon = wekan(WENA_WEKAN_ICON_ACTIVE);
    for (i = 0; i < NK_COLOR_COUNT; ++i) palette[i] = paper;
    palette[NK_COLOR_TEXT] = ink;
    palette[NK_COLOR_WINDOW] = panel;
    palette[NK_COLOR_HEADER] = header;
    palette[NK_COLOR_BORDER] = border;
    palette[NK_COLOR_BUTTON] = button;
    palette[NK_COLOR_BUTTON_HOVER] = button;
    palette[NK_COLOR_BUTTON_ACTIVE] = button;
    palette[NK_COLOR_TOGGLE] = paper;
    palette[NK_COLOR_TOGGLE_HOVER] = paper;
    palette[NK_COLOR_TOGGLE_CURSOR] = button;
    palette[NK_COLOR_SELECT] = paper;
    palette[NK_COLOR_SELECT_ACTIVE] = button;
    palette[NK_COLOR_SLIDER] = scroll;
    palette[NK_COLOR_SLIDER_CURSOR] = button;
    palette[NK_COLOR_SLIDER_CURSOR_HOVER] = button;
    palette[NK_COLOR_SLIDER_CURSOR_ACTIVE] = button;
    palette[NK_COLOR_PROPERTY] = paper;
    palette[NK_COLOR_EDIT] = paper;
    palette[NK_COLOR_EDIT_CURSOR] = ink;
    palette[NK_COLOR_COMBO] = paper;
    palette[NK_COLOR_CHART] = paper;
    palette[NK_COLOR_CHART_COLOR] = button;
    palette[NK_COLOR_CHART_COLOR_HIGHLIGHT] = icon;
    palette[NK_COLOR_SCROLLBAR] = panel;
    palette[NK_COLOR_SCROLLBAR_CURSOR] = scroll;
    palette[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = scroll_hover;
    palette[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = scroll_hover;
    palette[NK_COLOR_TAB_HEADER] = header;
    palette[NK_COLOR_KNOB] = paper;
    palette[NK_COLOR_KNOB_CURSOR] = button;
    palette[NK_COLOR_KNOB_CURSOR_HOVER] = button;
    palette[NK_COLOR_KNOB_CURSOR_ACTIVE] = button;
    nk_style_from_table(context, palette);
    context->style.window.header.label_normal = header_text;
    context->style.window.header.label_hover = header_text;
    context->style.window.header.label_active = header_text;
    context->style.window.header.close_button.text_normal = scroll_hover;
    context->style.window.header.close_button.text_hover = icon;
    context->style.window.header.close_button.text_active = icon;
    context->style.window.header.minimize_button.text_normal = scroll_hover;
    context->style.window.header.minimize_button.text_hover = icon;
    context->style.window.header.minimize_button.text_active = icon;
    context->style.button.text_normal = paper;
    context->style.button.text_hover = paper;
    context->style.button.text_active = paper;
    context->style.button.border_color = button;
    context->style.button.rounding = 4.0f;
    context->style.contextual_button.text_normal = ink;
    context->style.menu_button.text_normal = ink;
    context->style.edit.border_color = scroll_hover;
    context->style.edit.border = 1.0f;
    context->style.edit.rounding = 2.0f;
    context->style.property.border_color = scroll_hover;
    context->style.combo.border_color = scroll_hover;
    context->style.checkbox.border_color = scroll_hover;
    context->style.option.border_color = scroll_hover;
    context->style.selectable.text_normal_active = paper;
    context->style.selectable.text_hover_active = paper;
    context->style.selectable.text_pressed_active = paper;
    context->style.window.border_color = border;
    context->style.window.popup_border_color = border;
    context->style.window.group_border_color = border;
    context->style.window.rounding = 6.0f;
    context->style.window.scrollbar_size = nk_vec2(8.0f, 8.0f);
    context->style.scrollh.rounding = 4.0f;
    context->style.scrollv.rounding = 4.0f;
    context->style.scrollh.rounding_cursor = 4.0f;
    context->style.scrollv.rounding_cursor = 4.0f;
    context->style.window.padding = nk_vec2(WENA_SVG_THEME_WINDOW_PADDING_X, WENA_SVG_THEME_WINDOW_PADDING_Y);
    context->style.window.group_padding = nk_vec2(WENA_SVG_THEME_GROUP_PADDING_X, WENA_SVG_THEME_GROUP_PADDING_Y);
    context->style.window.spacing = nk_vec2(WENA_SVG_THEME_SPACING_X, WENA_SVG_THEME_SPACING_Y);
    context->style.button.padding = nk_vec2(WENA_SVG_THEME_BUTTON_PADDING_X, WENA_SVG_THEME_BUTTON_PADDING_Y);
    return 1;
}
