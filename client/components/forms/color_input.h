#ifndef WENA_COLOR_INPUT_H
#define WENA_COLOR_INPUT_H
#include "../../../models/color.h"
struct nk_context;
typedef int (*WenaColorInputSwatch)(struct nk_context *context,const char *label,const char *color);
typedef struct WenaColorInput {
    char color[WENA_COLOR_CAPACITY+1];
    /* Overflow byte rejects a pasted eighth character instead of saving a prefix. */
    char custom_color[9];
    int color_length;
    int use_custom_color;
} WenaColorInput;
/* Set preserves the state on invalid input. Empty is the canonical default.
 * Value is borrowed from state; NULL means an invalid/incomplete draft. */
int wena_color_input_set(WenaColorInput *state,const char *color);
const char *wena_color_input_value(WenaColorInput *state);
/* Pure UI: never persists or submits. Caller provides its reusable swatch and
 * preview label. Enter in the color field never commits the enclosing form. */
void wena_color_input_render(struct nk_context *context,WenaColorInput *state,
    const char *preview,WenaColorInputSwatch swatch);
#endif
