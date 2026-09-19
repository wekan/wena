#include "checklists.h"
#include "../../imports/ui/page_contract.h"
#include "../../models/checklist_item_titles.h"
#include <nuklear.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void wena_checklists_init(WenaChecklistsState *state, WenaChecklistsLoad load,
    WenaChecklistsSave save, void *context)
{
    if (state) {
        memset(state, 0, sizeof(*state));
        state->load = load;
        state->save = save;
        state->context = context;
    }
}

void wena_checklists_close(WenaChecklistsState *state)
{
    if (state) {
        free(state->snapshot);
        state->snapshot = NULL;
        state->visible = 0;
        state->error = 0;
        state->needs_refresh = 0;
        state->action = 0;
        state->length = 0;
        state->input[0] = 0;
    }
}

static int reload_snapshot(WenaChecklistsState *state)
{
    WenaChecklistSnapshot *replacement;
    /* Publish only after the detached replacement passes complete validation. */
    replacement = (WenaChecklistSnapshot *)calloc(1, sizeof(*replacement));
    if (!replacement) return 0;
    if (!state->load || !state->load(state->context, state->board_id, state->card_id,
        replacement) || !wena_checklist_snapshot_valid(replacement, state->board_id,
        state->card_id)) {
        free(replacement);
        return 0;
    }
    free(state->snapshot);
    state->snapshot = replacement;
    state->needs_refresh = 0;
    state->error = 0;
    state->action = 0;
    return 1;
}

int wena_checklists_open(WenaChecklistsState *state, const WenaCard *card)
{
    if (!state) return 0;
    wena_checklists_close(state);
    if (!card || card->archived || !wena_model_identifier_valid(card->id) ||
        !wena_model_identifier_valid(card->board_id)) return 0;
    strcpy(state->board_id, card->board_id);
    strcpy(state->card_id, card->id);
    if (!reload_snapshot(state)) return 0;
    state->visible = 1;
    return 1;
}

static void begin_edit(WenaChecklistsState *state, WenaChecklistAction action, size_t list_index,
    size_t item_index)
{
    /* Capture stable identities and authoritative revisions before editing. */
    state->action = action;
    state->card_version = state->snapshot->card_version;
    state->checklist_id[0] = 0;
    state->item_id[0] = 0;
    state->input[0] = 0;
    state->length = 0;
    state->error = 0;
    state->checklist_version = 0;
    state->item_version = 0;
    state->is_finished = 0;
    if (action != WENA_CHECKLIST_CREATE) {
        strcpy(state->checklist_id, state->snapshot->checklists[list_index].id);
        state->checklist_version = state->snapshot->checklist_versions[list_index];
    }
    if (action == WENA_CHECKLIST_RENAME || action == WENA_CHECKLIST_REORDER) strcpy(state->input,
        state->snapshot->checklists[list_index].title);
    if (action == WENA_CHECKLIST_RENAME_ITEM || action == WENA_CHECKLIST_SET_FINISHED ||
        action == WENA_CHECKLIST_REORDER_ITEM) {
        strcpy(state->item_id, state->snapshot->items[item_index].id);
        state->item_version = state->snapshot->item_versions[item_index];
        strcpy(state->input, state->snapshot->items[item_index].title);
        state->is_finished = !state->snapshot->items[item_index].is_finished;
    }
    if (action == WENA_CHECKLIST_SET_FLAGS) {
        state->hide_checked_items = state->snapshot->checklists[list_index].hide_checked_items;
        state->hide_all_items = state->snapshot->checklists[list_index].hide_all_items;
        state->preserved_show_on_minicard =
            state->snapshot->checklists[list_index].show_on_minicard;
    }
    state->length = (int)strlen(state->input);
}

static int is_ordering(WenaChecklistAction action)
{
    return action == WENA_CHECKLIST_REORDER || action == WENA_CHECKLIST_REORDER_ITEM;
}

