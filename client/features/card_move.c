#include "card_move.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static int layout_valid(const WenaBoardLayout *layout)
{
    return layout != NULL && layout->board != NULL && !layout->board->archived &&
        (layout->card_count == 0 || layout->cards != NULL) &&
        (layout->list_count == 0 || layout->lists != NULL) &&
        (layout->swimlane_count == 0 || layout->swimlanes != NULL);
}

static const WenaCard *source_card(const WenaCardMoveState *state,
    const WenaBoardLayout *layout)
{
    size_t i;
    if (!layout_valid(layout) || strcmp(layout->board->id, state->board_id)) return NULL;
    for (i = 0; i < layout->card_count; ++i) {
        const WenaCard *card;
        card = &layout->cards[i];
        if (!card->archived && !strcmp(card->id, state->card_id) &&
            !strcmp(card->board_id, state->board_id) &&
            !strcmp(card->list_id, state->source_list_id) &&
            !strcmp(card->swimlane_id, state->source_swimlane_id)) return card;
    }
    return NULL;
}

static const WenaSwimlane *find_lane(const WenaBoardLayout *layout, const char *id)
{
    size_t i;
    for (i = 0; i < layout->swimlane_count; ++i)
        if (!layout->swimlanes[i].archived && !strcmp(layout->swimlanes[i].id, id) &&
            !strcmp(layout->swimlanes[i].board_id, layout->board->id))
            return &layout->swimlanes[i];
    return NULL;
}

static const WenaList *find_list(const WenaBoardLayout *layout, const char *id,
    const char *lane_id)
{
    size_t i;
    for (i = 0; i < layout->list_count; ++i) {
        const WenaList *list;
        list = &layout->lists[i];
        if (!list->archived && !strcmp(list->id, id) &&
            !strcmp(list->board_id, layout->board->id) &&
            (list->swimlane_id[0] == '\0' || !strcmp(list->swimlane_id, lane_id))) return list;
    }
    return NULL;
}

void wena_card_move_init(WenaCardMoveState *state,
    WenaCardDetailsLoadTitle load, WenaCardMoveApply apply, void *context)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->load = load; state->apply = apply; state->context = context;
}

void wena_card_move_close(WenaCardMoveState *state)
{
    WenaCardDetailsLoadTitle load;
    WenaCardMoveApply apply;
    WenaCardMoveReorder reorder;
    void *context;
    if (state == NULL) return;
    load = state->load; apply = state->apply; reorder = state->reorder; context = state->context;
    free(state->order);
    wena_card_move_init(state, load, apply, context);
    state->reorder = reorder;
}

void wena_card_move_set_reorder_adapter(WenaCardMoveState *state,
    WenaCardMoveReorder reorder)
{
    if (state == NULL) return;
    wena_card_move_close(state);
    state->reorder = reorder;
}

static int same_column(const WenaCardMoveState *state, const WenaCard *card)
{
    return !strcmp(card->board_id, state->board_id) &&
        !strcmp(card->list_id, state->source_list_id) &&
        !strcmp(card->swimlane_id, state->source_swimlane_id);
}

static int capture_order(WenaCardMoveState *state, const WenaBoardLayout *layout)
{
    return wena_card_order_capture(layout->cards,layout->card_count,state->board_id,
        state->source_list_id,state->source_swimlane_id,&state->order,&state->order_count);
}

static int order_current(const WenaCardMoveState *state, const WenaBoardLayout *layout)
{
    return wena_card_order_current(state->order,state->order_count,layout->cards,
        layout->card_count,state->board_id,state->source_list_id,state->source_swimlane_id);
}

