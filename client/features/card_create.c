#include "card_create.h"
#include "../components/lists/list_header.h"
#include "../components/common/wekan_look.h"
#include "../../imports/ui/page_contract.h"
#include "../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>

static int scope_valid(const WenaCardCreateState *state,
    const WenaBoardLayout *layout)
{
    size_t i;
    int list_found, lane_found;
    if (layout == NULL || layout->board == NULL || layout->board->archived ||
        strcmp(state->board_id, layout->board->id) != 0 ||
        (layout->list_count != 0 && layout->lists == NULL) ||
        (layout->swimlane_count != 0 && layout->swimlanes == NULL)) return 0;
    list_found = 0; lane_found = 0;
    for (i = 0; i < layout->list_count; ++i) {
        const WenaList *list;
        list = &layout->lists[i];
        if (!list->archived && strcmp(list->id, state->list_id) == 0 &&
            strcmp(list->board_id, state->board_id) == 0 &&
            (list->swimlane_id[0] == '\0' ||
             strcmp(list->swimlane_id, state->swimlane_id) == 0)) list_found = 1;
    }
    for (i = 0; i < layout->swimlane_count; ++i) {
        const WenaSwimlane *lane;
        lane = &layout->swimlanes[i];
        if (!lane->archived && strcmp(lane->id, state->swimlane_id) == 0 &&
            strcmp(lane->board_id, state->board_id) == 0) lane_found = 1;
    }
    return list_found && lane_found;
}

void wena_card_create_init(WenaCardCreateState *state,
    WenaCardCreateApply apply, void *context)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->apply = apply;
    state->context = context;
}

void wena_card_create_close(WenaCardCreateState *state)
{
    if (state == NULL) return;
    state->visible = 0;
    state->error = 0;
    state->title_length = 0;
    state->title_input[0] = '\0';
    state->board_id[0] = '\0';
    state->list_id[0] = '\0';
    state->swimlane_id[0] = '\0';
}

int wena_card_create_open(WenaCardCreateState *state,
    const WenaBoardLayout *layout, const WenaListInteraction *interaction)
{
    if (state == NULL || interaction == NULL ||
        (interaction->actions & WENA_LIST_HEADER_ADD_CARD) == 0u) return 0;
    wena_card_create_close(state);
    if (!wena_model_set_required(state->board_id, sizeof(state->board_id),
                                 interaction->board_id) ||
        !wena_model_set_required(state->list_id, sizeof(state->list_id),
                                 interaction->list_id) ||
        !wena_model_set_required(state->swimlane_id, sizeof(state->swimlane_id),
                                 interaction->swimlane_id) || !scope_valid(state, layout)) {
        wena_card_create_close(state);
        return 0;
    }
    state->visible = 1;
    state->bottom = (interaction->actions & WENA_LIST_HEADER_ADD_CARD_BOTTOM) != 0u;
    state->focus = 1;
    return 1;
}

/* Creates the card from the input; 1 when it was created (and, for the top,
 * moved there). A failure keeps the input and shows the error. */
static int submit(WenaCardCreateState *state, const WenaBoardLayout *layout)
{
    if (state->title_length < 0 ||
        !wena_card_details_title_valid(state->title_input, (size_t)state->title_length)) {
        state->error = 1;
        return 0;
    }
    state->title_input[state->title_length] = '\0';
    if (state->apply == NULL || !state->apply(state->context, state->board_id, state->list_id,
                                              state->swimlane_id, state->title_input)) {
        state->error = 1;
        return 0;
    }
    if (!state->bottom && state->to_top != NULL && !state->to_top(state->to_top_context, layout)) {
        /* Created, but left last: say so rather than pretend. */
        state->error = 1;
        state->title_length = 0;
        state->title_input[0] = '\0';
        return 0;
    }
    return 1;
}

int wena_card_create_render(struct nk_context *context,
    WenaCardCreateState *state, const WenaBoardLayout *layout,
    float width, float height)
{
    int close_requested;
    int save_clicked;
    int cancel_clicked;
    unsigned int edit_keys;
    if (state == NULL || !state->visible) return 0;
    if (!scope_valid(state, layout)) {
        wena_card_create_close(state);
        return 0;
    }
    if (context == NULL || width <= 0 || height <= 0) return 0;
    close_requested = 0;
    if (nk_begin_titled(context, "Add card", wena_ui_control_text(WENA_UI_ADD_CARD), nk_rect(width * 0.5f, 0,
        width * 0.5f, height), NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(context, 32.0f, 1);
        nk_label(context, wena_ui_control_text(WENA_UI_ADD_CARD), NK_TEXT_LEFT);
        edit_keys = wena_title_input_keys(context,
            nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                state->title_input, &state->title_length,
                (int)sizeof(state->title_input), nk_filter_default));
        nk_layout_row_dynamic(context, 28.0f, 2);
        save_clicked = nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE));
        cancel_clicked = nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL));
        if (cancel_clicked || (edit_keys & WENA_TITLE_INPUT_CANCEL) != 0u)
            close_requested = 1;
        else if (save_clicked || (edit_keys & WENA_TITLE_INPUT_COMMIT) != 0u)
            close_requested = submit(state, layout);
        nk_layout_row_dynamic(context, 28.0f, 1);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE)))
            close_requested = 1;
        if (state->error) {
                nk_layout_row_dynamic(context, 48.0f, 1);
                nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
            }
    }
    nk_end(context);
    if (close_requested) wena_card_create_close(state);
    return 1;
}

