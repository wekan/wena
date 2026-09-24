#include "color_input.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

int wena_color_input_set(WenaColorInput *state, const char *color)
{
    unsigned char rgb[3];
    if (!state || !wena_color_rgb(color, rgb)) return 0;
    memmove(state->color, color, strlen(color)+1);
    sprintf(state->custom_color, "#%02x%02x%02x", (unsigned int)rgb[0],
        (unsigned int)rgb[1], (unsigned int)rgb[2]);
    state->color_length = 7;
    state->use_custom_color = 0;
    return 1;
}

const char *wena_color_input_value(WenaColorInput *state)
{
    if (!state) return NULL;
    if (!state->use_custom_color) return memchr(state->color,0,sizeof(state->color)) &&
        wena_color_valid(state->color) ? state->color : NULL;
    if (state->color_length != 7 || state->custom_color[0] != '#') return NULL;
    state->custom_color[7] = 0;
    return wena_color_valid(state->custom_color) ? state->custom_color : NULL;
}

void wena_color_input_render(struct nk_context *context,WenaColorInput *state,
    const char *preview,WenaColorInputSwatch swatch)
{
    const WenaColorContract *colors;
    size_t index, count;
    char previous[sizeof(state->custom_color)];
    int previous_length;
    if(!context||!state||!preview||!swatch||state->color_length<0||
        state->color_length>(int)sizeof(state->custom_color))return;
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_SELECT_COLOR), NK_TEXT_LEFT);
    colors = wena_colors(&count);
    nk_layout_row_dynamic(context, 22, 5);
    for (index = 0; index < count; ++index)
        if (swatch(context, colors[index].name, colors[index].name))
            (void)wena_color_input_set(state, colors[index].name);
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_CUSTOM_COLOR), NK_TEXT_LEFT);
    memcpy(previous, state->custom_color, sizeof(previous));
    previous_length = state->color_length;
    /* One extra byte detects overlong hex input. Enter in the color field does
     * not submit a form or a deletion confirmation. */
    (void)nk_edit_string(context, NK_EDIT_FIELD, state->custom_color,
        &state->color_length, (int)sizeof(state->custom_color), nk_filter_default);
    if (previous_length != state->color_length ||
        memcmp(previous, state->custom_color, sizeof(previous)))
        state->use_custom_color = 1;
    if (wena_color_input_value(state)) {
        nk_layout_row_dynamic(context, 24, 1);
        (void)swatch(context, preview, wena_color_input_value(state));
    }
}