int wena_card_move_open(WenaCardMoveState *state,
    const WenaBoardLayout *layout, const char *card_id)
{
    size_t i;
    const WenaCard *card;
    char title[WENA_CARD_DETAILS_TITLE_CAPACITY];
    if (state == NULL) return 0;
    wena_card_move_close(state);
    if (!layout_valid(layout) || state->load == NULL || state->apply == NULL ||
        !wena_model_set_required(state->card_id, sizeof(state->card_id), card_id) ||
        !wena_model_set_required(state->board_id, sizeof(state->board_id), layout->board->id)) return 0;
    card = NULL;
    for (i = 0; i < layout->card_count; ++i) {
        if (!strcmp(layout->cards[i].id, state->card_id) &&
            !strcmp(layout->cards[i].board_id, state->board_id) && !layout->cards[i].archived)
            card = &layout->cards[i];
    }
    if (card == NULL || find_lane(layout, card->swimlane_id) == NULL ||
        find_list(layout, card->list_id, card->swimlane_id) == NULL) return 0;
    strcpy(state->source_list_id, card->list_id);
    strcpy(state->source_swimlane_id, card->swimlane_id);
    strcpy(state->target_list_id, card->list_id);
    strcpy(state->target_swimlane_id, card->swimlane_id);
    if (!state->load(state->context, state->board_id, state->card_id,
        title, sizeof(title), &state->version) || state->version == 0ul) return 0;
    if (state->reorder != NULL && !capture_order(state, layout)) return 0;
    state->visible = 1;
    return 1;
}

typedef struct MoveOptions {
    const WenaBoardLayout *layout;
    const char *lane_id;
    int lists;
    char label[WENA_TITLE_CAPACITY + WENA_ID_CAPACITY + 8];
} MoveOptions;

static const char *option_id(MoveOptions *options, int selected, const char **title)
{
    size_t i;
    int index;
    index = 0;
    if (options->lists) {
        for (i = 0; i < options->layout->list_count; ++i) {
            const WenaList *list = &options->layout->lists[i];
            if (find_list(options->layout, list->id, options->lane_id) != list) continue;
            if (index++ == selected) { *title = list->title; return list->id; }
        }
    } else {
        for (i = 0; i < options->layout->swimlane_count; ++i) {
            const WenaSwimlane *lane = &options->layout->swimlanes[i];
            if (find_lane(options->layout, lane->id) != lane) continue;
            if (index++ == selected) { *title = lane->title; return lane->id; }
        }
    }
    *title = wena_ui_text(WENA_UI_TEXT_UNKNOWN);
    return NULL;
}

static void option_label(void *data, int index, const char **text)
{
    MoveOptions *options;
    const char *title, *id;
    options = (MoveOptions *)data;
    id = option_id(options, index, &title);
    if (id == NULL) { *text = wena_ui_text(WENA_UI_TEXT_UNKNOWN); return; }
    else sprintf(options->label, "%s [%s]", title, id);
    *text = options->label;
}

static void render_selector(struct nk_context *context, MoveOptions *options,
    char *target)
{
    int count, selected, result;
    const char *id, *title;
    selected = -1; count = 0;
    while ((id = option_id(options, count, &title)) != NULL) {
        if (!strcmp(id, target)) selected = count;
        ++count;
    }
    if (count == 0) { nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_NO_ITEMS)); return; }
    /* An invalidated selection must be explicitly repaired before Save. */
    result = nk_combo_callback(context, option_label, options, selected,
                                count, 24, nk_vec2(280, 220));
    if (result >= 0 && result < count && result != selected) {
        id = option_id(options, result, &title);
        if (id != NULL) strcpy(target, id);
    }
}

static void render_targets(struct nk_context *context, WenaCardMoveState *state,
    const WenaBoardLayout *layout)
{
    MoveOptions options;
    options.layout = layout; options.lane_id = state->target_swimlane_id;
    options.lists = 0;
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_SWIMLANE), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 28, 1);
    render_selector(context, &options, state->target_swimlane_id);
    options.lists = 1;
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_LIST), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 28, 1);
    render_selector(context, &options, state->target_list_id);
}

typedef struct CardOrderOptions {
    const WenaBoardLayout *layout;
    WenaCardMoveState *state;
    char label[WENA_TITLE_CAPACITY + WENA_ID_CAPACITY + 128];
} CardOrderOptions;