static void begin_order(WenaChecklistsState *state)
{
    size_t list, item, index;
    int ordinal;
    if (state->action != WENA_CHECKLIST_RENAME &&
        state->action != WENA_CHECKLIST_RENAME_ITEM) return;
    for (list = 0; list < state->snapshot->checklist_count; ++list)
        if (!strcmp(state->snapshot->checklists[list].id, state->checklist_id)) break;
    if (list == state->snapshot->checklist_count) { state->error = 1; return; }
    if (state->action == WENA_CHECKLIST_RENAME) {
        begin_edit(state, WENA_CHECKLIST_REORDER, list, 0);
        state->order_position = (int)list;
        state->order_count = (int)state->snapshot->checklist_count;
    } else {
        for (item = 0; item < state->snapshot->item_count; ++item)
            if (!strcmp(state->snapshot->items[item].id, state->item_id)) break;
        if (item == state->snapshot->item_count) { state->error = 1; return; }
        begin_edit(state, WENA_CHECKLIST_REORDER_ITEM, list, item);
        ordinal = 0;
        for (index = 0; index < state->snapshot->item_count; ++index) {
            if (strcmp(state->snapshot->items[index].checklist_id, state->checklist_id)) continue;
            if (index == item) state->order_position = ordinal;
            ++ordinal;
        }
        state->order_count = ordinal;
    }
}

static void order_label(void *context, int index, const char **label)
{
    char *buffer;
    buffer = (char *)context;
    sprintf(buffer, "%d", index + 1);
    *label = buffer;
}

static int is_deletion(WenaChecklistAction action)
{
    return action == WENA_CHECKLIST_DELETE || action == WENA_CHECKLIST_DELETE_ITEM;
}

static void confirm_deletion(WenaChecklistsState *state)
{
    size_t index;

    /* A deletion names the selected stored object, never an unsaved rename. */
    if (state->action == WENA_CHECKLIST_RENAME) {
        for (index = 0; index < state->snapshot->checklist_count; ++index) {
            if (strcmp(state->snapshot->checklists[index].id, state->checklist_id) == 0) {
                strcpy(state->input, state->snapshot->checklists[index].title);
                state->action = WENA_CHECKLIST_DELETE;
                state->error = 0;
                return;
            }
        }
    } else if (state->action == WENA_CHECKLIST_RENAME_ITEM) {
        for (index = 0; index < state->snapshot->item_count; ++index) {
            if (strcmp(state->snapshot->items[index].id, state->item_id) == 0) {
                strcpy(state->input, state->snapshot->items[index].title);
                state->action = WENA_CHECKLIST_DELETE_ITEM;
                state->error = 0;
                return;
            }
        }
    }
}