int wena_card_create_render_inline(struct nk_context *context,
    WenaCardCreateState *state, const WenaBoardLayout *layout,
    const WenaList *list, const WenaSwimlane *lane, int bottom)
{
    struct nk_rect bounds;
    unsigned int edit_keys;
    int close_requested, add_clicked;
    if (context == NULL || state == NULL || !state->visible || list == NULL || lane == NULL ||
        (state->bottom != 0) != (bottom != 0) || strcmp(state->list_id, list->id) != 0 ||
        strcmp(state->swimlane_id, lane->id) != 0) return 0;
    if (!scope_valid(state, layout)) {
        wena_card_create_close(state);
        return 0;
    }
    /* The card to be: a white minicard, where the new one will be. */
    nk_layout_row_begin(context, NK_STATIC, 58.0f, 2);
    nk_layout_row_push(context, 12.0f);
    nk_spacing(context, 1);
    nk_layout_row_push(context, 238.0f);
    bounds = nk_widget_bounds(context);
    wena_wekan_fill(context, bounds.x, bounds.y + 2.0f, bounds.w, bounds.h, WENA_WEKAN_MINICARD_SHADOW, 7.0f);
    wena_wekan_fill(context, bounds.x, bounds.y, bounds.w, bounds.h, WENA_WEKAN_MINICARD, 7.0f);
    nk_style_push_style_item(context, &context->style.edit.normal, nk_style_item_color(nk_rgba(0, 0, 0, 0)));
    nk_style_push_style_item(context, &context->style.edit.hover, nk_style_item_color(nk_rgba(0, 0, 0, 0)));
    nk_style_push_style_item(context, &context->style.edit.active, nk_style_item_color(nk_rgba(0, 0, 0, 0)));
    nk_style_push_float(context, &context->style.edit.border, 0.0f);
    if (state->focus) {
        nk_edit_focus(context, NK_EDIT_BOX | NK_EDIT_SIG_ENTER);
        state->focus = 0;
    }
    wena_ui_control_record(NULL, wena_ui_text(bottom ? WENA_UI_TEXT_ADD_CARD_BOTTOM : WENA_UI_TEXT_ADD_CARD_TOP),
                           bounds.x, bounds.y, bounds.w, bounds.h);
    /* Enter adds, as in WeKan; Escape closes. */
    edit_keys = wena_title_input_keys(context,
        nk_edit_string(context, NK_EDIT_BOX | NK_EDIT_SIG_ENTER, state->title_input,
                       &state->title_length, (int)sizeof(state->title_input), nk_filter_default));
    nk_style_pop_float(context);
    nk_style_pop_style_item(context);
    nk_style_pop_style_item(context);
    nk_style_pop_style_item(context);
    nk_layout_row_end(context);
    /* "Add" and the cross that closes it. */
    nk_layout_row_begin(context, NK_STATIC, 30.0f, 3);
    nk_layout_row_push(context, 12.0f);
    nk_spacing(context, 1);
    nk_layout_row_push(context, 64.0f);
    add_clicked = wena_wekan_button(context, wena_ui_text(WENA_UI_TEXT_ADD), WENA_WEKAN_BUTTON_ADD);
    nk_layout_row_push(context, 28.0f);
    close_requested = wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_CLOSE),
                                             16.0f, WENA_WEKAN_ICON);
    nk_layout_row_end(context);
    if (state->error) {
        nk_layout_row_dynamic(context, 40.0f, 1);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
    }
    if ((edit_keys & WENA_TITLE_INPUT_CANCEL) != 0u) close_requested = 1;
    else if (add_clicked || (edit_keys & WENA_TITLE_INPUT_COMMIT) != 0u) {
        if (submit(state, layout)) {
            /* WeKan keeps the composer open for the next card. */
            state->title_length = 0;
            state->title_input[0] = '\0';
            state->error = 0;
            state->focus = 1;
        }
    }
    if (close_requested) wena_card_create_close(state);
    return 1;
}