static void order_label(void *data, int selected, const char **text)
{
    CardOrderOptions *options;
    const WenaCardMoveSlot *slot;
    const char *title, *archived;
    size_t i;
    options = (CardOrderOptions *)data;
    if (selected == 0) { *text = wena_ui_text(WENA_UI_TEXT_MOVE_TO_BOTTOM); return; }
    if (selected < 1 || (size_t)selected > options->state->order_count) {
        *text = wena_ui_text(WENA_UI_TEXT_UNKNOWN); return;
    }
    slot = &options->state->order[selected - 1]; title = "";
    i = slot->model_index;
    if (i < options->layout->card_count &&
        same_column(options->state, &options->layout->cards[i]) &&
        !strcmp(options->layout->cards[i].id, slot->id)) title = options->layout->cards[i].title;
    archived = slot->archived ? wena_ui_text(WENA_UI_TEXT_ARCHIVED) : "";
    if (strlen(title) + strlen(archived) + strlen(slot->id) + 40 >= sizeof(options->label)) title = "";
    if (strlen(archived) + strlen(slot->id) + 40 >= sizeof(options->label)) archived = "*";
    if (slot->archived) sprintf(options->label, "%d. %s (%s) [%s]", selected, title, archived, slot->id);
    else sprintf(options->label, "%d. %s [%s]", selected, title, slot->id);
    *text = options->label;
}

static void render_order(struct nk_context *context, WenaCardMoveState *state,
    const WenaBoardLayout *layout)
{
    CardOrderOptions options;
    if (state->reorder == NULL) return;
    if (strcmp(state->target_list_id, state->source_list_id) ||
        strcmp(state->target_swimlane_id, state->source_swimlane_id)) {
        state->reorder_choice = 0; return;
    }
    nk_layout_row_dynamic(context, 24, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_MANUAL_ORDER), NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 28, 1);
    options.layout = layout; options.state = state;
    state->reorder_choice = nk_combo_callback(context, order_label, &options,
        state->reorder_choice, (int)state->order_count + 1, 24, nk_vec2(300, 220));
}

int wena_card_move_render(struct nk_context *context, WenaCardMoveState *state,
    const WenaBoardLayout *layout, float width, float height)
{
    int close_requested;
    if (state == NULL || !state->visible) return 0;
    if (source_card(state, layout) == NULL ||
        find_lane(layout, state->source_swimlane_id) == NULL ||
        find_list(layout, state->source_list_id, state->source_swimlane_id) == NULL) {
        wena_card_move_close(state); return 0;
    }
    if (context == NULL || width <= 0 || height <= 0) return 0;
    close_requested = 0;
    if (nk_begin_titled(context, "Move card", wena_ui_control_text(WENA_UI_MOVE_CARD_TO), nk_rect(width * 0.5f, 0,
        width * 0.5f, height), NK_WINDOW_BORDER)) {
        /* Escape cancels the entire focused panel, including an open selector.
         * Check before widgets so it cannot share a frame with a mutation. */
        if ((wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) != 0u) {
            nk_end(context);
            wena_card_move_close(state);
            return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        nk_label(context, wena_ui_control_text(WENA_UI_MOVE_CARD_TO), NK_TEXT_LEFT);
        render_targets(context, state, layout);
        render_order(context, state, layout);
        nk_layout_row_dynamic(context, 28, 2);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE))) {
            if (find_lane(layout, state->target_swimlane_id) == NULL ||
                find_list(layout, state->target_list_id, state->target_swimlane_id) == NULL)
                state->error = 1;
            else if (state->reorder != NULL && state->reorder_choice != 0) {
                if (state->reorder_choice < 1 ||
                    (size_t)state->reorder_choice > state->order_count ||
                    strcmp(state->target_list_id, state->source_list_id) ||
                    strcmp(state->target_swimlane_id, state->source_swimlane_id) ||
                    !order_current(state, layout) ||
                    !state->reorder(state->context, state->board_id, state->card_id,
                        state->version, (unsigned long)(state->reorder_choice - 1))) state->error = 1;
                else close_requested = 1;
            } else if (state->apply != NULL && state->apply(state->context,
                state->board_id, state->card_id, state->version,
                state->target_list_id, state->target_swimlane_id)) close_requested = 1;
            else state->error = 1;
        }
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL))) close_requested = 1;
        nk_layout_row_dynamic(context, 28, 1);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE))) close_requested = 1;
        if (state->error) {
                nk_layout_row_dynamic(context, 48.0f, 1);
                nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
            }
    }
    nk_end(context);
    if (close_requested) wena_card_move_close(state);
    return 1;
}