static void submit_edit(WenaChecklistsState *state)
{
    WenaChecklistEdit mutation;
    char parsed[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    size_t count;
    if (is_ordering(state->action) && (state->order_position < 0 ||
        state->order_position >= state->order_count)) {
        state->error = 1;
        return;
    }
    if (state->action == WENA_CHECKLIST_SET_FLAGS && ((state->hide_checked_items != 0 &&
        state->hide_checked_items != 1) || (state->hide_all_items != 0 && state->hide_all_items
        != 1))) {
        state->error = 1;
        return;
    }
    /* Title forms retain their draft after validation or persistence failures. */
    if (state->action == WENA_CHECKLIST_ADD_ITEMS) {
        if (state->length < 0 ||
            !wena_checklist_item_batch_parse(state->input, (size_t)state->length,
                parsed, &count)) {
            state->error = 1;
            return;
        }
    }
    else if (!is_deletion(state->action) && !is_ordering(state->action) &&
        state->action != WENA_CHECKLIST_SET_FINISHED &&
        state->action != WENA_CHECKLIST_SET_FLAGS) {
        if (state->length < 0 || !wena_model_title_valid(state->input, (size_t) state->length,
            WENA_CHECKLIST_TITLE_CAPACITY)) {
            state->error = 1;
            return;
        }
        state->input[state->length] = 0;
        if (state->action == WENA_CHECKLIST_ADD_ITEM) {
            if (!wena_checklist_item_titles_parse(state->input, (size_t) state->length, 0, 0,
                parsed, 1, &count) || count != 1) {
                state->error = 1;
                return;
            }
        }
        else strcpy(parsed[0], state->input);
    }
    memset(&mutation, 0, sizeof(mutation));
    mutation.action = state->action;
    mutation.checklist_id = state->checklist_id;
    mutation.item_id = state->item_id;
    mutation.expected_card_version = state->card_version;
    mutation.expected_checklist_version = state->checklist_version;
    mutation.expected_item_version = state->item_version;
    /* The adapter consumes these borrowed pointers during this call only. */
    mutation.title = (state->action == WENA_CHECKLIST_SET_FINISHED || state->action ==
        WENA_CHECKLIST_SET_FLAGS || state->action == WENA_CHECKLIST_ADD_ITEMS ||
        is_deletion(state->action) || is_ordering(state->action)) ? NULL : parsed[0];
    if (is_ordering(state->action)) mutation.target_position = (unsigned long)state->order_position;
    if (state->action == WENA_CHECKLIST_ADD_ITEMS) {
        mutation.batch_text = state->input;
        mutation.batch_length = (size_t)state->length;
    }
    mutation.is_finished = state->is_finished;
    mutation.hide_checked_items = state->hide_checked_items;
    mutation.hide_all_items = state->hide_all_items;
    mutation.show_on_minicard = state->preserved_show_on_minicard;
    if (!state->save || !state->save(state->context, state->board_id, state->card_id, &mutation)) {
        state->error = 1;
        return;
    }
    /* A committed write must never be resubmitted when the subsequent read fails. */
    state->action = 0;
    state->needs_refresh = 1;
    state->error = !reload_snapshot(state);
}

static int progress_for(const WenaChecklistSnapshot *snapshot, size_t index,
    WenaChecklistProgress *progress)
{
    WenaChecklistItem *items;
    size_t item_index, count;
    int valid;
    count = 0;
    items = NULL;
    if (snapshot->item_count != 0) {
        items = (WenaChecklistItem *)malloc(snapshot->item_count * sizeof(*items));
        if (items == NULL) return 0;
    }
    for (item_index = 0; item_index < snapshot->item_count; ++item_index) {
        if (strcmp(snapshot->items[item_index].checklist_id,
            snapshot->checklists[index].id) == 0) {
            items[count++] = snapshot->items[item_index];
        }
    }
    valid = wena_checklist_progress(&snapshot->checklists[index], items, count, progress);
    free(items);
    return valid;
}

int wena_checklists_render(struct nk_context *context, WenaChecklistsState *state,
    const WenaCard *cards, size_t card_count, float width, float height)
{
    size_t list_index, item_index, selected, batch_count;
    WenaChecklistProgress counts;
    int cancel, submit, checked, split;
    unsigned int keys;
    char progress[64];
    char batch_titles[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    if (!state || !state->visible) return 0;
    /* Selection is exact and unique. A removed, archived, or ambiguous card
     * closes the panel before any pending draft can reach the adapter. */
    selected = 0;
    if (cards != NULL) {
        for (list_index = 0; list_index < card_count; ++list_index) {
            if (!cards[list_index].archived &&
                strcmp(cards[list_index].id, state->card_id) == 0 &&
                strcmp(cards[list_index].board_id, state->board_id) == 0) {
                ++selected;
            }
        }
    }
    if (selected != 1) {
        wena_checklists_close(state);
        return 0;
    }
    if (!context || width <= 0 || height <= 0) return 0;
    cancel = 0;
    submit = 0;
    if (nk_begin_titled(context, "Card checklists", wena_ui_text(WENA_UI_TEXT_CHECKLISTS),
        nk_rect(width * 0.3f, 0, width * 0.7f, height), NK_WINDOW_BORDER)) {
        keys = wena_title_input_keys(context, 0u);
        if (keys & WENA_TITLE_INPUT_CANCEL) {
            nk_end(context);
            wena_checklists_close(state);
            return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        if (state->needs_refresh) {
            if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH))) state->error =
                !reload_snapshot(state);
        }
        else if (state->action) {
            if (is_deletion(state->action)) {
                nk_layout_row_dynamic(context, 48, 1);
                nk_label_wrap(context, wena_ui_text(state->action == WENA_CHECKLIST_DELETE ?
                    WENA_UI_TEXT_CONFIRM_DELETE_CHECKLIST : WENA_UI_TEXT_CONFIRM_DELETE_CHECKLIST_ITEM));
                nk_label_wrap(context, state->input);
                if (state->action == WENA_CHECKLIST_DELETE) {
                    selected = 0;
                    for (item_index = 0; item_index < state->snapshot->item_count; ++item_index)
                        if (strcmp(state->snapshot->items[item_index].checklist_id,
                            state->checklist_id) == 0) ++selected;
                    nk_label(context, wena_ui_text(WENA_UI_TEXT_CHECKLIST_WITH_ITEMS), NK_TEXT_LEFT);
                    sprintf(progress, "%lu", (unsigned long)selected);
                    nk_label(context, progress, NK_TEXT_LEFT);
                    /* Include hidden and completed children in the deletion scope. */
                    for (item_index = 0; item_index < state->snapshot->item_count; ++item_index)
                        if (strcmp(state->snapshot->items[item_index].checklist_id,
                            state->checklist_id) == 0)
                            nk_label_wrap(context, state->snapshot->items[item_index].title);
                }
            }
            else if (is_ordering(state->action)) {
                nk_label_wrap(context, state->input);
                nk_label(context, wena_ui_text(WENA_UI_TEXT_MANUAL_ORDER), NK_TEXT_LEFT);
                state->order_position = nk_combo_callback(context, order_label, progress,
                    state->order_position, state->order_count, 24, nk_vec2(180, 220));
                /* Enter does not confirm movement, including an open selector. */
            }
            else if (state->action == WENA_CHECKLIST_SET_FLAGS) {
                nk_checkbox_label(context, wena_ui_text(WENA_UI_TEXT_HIDE_CHECKED_ITEMS),
                    &state->hide_checked_items);
                nk_checkbox_label(context, wena_ui_text(WENA_UI_TEXT_HIDE_ALL_ITEMS),
                    &state->hide_all_items);
            }
            else if (state->action == WENA_CHECKLIST_SET_FINISHED) {
                nk_label_wrap(context, state->input);
                nk_checkbox_label(context, wena_ui_text(WENA_UI_TEXT_COMPLETE),
                    &state->is_finished);
            }
            else {
                nk_label(context, wena_ui_text(WENA_UI_TEXT_CHECKLIST), NK_TEXT_LEFT);
                if (state->action == WENA_CHECKLIST_ADD_ITEM ||
                    state->action == WENA_CHECKLIST_ADD_ITEMS) {
                    split = state->action == WENA_CHECKLIST_ADD_ITEMS;
                    if (nk_checkbox_label(context,
                        wena_ui_text(WENA_UI_TEXT_CHECKLIST_SPLIT_LINES), &split)) {
                        /* Switching modes never discards or truncates a draft. */
                        if (!split && (state->length < 0 ||
                            state->length >= WENA_CHECKLIST_TITLE_CAPACITY ||
                            memchr(state->input, '\n', (size_t)state->length) ||
                            memchr(state->input, '\r', (size_t)state->length)))
                            state->error = 1;
                        else state->action = split ? WENA_CHECKLIST_ADD_ITEMS :
                            WENA_CHECKLIST_ADD_ITEM;
                    }
                }
                if (state->action == WENA_CHECKLIST_ADD_ITEMS) {
                    batch_count = 0;
                    if (state->length >= 0) (void)wena_checklist_item_batch_parse(
                        state->input, (size_t)state->length, batch_titles, &batch_count);
                    sprintf(progress, "%lu / %lu", (unsigned long)batch_count,
                        (unsigned long)WENA_CHECKLIST_BATCH_MAX_ITEMS);
                    nk_label(context, wena_ui_text(WENA_UI_TEXT_CHECKLIST_WITH_ITEMS), NK_TEXT_LEFT);
                    nk_label(context, progress, NK_TEXT_LEFT);
                    nk_layout_row_dynamic(context, 120, 1);
                    (void)nk_edit_string(context, NK_EDIT_BOX, state->input,
                        &state->length, (int)sizeof(state->input), nk_filter_default);
                    /* Enter creates a line; only the explicit Save submits. */
                } else {
                    keys = nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                        state->input, &state->length,
                        WENA_NATIVE_EDIT_CAPACITY(WENA_CHECKLIST_TITLE_CAPACITY), nk_filter_default);
                    submit = (wena_title_input_keys(context, keys) & WENA_TITLE_INPUT_COMMIT) != 0u;
                }
            }
            nk_layout_row_dynamic(context, 28, 2);
            /* Enter never confirms deletion: only an explicit button click does. */
            if (nk_button_label(context, wena_ui_control_text(is_deletion(state->action) ?
                WENA_UI_CONFIRM_DELETE : WENA_UI_SAVE))) submit = 1;
            if (nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL))) {
                state->action = 0;
                state->error = 0;
                submit = 0;
            }
            if (state->action == WENA_CHECKLIST_RENAME ||
                state->action == WENA_CHECKLIST_RENAME_ITEM) {
                nk_layout_row_dynamic(context, 28, 1);
                if (nk_button_label(context, wena_ui_control_text(
                    state->action == WENA_CHECKLIST_RENAME ? WENA_UI_OPEN_DELETE_CHECKLIST :
                    WENA_UI_OPEN_DELETE_CHECKLIST_ITEM))) {
                    confirm_deletion(state);
                    submit = 0;
                }
                if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION))) {
                    begin_order(state);
                    submit = 0;
                }
            }
        }
        else {
            if (state->save && nk_button_label(context,
                wena_ui_control_text(WENA_UI_ADD_CHECKLIST))) begin_edit(state, WENA_CHECKLIST_CREATE,
                0, 0);
            for (list_index = 0; list_index < state->snapshot->checklist_count; ++list_index) {
                if (!progress_for(state->snapshot, list_index, &counts)) {
                    state->error = 1;
                    break;
                }
                sprintf(progress, "%lu / %lu (%u%%)", (unsigned long)counts.finished,
                    (unsigned long)counts.total, counts.percent);
                nk_layout_row_dynamic(context, 30, 1);
                nk_label_wrap(context, state->snapshot->checklists[list_index].title);
                nk_label(context, progress, NK_TEXT_LEFT);
                if (state->save) {
                    nk_layout_row_dynamic(context, 28, 2);
                    if (nk_button_label(context,
                        wena_ui_control_text(WENA_UI_RENAME_CHECKLIST))) begin_edit(state,
                        WENA_CHECKLIST_RENAME, list_index, 0);
                    if (nk_button_label(context,
                        wena_ui_control_text(WENA_UI_ADD_CHECKLIST_ITEM))) begin_edit(state,
                        WENA_CHECKLIST_ADD_ITEM, list_index, 0);
                }
                if (state->save) {
                    nk_layout_row_dynamic(context, 28, 1);
                    if (nk_button_label(context,
                        wena_ui_control_text(WENA_UI_CHECKLIST_SETTINGS))) begin_edit(state,
                        WENA_CHECKLIST_SET_FLAGS, list_index, 0);
                }
                for (item_index = 0; item_index < state->snapshot->item_count; ++item_index) {
                    if (!state->snapshot->checklists[list_index].hide_all_items &&
                        !(state->snapshot->checklists[list_index].hide_checked_items &&
                          state->snapshot->items[item_index].is_finished) &&
                        strcmp(state->snapshot->items[item_index].checklist_id,
                            state->snapshot->checklists[list_index].id) == 0) {
                        nk_layout_row_dynamic(context, 40, 1);
                        nk_label_wrap(context, state->snapshot->items[item_index].title);
                        nk_layout_row_dynamic(context, 28, state->save ? 2 : 1);
                        checked = state->snapshot->items[item_index].is_finished;
                        if (state->save) {
                            if (nk_checkbox_label(context,
                                wena_ui_text(WENA_UI_TEXT_COMPLETE), &checked)) {
                                begin_edit(state, WENA_CHECKLIST_SET_FINISHED,
                                    list_index, item_index);
                            }
                            if (nk_button_label(context,
                                wena_ui_control_text(WENA_UI_RENAME_CHECKLIST_ITEM))) {
                                begin_edit(state, WENA_CHECKLIST_RENAME_ITEM,
                                    list_index, item_index);
                            }
                        } else {
                            nk_label(context, checked ? "[x]" : "[ ]", NK_TEXT_LEFT);
                        }
                    }
                }
            }
        }
        if (state->error) {
            nk_layout_row_dynamic(context, 48, 1);
            nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        }
        nk_layout_row_dynamic(context, 28, 1);
        cancel = nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE));
    }
    nk_end(context);
    if (cancel) wena_checklists_close(state);
    else if (submit) submit_edit(state);
    return 1;
}
