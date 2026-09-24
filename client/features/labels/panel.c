#include "panel.h"
#include "component.h"
#include "../../../imports/ui/page_contract.h"
#include "../../../models/text.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

void wena_labels_init(WenaLabelsState *state, WenaLabelsLoad load,
    WenaLabelsSave save, void *context)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->load = load;
    state->save = save;
    state->context = context;
}

void wena_labels_close(WenaLabelsState *state)
{
    if (!state) return;
    wena_label_snapshot_free(state->snapshot);
    state->snapshot = NULL;
    state->visible = 0;
    state->error = 0;
    state->needs_refresh = 0;
    state->action = 0;
    state->board_id[0] = 0;
    state->card_id[0] = 0;
    state->label_id[0] = 0;
    state->name[0] = 0;
    memset(&state->color_input,0,sizeof(state->color_input));
    state->name_length = 0;
}

static const char *selected_card(const WenaLabelsState *state)
{
    return state->card_id[0] ? state->card_id : NULL;
}

static int reload_snapshot(WenaLabelsState *state)
{
    WenaLabelSnapshot *replacement;
    replacement = wena_label_snapshot_create();
    if (!replacement) return 0;
    if (!state->load || !state->load(state->context, state->board_id,
        selected_card(state), replacement) || !wena_label_snapshot_valid(
            replacement, state->board_id, selected_card(state))) {
        wena_label_snapshot_free(replacement);
        return 0;
    }
    wena_label_snapshot_free(state->snapshot);
    state->snapshot = replacement;
    state->error = 0;
    state->needs_refresh = 0;
    state->action = 0;
    return 1;
}

int wena_labels_open(WenaLabelsState *state, const char *board_id,
    const WenaCard *card)
{
    if (!state) return 0;
    wena_labels_close(state);
    if (!wena_model_identifier_valid(board_id) || (card && (card->archived ||
        !wena_model_identifier_valid(card->id) ||
        !wena_model_identifier_valid(card->board_id) ||
        strcmp(card->board_id, board_id) != 0))) return 0;
    strcpy(state->board_id, board_id);
    if (card) strcpy(state->card_id, card->id);
    if (!reload_snapshot(state)) {
        wena_labels_close(state);
        return 0;
    }
    state->visible = 1;
    return 1;
}

static void begin_edit(WenaLabelsState *state, WenaLabelAction action,
    size_t label_index)
{
    const WenaColorContract *colors;
    size_t color_count, index, other;
    int used;
    state->action = action;
    state->error = 0;
    state->board_version = state->snapshot->board_version;
    state->card_version = state->snapshot->card_version;
    state->label_version = 0;
    state->label_id[0] = 0;
    state->name[0] = 0;
    state->name_length = 0;
    state->affected_cards = 0;
    if (action != WENA_LABEL_CREATE) {
        strcpy(state->label_id, state->snapshot->labels[label_index].id);
        strcpy(state->name, state->snapshot->labels[label_index].name);
        state->name_length = (int)strlen(state->name);
        state->label_version = state->snapshot->label_versions[label_index];
        state->affected_cards = state->snapshot->assigned_card_counts[label_index];
        (void)wena_color_input_set(&state->color_input, state->snapshot->labels[label_index].color);
        return;
    }
    /* WeKan chooses the first unused named color, then the first color if all
     * are in use. Equal colors remain legal for differently named labels. */
    colors = wena_colors(&color_count);
    (void)wena_color_input_set(&state->color_input, colors[0].name);
    for (index = 0; index < color_count; ++index) {
        used = 0;
        for (other = 0; other < state->snapshot->label_count; ++other)
            if (!strcmp(colors[index].name, state->snapshot->labels[other].color))
                used = 1;
        if (!used) {
            (void)wena_color_input_set(&state->color_input, colors[index].name);
            break;
        }
    }
}

static void confirm_deletion(WenaLabelsState *state)
{
    size_t index;
    for (index = 0; index < state->snapshot->label_count; ++index) {
        if (!strcmp(state->snapshot->labels[index].id, state->label_id)) {
            /* Restore the saved name/color; a discarded draft must never name
             * the destructive target or alter its captured revision. */
            strcpy(state->name, state->snapshot->labels[index].name);
            state->name_length = (int)strlen(state->name);
            (void)wena_color_input_set(&state->color_input, state->snapshot->labels[index].color);
            state->action = WENA_LABEL_DELETE;
            state->error = 0;
            return;
        }
    }
    state->error = 1;
}

static void submit_edit(WenaLabelsState *state)
{
    WenaLabelEdit edit;
    char name[WENA_LABEL_NAME_CAPACITY];
    const char *color;
    size_t start, length;
    memset(&edit, 0, sizeof(edit));
    edit.action = state->action;
    edit.label_id = state->label_id;
    edit.expected_board_version = state->board_version;
    edit.expected_label_version = state->label_version;
    edit.expected_card_version = state->card_version;
    if (state->action == WENA_LABEL_CREATE || state->action == WENA_LABEL_EDIT) {
        color = wena_color_input_value(&state->color_input);
        if (state->name_length < 0 || !wena_label_name_valid(state->name,
            (size_t)state->name_length) || !color ||
            !wena_model_text_trim_bounds(state->name, (size_t)state->name_length,
                &start, &length)) {
            state->error = 1;
            return;
        }
        memcpy(name, state->name + start, length);
        name[length] = 0;
        edit.name = name;
        edit.color = color;
    }
    if (!state->save || !state->save(state->context, state->board_id,
        selected_card(state), &edit)) {
        state->error = 1;
        if (state->action == WENA_LABEL_ASSIGN || state->action == WENA_LABEL_UNASSIGN)
            state->action = 0;
        return;
    }
    state->action = 0;
    state->needs_refresh = 1;
    state->error = !reload_snapshot(state);
}

