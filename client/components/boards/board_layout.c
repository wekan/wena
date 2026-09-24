#include "board_layout.h"
#include "board_header.h"
#include "../../../imports/ui/page_contract.h"
#include "../lists/list_header.h"
#include "../cards/card_body.h"

#include <nuklear.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

static int wena_same_id(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static int wena_collapse_layout_valid(const WenaBoardLayout *layout)
{
    return layout != NULL && layout->board != NULL &&
        !layout->board->archived && layout->board->id[0] != '\0' &&
        (layout->swimlane_count == 0 || layout->swimlanes != NULL) &&
        (layout->list_count == 0 || layout->lists != NULL);
}

static int wena_collapse_active(const WenaBoardLayout *layout,
                                WenaBoardCollapseKind kind, const char *id)
{
    size_t index;
    size_t lane_index;

    if (kind == WENA_COLLAPSE_SWIMLANE) {
        for (index = 0; index < layout->swimlane_count; ++index) {
            const WenaSwimlane *lane = &layout->swimlanes[index];
            if (!lane->archived && wena_same_id(lane->id, id) &&
                wena_same_id(lane->board_id, layout->board->id)) {
                return 1;
            }
        }
    } else if (kind == WENA_COLLAPSE_LIST) {
        for (index = 0; index < layout->list_count; ++index) {
            const WenaList *list = &layout->lists[index];
            if (!list->archived && wena_same_id(list->id, id) &&
                wena_same_id(list->board_id, layout->board->id)) {
                for (lane_index = 0; lane_index < layout->swimlane_count;
                     ++lane_index) {
                    const WenaSwimlane *lane = &layout->swimlanes[lane_index];
                    if (!lane->archived &&
                        wena_same_id(lane->board_id, layout->board->id) &&
                        (list->swimlane_id[0] == '\0' ||
                         wena_same_id(list->swimlane_id, lane->id))) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void wena_board_collapse_init(WenaBoardCollapseState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

static void wena_collapse_prune(WenaId *ids, size_t *count,
                                const WenaBoardLayout *layout,
                                WenaBoardCollapseKind kind)
{
    size_t read_index;
    size_t write_index;

    write_index = 0;
    for (read_index = 0; read_index < *count; ++read_index) {
        if (wena_collapse_active(layout, kind, ids[read_index])) {
            if (write_index != read_index) {
                memcpy(ids[write_index], ids[read_index], sizeof(WenaId));
            }
            ++write_index;
        }
    }
    *count = write_index;
}

int wena_board_collapse_sync(WenaBoardCollapseState *state,
                              const WenaBoardLayout *layout)
{
    if (state == NULL || !wena_collapse_layout_valid(layout)) {
        return 0;
    }
    if (!wena_same_id(state->board_id, layout->board->id)) {
        wena_board_collapse_init(state);
        (void)wena_model_set_required(state->board_id, sizeof(state->board_id),
                                      layout->board->id);
    }
    if (state->swimlane_count > WENA_BOARD_COLLAPSE_CAPACITY ||
        state->list_count > WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    wena_collapse_prune(state->swimlane_ids, &state->swimlane_count, layout,
                        WENA_COLLAPSE_SWIMLANE);
    wena_collapse_prune(state->list_ids, &state->list_count, layout,
                        WENA_COLLAPSE_LIST);
    return 1;
}

int wena_board_is_collapsed(const WenaBoardCollapseState *state,
                             const char *board_id, WenaBoardCollapseKind kind,
                             const char *id)
{
    size_t index;
    size_t count;
    const WenaId *ids;

    if (state == NULL || board_id == NULL || id == NULL ||
        !wena_same_id(state->board_id, board_id) ||
        (kind != WENA_COLLAPSE_SWIMLANE && kind != WENA_COLLAPSE_LIST)) {
        return 0;
    }
    count = kind == WENA_COLLAPSE_SWIMLANE ? state->swimlane_count :
        state->list_count;
    ids = kind == WENA_COLLAPSE_SWIMLANE ? state->swimlane_ids : state->list_ids;
    if (count > WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    for (index = 0; index < count; ++index) {
        if (wena_same_id(ids[index], id)) {
            return 1;
        }
    }
    return 0;
}

int wena_board_collapse_set(WenaBoardCollapseState *state,
                             const WenaBoardLayout *layout,
                             WenaBoardCollapseKind kind, const char *id,
                             int collapsed)
{
    WenaId validated;
    WenaId *ids;
    size_t *count;
    size_t index;

    if (state == NULL || !wena_collapse_layout_valid(layout) ||
        !wena_same_id(state->board_id, layout->board->id) ||
        !wena_model_set_required(validated, sizeof(validated), id) ||
        !wena_collapse_active(layout, kind, validated)) {
        return 0;
    }
    ids = kind == WENA_COLLAPSE_SWIMLANE ? state->swimlane_ids : state->list_ids;
    count = kind == WENA_COLLAPSE_SWIMLANE ? &state->swimlane_count :
        &state->list_count;
    if (*count > WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    for (index = 0; index < *count; ++index) {
        if (wena_same_id(ids[index], validated)) {
            if (!collapsed) {
                --*count;
                if (index < *count) {
                    memmove(ids[index], ids[index + 1],
                            (*count - index) * sizeof(WenaId));
                }
            }
            return 1;
        }
    }
    if (!collapsed) {
        return 1;
    }
    if (*count == WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    strcpy(ids[*count], validated);
    ++*count;
    return 1;
}

static int wena_collapse_control(struct nk_context *context,
                                  const WenaBoardLayout *layout,
                                  WenaBoardCollapseKind kind, const char *id)
{
    int collapsed;
    WenaUiControlId control;

    if (layout->collapse == NULL) {
        return 0;
    }
    collapsed = wena_board_is_collapsed(layout->collapse, layout->board->id,
                                         kind, id);
    if (kind == WENA_COLLAPSE_LIST) {
        control = collapsed ? WENA_UI_EXPAND_LIST : WENA_UI_COLLAPSE_LIST;
    } else {
        control = collapsed ? WENA_UI_EXPAND_SWIMLANE : WENA_UI_COLLAPSE_SWIMLANE;
    }
    nk_layout_row_dynamic(context, 24.0f, 1);
    if (nk_button_label(context, wena_ui_control_text(control)) &&
        wena_board_collapse_set(layout->collapse, layout, kind, id, !collapsed)) {
        collapsed = !collapsed;
    }
    return collapsed;
}

/* Titles may repeat or change. Nuklear scroll/widget state follows model IDs. */
static int wena_model_group_begin(struct nk_context *context,
                                   const char *prefix, const char *board_id,
                                   const char *lane_id, const char *id)
{
    char key[3 * WENA_ID_CAPACITY + 32];

    (void)sprintf(key, "%s%lu:%s/%lu:%s/%s", prefix,
                   (unsigned long)strlen(board_id), board_id,
                   (unsigned long)strlen(lane_id), lane_id, id);
    return nk_group_begin(context, key, NK_WINDOW_BORDER);
}

static void wena_render_cards(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaList *list,
                              const WenaSwimlane *swimlane)
{
    size_t index, ordinal, position;
    unsigned int card_action;
    int collapsed;

    if (layout->card_drop_target)
        layout->card_drop_target(context, layout->card_drop_context, list, swimlane);
    ordinal = 0;
    for (index = 0; index < layout->card_count; ++index) {
        const WenaCard *card = &layout->cards[index];

        if (!wena_same_id(card->board_id, layout->board->id) ||
            !wena_same_id(card->list_id, list->id) ||
            !wena_same_id(card->swimlane_id, swimlane->id)) continue;
        position = ordinal++;
        if (!card->archived &&
            (layout->card_visible == NULL ||
             layout->card_visible(layout->card_visible_context, card))) {
            card_action = WENA_CARD_BODY_NO_ACTION;
            if (layout->card_drag_handle)
                layout->card_drag_handle(context, layout->card_drag_context, card, position);
            collapsed = layout->card_collapsed && layout->card_collapsed(context,
                layout->card_collapsed_context, card);
            if (!collapsed && layout->card_badges != NULL)
                card_action = layout->card_badges(context,
                    layout->card_badges_context, card);
            card_action |= wena_card_body_render(context, card);
            if (!collapsed && layout->card_contents != NULL)
                card_action |= layout->card_contents(context,
                    layout->card_contents_context, card);
            if (layout->card_interaction != NULL &&
                card_action != WENA_CARD_BODY_NO_ACTION) {
                layout->card_interaction->actions = card_action;
                (void)wena_model_set_required(layout->card_interaction->card_id,
                    sizeof(layout->card_interaction->card_id), card->id);
            }
        }
    }
}

static void wena_render_lists(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaSwimlane *swimlane)
{
    size_t index;
    size_t visible_count;
    unsigned int list_action;

    visible_count = 0;
    for (index = 0; index < layout->list_count; ++index) {
        const WenaList *list = &layout->lists[index];
        if (!list->archived && wena_same_id(list->board_id, layout->board->id) &&
            (list->swimlane_id[0] == '\0' ||
             wena_same_id(list->swimlane_id, swimlane->id))) {
            ++visible_count;
        }
    }
    if (visible_count == 0 || visible_count > (size_t)INT_MAX) {
        return;
    }
    /* Fixed-width columns stay readable; the containing lane scrolls sideways. */
    nk_layout_row_begin(context, NK_STATIC, 270.0f, (int)visible_count);
    for (index = 0; index < layout->list_count; ++index) {
        const WenaList *list = &layout->lists[index];

        if (list->archived || !wena_same_id(list->board_id, layout->board->id) ||
            (list->swimlane_id[0] != '\0' &&
             !wena_same_id(list->swimlane_id, swimlane->id))) {
            continue;
        }
        nk_layout_row_push(context, 260.0f);
        if (wena_model_group_begin(context, "list/", layout->board->id,
                                    swimlane->id, list->id)) {
            list_action = wena_list_header_render(context, list);
            if (layout->list_drag_handle) layout->list_drag_handle(context,
                layout->hierarchy_drag_context,list,index);
            if (layout->list_interaction != NULL &&
                list_action != WENA_LIST_HEADER_NO_ACTION) {
                layout->list_interaction->actions = list_action;
                (void)wena_model_set_required(layout->list_interaction->board_id,
                    sizeof(layout->list_interaction->board_id), layout->board->id);
                (void)wena_model_set_required(layout->list_interaction->swimlane_id,
                    sizeof(layout->list_interaction->swimlane_id), swimlane->id);
                (void)wena_model_set_required(layout->list_interaction->list_id,
                    sizeof(layout->list_interaction->list_id), list->id);
            }
            if (!wena_collapse_control(context, layout, WENA_COLLAPSE_LIST,
                                       list->id)) {
                wena_render_cards(context, layout, list, swimlane);
            }
            nk_group_end(context);
        }
    }
    nk_layout_row_end(context);
}

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout)
{
    size_t index;
    unsigned int header_action;

    if (context == NULL || layout == NULL || layout->board == NULL ||
        layout->board->archived ||
        (layout->swimlane_count != 0 && layout->swimlanes == NULL) ||
        (layout->list_count != 0 && layout->lists == NULL) ||
        (layout->card_count != 0 && layout->cards == NULL)) {
        return 0;
    }
    if (layout->list_interaction != NULL) {
        layout->list_interaction->actions = WENA_LIST_HEADER_NO_ACTION;
        layout->list_interaction->list_id[0] = '\0';
        layout->list_interaction->board_id[0] = '\0';
        layout->list_interaction->swimlane_id[0] = '\0';
    }
    if (layout->swimlane_interaction != NULL) {
        layout->swimlane_interaction->actions = 0u;
        layout->swimlane_interaction->board_id[0] = '\0';
        layout->swimlane_interaction->swimlane_id[0] = '\0';
    }
    if (layout->card_interaction != NULL) {
        layout->card_interaction->actions = WENA_CARD_BODY_NO_ACTION;
        layout->card_interaction->card_id[0] = '\0';
    }
    if (layout->collapse != NULL &&
        !wena_board_collapse_sync(layout->collapse, layout)) {
        return 0;
    }
    header_action = wena_board_header_render(context, layout->board);
    if (layout->toolbar != NULL) {
        layout->toolbar(context, layout->toolbar_context);
    }
    if (layout->sidebar != NULL &&
        (header_action & WENA_BOARD_HEADER_OPEN_MENU) != 0u) {
        layout->sidebar->visible = 1;
    }
    if (layout->card_visible != NULL) {
        int matched;
        matched = 0;
        for (index = 0; index < layout->card_count; ++index)
            if (!layout->cards[index].archived &&
                wena_same_id(layout->cards[index].board_id, layout->board->id) &&
                layout->card_visible(layout->card_visible_context, &layout->cards[index])) {
                matched = 1; break;
            }
        if (!matched) {
            nk_layout_row_dynamic(context, 28, 1);
            nk_label(context, wena_ui_text(WENA_UI_TEXT_NO_CARDS_FOUND), NK_TEXT_LEFT);
        }
    }
    for (index = 0; index < layout->swimlane_count; ++index) {
        const WenaSwimlane *swimlane = &layout->swimlanes[index];

        if (swimlane->archived ||
            !wena_same_id(swimlane->board_id, layout->board->id)) {
            continue;
        }
        nk_layout_row_dynamic(context,
            wena_board_is_collapsed(layout->collapse, layout->board->id,
                WENA_COLLAPSE_SWIMLANE, swimlane->id) ?
                (layout->swimlane_drag_handle ? 112.0f : 80.0f) : 360.0f, 1);
        if (wena_model_group_begin(context, "lane/", layout->board->id,
                                    "", swimlane->id)) {
            nk_layout_row_dynamic(context, 26.0f,
                layout->swimlane_interaction == NULL ? 1 : 2);
            nk_label(context, swimlane->title, NK_TEXT_LEFT);
            if (layout->swimlane_interaction != NULL &&
                nk_button_label(context, wena_ui_control_text(WENA_UI_RENAME_SWIMLANE))) {
                layout->swimlane_interaction->actions = WENA_SWIMLANE_EDIT_TITLE;
                (void)wena_model_set_required(layout->swimlane_interaction->board_id,
                    sizeof(layout->swimlane_interaction->board_id), layout->board->id);
                (void)wena_model_set_required(layout->swimlane_interaction->swimlane_id,
                    sizeof(layout->swimlane_interaction->swimlane_id), swimlane->id);
            }
            if (layout->swimlane_drag_handle) layout->swimlane_drag_handle(context,
                layout->hierarchy_drag_context,swimlane,index);
            if (!wena_collapse_control(context, layout, WENA_COLLAPSE_SWIMLANE,
                                       swimlane->id)) {
                wena_render_lists(context, layout, swimlane);
            }
            nk_group_end(context);
        }
    }
    if (!layout->sidebar_as_window)
        (void)wena_board_sidebar_render(context, layout->sidebar);
    return 1;
}
