#include "settings_panel.h"
#include "../card_details.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>

void wena_board_settings_init(WenaBoardSettingsState *state,
    WenaBoardSettingsLoad load, WenaBoardSettingsSave save, void *context)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->load = load;
    state->save = save;
    state->context = context;
}

void wena_board_settings_close(WenaBoardSettingsState *state)
{
    if (!state) return;
    state->visible = 0;
    state->error = 0;
    state->needs_refresh = 0;
    state->show_checklist_count = 0;
    state->show_checklists = 1;
    state->board_id[0] = 0;
    memset(&state->snapshot, 0, sizeof(state->snapshot));
}

static int reload_settings(WenaBoardSettingsState *state)
{
    WenaBoardSettingsSnapshot replacement;
    memset(&replacement, 0, sizeof(replacement));
    if (!state->load || !state->load(state->context, state->board_id, &replacement) ||
        !wena_board_settings_snapshot_valid(&replacement, state->board_id)) return 0;
    state->snapshot = replacement;
    state->show_checklist_count = replacement.show_checklist_count;
    state->show_checklists = replacement.show_checklists;
    state->needs_refresh = 0;
    state->error = 0;
    return 1;
}

int wena_board_settings_open(WenaBoardSettingsState *state, const char *board_id)
{
    if (!state) return 0;
    wena_board_settings_close(state);
    if (!wena_model_identifier_valid(board_id)) return 0;
    strcpy(state->board_id, board_id);
    if (!reload_settings(state)) {
        wena_board_settings_close(state);
        return 0;
    }
    state->visible = 1;
    return 1;
}

static void boolean_field(struct nk_context *context, const char *label,
    int *value, int editable)
{
    if (editable) {
        nk_layout_row_dynamic(context, 28, 1);
        (void)nk_checkbox_label(context, label, value);
    } else {
        nk_layout_row_begin(context, NK_DYNAMIC, 48, 2);
        nk_layout_row_push(context, 0.08f);
        nk_label(context, *value ? "[x]" : "[ ]", NK_TEXT_LEFT);
        nk_layout_row_push(context, 0.92f);
        nk_label_wrap(context, label);
        nk_layout_row_end(context);
    }
}

int wena_board_settings_render(struct nk_context *context,
    WenaBoardSettingsState *state, const char *board_id, float width, float height)
{
    int save, cancel;
    if (!state || !state->visible) return 0;
    if (!wena_model_identifier_valid(board_id) || strcmp(state->board_id, board_id)) {
        wena_board_settings_close(state);
        return 0;
    }
    if (!context || width <= 0 || height <= 0) return 0;
    save = 0;
    cancel = 0;
    if (nk_begin_titled(context, "Board settings", wena_ui_text(WENA_UI_TEXT_SETTINGS),
        nk_rect(width * 0.3f, 0, width * 0.7f, height), NK_WINDOW_BORDER)) {
        if (wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) {
            nk_end(context);
            wena_board_settings_close(state);
            return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        if (state->needs_refresh) {
            if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH)))
                state->error = !reload_settings(state);
        } else {
            boolean_field(context, wena_ui_text(WENA_UI_TEXT_CHECKLIST_COUNT_ON_MINICARD),
                &state->show_checklist_count, state->save != NULL);
            nk_layout_row_dynamic(context, 28, 1);
            nk_label(context, wena_ui_text(WENA_UI_TEXT_SHOW_ON_MINICARD), NK_TEXT_LEFT);
            boolean_field(context, wena_ui_text(WENA_UI_TEXT_CHECKLISTS),
                &state->show_checklists, state->save != NULL);
            nk_layout_row_dynamic(context, 28, state->save ? 2 : 1);
            /* Enter never changes a checkbox draft or implicitly commits it. */
            if (state->save)
                save = nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE));
            cancel = nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL));
        }
        if (state->error) {
            nk_layout_row_dynamic(context, 48, 1);
            nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        }
        nk_layout_row_dynamic(context, 28, 1);
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE))) cancel = 1;
    }
    nk_end(context);
    if (cancel) {
        wena_board_settings_close(state);
    } else if (save) {
        if ((state->show_checklist_count != 0 && state->show_checklist_count != 1) ||
            (state->show_checklists != 0 && state->show_checklists != 1) ||
            !state->save || !state->save(state->context, state->board_id,
                state->snapshot.board_version, state->show_checklist_count, state->show_checklists)) {
            state->error = 1;
        } else {
            state->needs_refresh = 1;
            state->error = !reload_settings(state);
        }
    }
    return 1;
}
