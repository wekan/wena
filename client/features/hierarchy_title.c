#include "hierarchy_title.h"
#include "../../imports/ui/page_contract.h"

#include <nuklear.h>
#include <string.h>

static int selected(const WenaBoardLayout *layout, WenaHierarchyKind kind,
                      const char *board, const char *target)
{
    size_t index;
    if (!layout || !layout->board || layout->board->archived || !board ||
        !target || !target[0] || strcmp(layout->board->id, board) != 0 ||
        (layout->list_count && !layout->lists) ||
        (layout->swimlane_count && !layout->swimlanes)) return 0;
    if (kind == WENA_HIERARCHY_BOARD) return strcmp(board, target) == 0;
    if (kind == WENA_HIERARCHY_LIST) {
        for (index = 0; index < layout->list_count; ++index) {
            if (!layout->lists[index].archived &&
                strcmp(layout->lists[index].id, target) == 0 &&
                strcmp(layout->lists[index].board_id, board) == 0) return 1;
        }
    } else if (kind == WENA_HIERARCHY_SWIMLANE) {
        for (index = 0; index < layout->swimlane_count; ++index) {
            if (!layout->swimlanes[index].archived &&
                strcmp(layout->swimlanes[index].id, target) == 0 &&
                strcmp(layout->swimlanes[index].board_id, board) == 0) return 1;
        }
    }
    return 0;
}

void wena_hierarchy_title_init(WenaHierarchyTitleState *state)
{
    if (state) memset(state, 0, sizeof(*state));
}

void wena_hierarchy_title_close(WenaHierarchyTitleState *state)
{
    if (!state) return;
    state->visible = 0;
    state->requested_action = 0u;
    state->creating = 0;
    state->error = 0;
    state->title_length = 0;
    state->board_id[0] = '\0';
    state->target_id[0] = '\0';
    state->title_input[0] = '\0';
    state->title_version = 0;
}

void wena_hierarchy_title_set_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyLoadTitle load, WenaHierarchySaveTitle save, void *context)
{
    if (!state) return;
    wena_hierarchy_title_close(state);
    state->load_title = load;
    state->save_title = save;
    state->context = context;
    state->create_title = NULL;
}

void wena_hierarchy_title_set_create_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyCreateTitle create)
{
    if (!state) return;
    wena_hierarchy_title_close(state);
    state->create_title = create;
}

int wena_hierarchy_title_open_create(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind)
{
    if (!state) return 0;
    wena_hierarchy_title_close(state);
    if (!layout || !layout->board || !state->create_title ||
        (kind != WENA_HIERARCHY_LIST && kind != WENA_HIERARCHY_SWIMLANE) ||
        !selected(layout, WENA_HIERARCHY_BOARD, layout->board->id, layout->board->id) ||
        !wena_model_set_required(state->board_id, sizeof(state->board_id), layout->board->id))
        return 0;
    state->kind = kind;
    state->creating = 1;
    state->visible = 1;
    return 1;
}

int wena_hierarchy_title_open(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind, const char *target)
{
    if (!state) return 0;
    wena_hierarchy_title_close(state);
    if (!layout || !layout->board || !state->load_title || !state->save_title ||
        !selected(layout, kind, layout->board->id, target) ||
        !wena_model_set_required(state->board_id, sizeof(state->board_id),
                                  layout->board->id) ||
        !wena_model_set_required(state->target_id, sizeof(state->target_id), target))
        return 0;
    state->kind = kind;
    memset(state->title_input, 0, sizeof(state->title_input));
    if (!state->load_title(state->context, state->board_id, kind, state->target_id,
            state->title_input, WENA_CARD_DETAILS_TITLE_CAPACITY,
            &state->title_version) || state->title_version == 0 ||
        memchr(state->title_input, '\0', WENA_CARD_DETAILS_TITLE_CAPACITY) == NULL ||
        !wena_card_details_title_valid(state->title_input,
                                        strlen(state->title_input))) {
        wena_hierarchy_title_close(state);
        return 0;
    }
    state->title_length = (int)strlen(state->title_input);
    state->visible = 1;
    return 1;
}

int wena_hierarchy_title_render(struct nk_context *context,
    WenaHierarchyTitleState *state, const WenaBoardLayout *layout,
    float width, float height)
{
    int cancel;
    int save;
    unsigned int edit_keys;
    if (state) state->requested_action = 0u;
    if (!state || !state->visible || !context || width <= 0.0f || height <= 0.0f)
        return 0;
    if (!selected(layout, state->creating ? WENA_HIERARCHY_BOARD : state->kind,
        state->board_id, state->creating ? state->board_id : state->target_id) ||
        (state->creating && state->kind != WENA_HIERARCHY_LIST &&
         state->kind != WENA_HIERARCHY_SWIMLANE)) {
        wena_hierarchy_title_close(state);
        return 0;
    }
    save = cancel = 0;
    if (nk_begin_titled(context, "Edit hierarchy title",
        wena_ui_control_text(state->creating ?
            (state->kind == WENA_HIERARCHY_LIST ? WENA_UI_ADD_LIST :
             WENA_UI_ADD_SWIMLANE) : WENA_UI_EDIT_TITLE),
        nk_rect(width * 0.2f, height * 0.2f, width * 0.6f, 210.0f),
        NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        nk_label(context, wena_ui_control_text(state->creating ?
            (state->kind == WENA_HIERARCHY_LIST ? WENA_UI_ADD_LIST :
             WENA_UI_ADD_SWIMLANE) : WENA_UI_EDIT_TITLE), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 32.0f, 1);
        edit_keys = wena_title_input_keys(context,
            nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                state->title_input, &state->title_length,
                (int)sizeof(state->title_input), nk_filter_default));
        nk_layout_row_dynamic(context, 28.0f, 2);
        save = nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE));
        cancel = nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL));
        save = save || (edit_keys & WENA_TITLE_INPUT_COMMIT) != 0u;
        cancel = cancel || (edit_keys & WENA_TITLE_INPUT_CANCEL) != 0u;
        if (!state->creating && (state->kind == WENA_HIERARCHY_LIST ||
            state->kind == WENA_HIERARCHY_SWIMLANE)) {
            nk_layout_row_dynamic(context, 28.0f, 1);
            if (nk_button_label(context, wena_ui_control_text(
                state->kind == WENA_HIERARCHY_LIST ? WENA_UI_MOVE_LIST_TO :
                WENA_UI_MOVE_SWIMLANE_TO))) state->requested_action = WENA_HIERARCHY_TITLE_MOVE;
        }
        if (state->error) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        }
    }
    nk_end(context);
    if (cancel) wena_hierarchy_title_close(state);
    else if (save) {
        if (state->title_length >= 0 &&
            wena_card_details_title_valid(state->title_input,
                                            (size_t)state->title_length)) {
            state->title_input[state->title_length] = '\0';
            if (state->creating ?
                (state->create_title && state->create_title(state->context,
                    state->board_id, state->kind, state->title_input)) :
                (state->save_title && state->save_title(state->context,
                    state->board_id, state->kind, state->target_id,
                    state->title_version, state->title_input))) {
                wena_hierarchy_title_close(state);
                return 1;
            }
        }
        state->error = 1;
    }
    return 1;
}
