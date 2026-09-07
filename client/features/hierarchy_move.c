#include "hierarchy_move.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

static int board_valid(const WenaBoardLayout *layout, WenaHierarchyKind kind)
{
    return layout != NULL && layout->board != NULL && !layout->board->archived &&
        ((kind == WENA_HIERARCHY_LIST &&
          (layout->list_count == 0 || layout->lists != NULL)) ||
         (kind == WENA_HIERARCHY_SWIMLANE &&
          (layout->swimlane_count == 0 || layout->swimlanes != NULL)));
}

/* Every visible sibling owns exactly one contiguous nonnegative position.
 * Filtering by board never changes the semantic SQLite destination ordinal. */
static int item(const WenaBoardLayout *layout, WenaHierarchyKind kind,
    size_t index, const char **id, const char **title, double *position)
{
    if (kind == WENA_HIERARCHY_LIST) {
        const WenaList *list;
        list = &layout->lists[index];
        if (list->archived || strcmp(list->board_id, layout->board->id)) return 0;
        *id = list->id; *title = list->title; *position = list->sort;
    } else {
        const WenaSwimlane *lane;
        lane = &layout->swimlanes[index];
        if (lane->archived || strcmp(lane->board_id, layout->board->id)) return 0;
        *id = lane->id; *title = lane->title; *position = lane->sort;
    }
    return 1;
}

static int order_valid(WenaHierarchyMoveState *state,
    const WenaBoardLayout *layout, int capture)
{
    size_t i, j, count, size;
    const char *id, *title;
    double position;
    unsigned char seen[WENA_HIERARCHY_MOVE_CAPACITY];
    if (!board_valid(layout, state->kind) || strcmp(state->board_id, layout->board->id)) return 0;
    size = state->kind == WENA_HIERARCHY_LIST ? layout->list_count : layout->swimlane_count;
    memset(seen, 0, sizeof(seen)); count = 0;
    for (i = 0; i < size; ++i) {
        if (!item(layout, state->kind, i, &id, &title, &position)) continue;
        (void)title;
        if (!(position >= 0 && position < (double)WENA_HIERARCHY_MOVE_CAPACITY)) return 0;
        j = (size_t)position;
        if (position != (double)j || seen[j]) return 0;
        seen[j] = 1;
        if (capture) {
            if (!wena_model_set_required(state->order[j], sizeof(WenaId), id)) return 0;
        } else if (strcmp(state->order[j], id)) return 0;
        ++count;
    }
    if (count == 0 || count > WENA_HIERARCHY_MOVE_CAPACITY || (!capture && count != state->count)) return 0;
    for (i = 0; i < count; ++i) {
        if (!seen[i]) return 0;
        for (j = 0; j < i; ++j) if (!strcmp(state->order[i], state->order[j])) return 0;
    }
    if (capture) state->count = count;
    return 1;
}

void wena_hierarchy_move_init(WenaHierarchyMoveState *state,
    WenaHierarchyMoveLoad load, WenaHierarchyMoveApply apply, void *context)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->load = load; state->apply = apply; state->context = context;
}

void wena_hierarchy_move_close(WenaHierarchyMoveState *state)
{
    if (state == NULL) return;
    state->visible = 0; state->error = 0; state->version = 0;
    state->count = 0; state->source_position = 0; state->target_position = 0;
    state->board_id[0] = '\0'; state->target_id[0] = '\0';
}

int wena_hierarchy_move_open(WenaHierarchyMoveState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind, const char *target_id)
{
    size_t i;
    if (state == NULL) return 0;
    wena_hierarchy_move_close(state);
    if (!board_valid(layout, kind) || state->load == NULL || state->apply == NULL) return 0;
    state->kind = kind;
    if (!wena_model_set_required(state->board_id, sizeof(state->board_id), layout->board->id) ||
        !wena_model_set_required(state->target_id, sizeof(state->target_id), target_id) ||
        !order_valid(state, layout, 1)) return 0;
    for (i = 0; i < state->count; ++i) if (!strcmp(state->order[i], target_id)) break;
    if (i == state->count || !state->load(state->context, state->board_id, kind,
        state->target_id, &state->version, &state->source_position) || state->version == 0 ||
        state->source_position != (unsigned long)i) return 0;
    state->target_position = (int)i;
    state->visible = 1;
    return 1;
}

typedef struct HierarchyMoveOptions {
    WenaHierarchyMoveState *state;
    const WenaBoardLayout *layout;
    char label[WENA_TITLE_CAPACITY + WENA_ID_CAPACITY + 32];
} HierarchyMoveOptions;

static void option_label(void *data, int selected, const char **label)
{
    HierarchyMoveOptions *options;
    const char *id, *title;
    double position;
    size_t i, size;
    options = (HierarchyMoveOptions *)data;
    *label = wena_ui_text(WENA_UI_TEXT_UNKNOWN);
    if (selected >= 0 && (size_t)selected < options->state->count) {
        size = options->state->kind == WENA_HIERARCHY_LIST ? options->layout->list_count : options->layout->swimlane_count;
        for (i = 0; i < size; ++i) {
            if (item(options->layout, options->state->kind, i, &id, &title, &position) &&
                !strcmp(id, options->state->order[selected])) {
                sprintf(options->label, "%d. %s [%s]", selected + 1, title, id);
                *label = options->label;
                break;
            }
        }
    }
}

int wena_hierarchy_move_render(struct nk_context *context,
    WenaHierarchyMoveState *state, const WenaBoardLayout *layout,
    float width, float height)
{
    HierarchyMoveOptions options;
    int close_requested;
    if (state == NULL || !state->visible) return 0;
    if (!order_valid(state, layout, 0)) { wena_hierarchy_move_close(state); return 0; }
    if (context == NULL || width <= 0 || height <= 0) return 0;
    close_requested = 0;
    if (nk_begin_titled(context, "Move hierarchy",
        wena_ui_control_text(state->kind == WENA_HIERARCHY_LIST ?
            WENA_UI_MOVE_LIST_TO : WENA_UI_MOVE_SWIMLANE_TO), nk_rect(width * 0.2f,
        height * 0.2f, width * 0.6f, 210), NK_WINDOW_BORDER)) {
        /* Escape cancels the entire focused panel, including an open selector.
         * Check before widgets so it cannot share a frame with a mutation. */
        if ((wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) != 0u) {
            nk_end(context);
            wena_hierarchy_move_close(state);
            return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        nk_label(context, wena_ui_control_text(state->kind == WENA_HIERARCHY_LIST ?
            WENA_UI_MOVE_LIST_TO : WENA_UI_MOVE_SWIMLANE_TO), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28, 1);
        options.state = state; options.layout = layout;
        state->target_position = nk_combo_callback(context, option_label, &options,
            state->target_position, (int)state->count, 24, nk_vec2(300, 220));
        nk_layout_row_dynamic(context, 28, 2);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE))) {
            if (state->target_position < 0 || (size_t)state->target_position >= state->count)
                state->error = 1;
            else if (state->apply != NULL && state->apply(state->context,
                state->board_id, state->kind, state->target_id, state->version,
                (unsigned long)state->target_position)) close_requested = 1;
            else state->error = 1;
        }
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL))) close_requested = 1;
        nk_layout_row_dynamic(context, 28, 1);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE))) close_requested = 1;
        if (state->error) {
            nk_layout_row_dynamic(context, 24, 1);
            nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        }
    }
    nk_end(context);
    if (close_requested) wena_hierarchy_move_close(state);
    return 1;
}
