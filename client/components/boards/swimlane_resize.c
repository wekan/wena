#include "swimlane_resize.h"
#include "../common/wekan_look.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>

/* The bar below an expanded lane: press and drag it up or down to change the
 * lane's height, release to keep it, Escape to cancel. Handled even when the
 * bar is scrolled out of view, so a release is never lost. */
void wena_swimlane_resize_bar(struct nk_context *context,
                                     const WenaBoardLayout *layout,
                                     const WenaSwimlane *swimlane,
                                     unsigned int height)
{
    WenaSwimlaneResize *resize = layout->swimlane_resize;
    struct nk_rect bounds;
    struct nk_color color;
    int visible, hovered, mine;

    mine = resize->active && !strcmp(resize->swimlane_id, swimlane->id);
    /* WeKan's 10px resize handle: invisible until hovered or dragged. */
    nk_layout_row_dynamic(context, 10.0f, 1);
    visible = nk_widget(&bounds, context) != NK_WIDGET_INVALID;
    /* Named as WeKan's element, so tests and the inventory find it unseen. */
    if (visible) wena_ui_control_record("swimlane", "swimlane-resize-handle",
                                        bounds.x, bounds.y, bounds.w, bounds.h);
    hovered = visible && nk_input_is_mouse_hovering_rect(&context->input, bounds);
    if (hovered) resize->hovered = 1;
    if (!resize->active && hovered &&
        nk_input_is_mouse_pressed(&context->input, NK_BUTTON_LEFT) &&
        wena_model_set_required(resize->swimlane_id, sizeof(resize->swimlane_id),
                                swimlane->id)) {
        resize->active = 1; mine = 1;
        resize->start_y = context->input.mouse.pos.y;
        resize->start_height = resize->height = height;
    }
    if (mine) {
        if (nk_input_is_key_pressed(&context->input, NK_KEY_TEXT_RESET_MODE)) {
            resize->active = 0;
        } else if (context->input.mouse.buttons[NK_BUTTON_LEFT].down) {
            resize->height = wena_board_swimlane_height_clamp((long)resize->start_height +
                (long)(context->input.mouse.pos.y - resize->start_y));
        } else {
            (void)wena_board_swimlane_height_set(layout->collapse, layout,
                                                 swimlane->id, resize->height);
            resize->active = 0;
        }
    }
    if (visible && (hovered || mine)) {
        color = context->style.scrollv.cursor_hover.data.color;
        nk_fill_rect(nk_window_get_canvas(context),
            nk_rect(bounds.x, bounds.y + bounds.h / 2.0f - 2.0f, bounds.w, 4.0f), 2.0f, color);
    }
}