static int scope_valid(const WenaLabelsState *state, const char *board_id,
    const WenaCard *cards, size_t card_count)
{
    size_t index, matches;
    if (!wena_model_identifier_valid(board_id) || strcmp(board_id, state->board_id))
        return 0;
    if (!state->card_id[0]) return 1;
    if (!cards) return 0;
    matches = 0;
    for (index = 0; index < card_count; ++index)
        if (!cards[index].archived && !strcmp(cards[index].id, state->card_id) &&
            !strcmp(cards[index].board_id, state->board_id)) ++matches;
    return matches == 1;
}

int wena_labels_render(struct nk_context *context, WenaLabelsState *state,
    const char *board_id, const WenaCard *cards, size_t card_count,
    float width, float height)
{
    size_t index;
    int submit, cancel;
    unsigned int keys;
    char count[32];
    if (!state || !state->visible) return 0;
    if (!scope_valid(state, board_id, cards, card_count)) {
        wena_labels_close(state);
        return 0;
    }
    if (!context || width <= 0 || height <= 0) return 0;
    submit = 0;
    cancel = 0;
    if (nk_begin_titled(context, "Board labels", wena_ui_text(WENA_UI_TEXT_LABELS),
        nk_rect(width * 0.3f, 0, width * 0.7f, height), NK_WINDOW_BORDER)) {
        if (wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) {
            nk_end(context);
            wena_labels_close(state);
            return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        if (state->needs_refresh) {
            if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH)))
                state->error = !reload_snapshot(state);
        } else if (state->action) {
            if (state->action == WENA_LABEL_DELETE) {
                nk_label(context, wena_ui_text(WENA_UI_TEXT_DELETE_LABEL), NK_TEXT_LEFT);
                (void)wena_label_badge_render(context, state->name, state->color_input.color);
                nk_label(context, wena_ui_text(WENA_UI_TEXT_CARDS), NK_TEXT_LEFT);
                sprintf(count, "%lu", state->affected_cards);
                nk_label(context, count, NK_TEXT_LEFT);
            } else {
                nk_label(context, wena_ui_text(WENA_UI_TEXT_NAME), NK_TEXT_LEFT);
                keys = nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                    state->name, &state->name_length, (int)sizeof(state->name),
                    nk_filter_default);
                /* Nuklear length-delimited input is not guaranteed terminated.
                 * This extra byte is only for preview, never for validation. */
                if (state->name_length >= 0 &&
                    (size_t)state->name_length < sizeof(state->name))
                    state->name[state->name_length] = 0;
                submit = (wena_title_input_keys(context, keys) &
                    WENA_TITLE_INPUT_COMMIT) != 0u;
                wena_color_input_render(context,&state->color_input,state->name,wena_label_badge_render);
            }
            nk_layout_row_dynamic(context, 28, 2);
            /* Destruction requires an explicit click in the confirmation view. */
            if (nk_button_label(context, wena_ui_control_text(state->action ==
                WENA_LABEL_DELETE ? WENA_UI_CONFIRM_DELETE : (state->action ==
                    WENA_LABEL_CREATE ? WENA_UI_CREATE_LABEL : WENA_UI_SAVE)))) submit = 1;
            if (nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL))) {
                state->action = 0;
                state->error = 0;
                submit = 0;
            }
            if (state->action == WENA_LABEL_EDIT) {
                nk_layout_row_dynamic(context, 28, 1);
                if (nk_button_label(context, wena_ui_control_text(WENA_UI_OPEN_DELETE_LABEL))) {
                    confirm_deletion(state);
                    submit = 0;
                }
            }
        } else {
            if (state->save && nk_button_label(context,
                wena_ui_control_text(WENA_UI_ADD_LABEL)))
                begin_edit(state, WENA_LABEL_CREATE, 0);
            if (!state->snapshot->label_count)
                nk_label(context, wena_ui_text(WENA_UI_TEXT_NO_ITEMS), NK_TEXT_LEFT);
            for (index = 0; index < state->snapshot->label_count; ++index) {
                nk_layout_row_begin(context, NK_DYNAMIC, 28, state->card_id[0] ? 3 : 2);
                if (state->card_id[0]) {
                    nk_layout_row_push(context, 0.08f);
                    nk_label(context, state->snapshot->assigned[index] ? "[x]" : "[ ]",
                        NK_TEXT_LEFT);
                }
                nk_layout_row_push(context, state->card_id[0] ? 0.60f : 0.68f);
                if (wena_label_badge_render(context, state->snapshot->labels[index].name,
                    state->snapshot->labels[index].color) && state->save && state->card_id[0]) {
                    begin_edit(state, state->snapshot->assigned[index] ?
                        WENA_LABEL_UNASSIGN : WENA_LABEL_ASSIGN, index);
                    submit = 1;
                }
                nk_layout_row_push(context, 0.32f);
                if (state->save && nk_button_label(context,
                    wena_ui_control_text(WENA_UI_EDIT_LABEL))) {
                    begin_edit(state, WENA_LABEL_EDIT, index);
                    submit = 0;
                }
                nk_layout_row_end(context);
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
    if (cancel) wena_labels_close(state);
    else if (submit) submit_edit(state);
    return 1;
}
