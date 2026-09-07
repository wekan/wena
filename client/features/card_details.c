#include "card_details.h"

#include "../../imports/ui/page_contract.h"

#include <nuklear.h>
#include <stddef.h>
#include <string.h>

unsigned int wena_title_input_keys(struct nk_context *context,
                                    unsigned int edit_result)
{
    if (context == NULL || !nk_window_has_focus(context)) return 0u;
    /* The pinned SDL adapter maps Escape to Nuklear's text-reset key. */
    if (nk_input_is_key_pressed(&context->input, NK_KEY_TEXT_RESET_MODE))
        return WENA_TITLE_INPUT_CANCEL;
    return (edit_result & NK_EDIT_COMMITED) != 0u ? WENA_TITLE_INPUT_COMMIT : 0u;
}

int wena_card_details_title_valid(const char *title, size_t length)
{
    return wena_model_title_valid(title, length, WENA_CARD_DETAILS_TITLE_CAPACITY);
}

void wena_card_details_set_title_adapter(WenaCardDetailsState *state,
    WenaCardDetailsLoadTitle load, WenaCardDetailsSaveTitle save, void *context)
{
    if (state == NULL) return;
    wena_card_details_close(state);
    state->load_title = load;
    state->save_title = save;
    state->title_context = context;
    state->archive_card = NULL;
}

void wena_card_details_set_archive_adapter(WenaCardDetailsState *state,
    WenaCardDetailsArchive archive)
{
    if (state != NULL) {
        wena_card_details_close(state);
        state->archive_card = archive;
    }
}

void wena_card_details_init(WenaCardDetailsState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

int wena_card_details_open(WenaCardDetailsState *state, const WenaCard *card)
{
    if (state == NULL || card == NULL || card->archived) {
        return 0;
    }
    wena_card_details_close(state);
    if (!wena_model_set_required(state->board_id, sizeof(state->board_id),
                                  card->board_id) ||
        !wena_model_set_required(state->card_id, sizeof(state->card_id),
                                 card->id)) {
        wena_card_details_close(state);
        return 0;
    }
    state->visible = 1;
    if (state->load_title != NULL && state->archive_card != NULL) {
        if (!state->load_title(state->title_context, state->board_id,
            state->card_id, state->title_input,
            WENA_CARD_DETAILS_TITLE_CAPACITY, &state->title_version) ||
            state->title_version == 0ul) {
            state->title_version = 0ul;
            state->title_error = 1;
        }
    }
    return 1;
}

void wena_card_details_close(WenaCardDetailsState *state)
{
    if (state != NULL) {
        state->visible = 0;
        state->card_id[0] = '\0';
        state->board_id[0] = '\0';
        state->editing_title = 0;
        state->title_error = 0;
        state->title_length = 0;
        state->title_input[0] = '\0';
        state->title_version = 0ul;
        state->interaction.actions = WENA_CARD_DETAILS_NO_ACTION;
        state->interaction.card_id[0] = '\0';
    }
}

int wena_card_details_render(struct nk_context *context,
                             WenaCardDetailsState *state,
                             const WenaCard *cards, size_t card_count,
                             float width, float height)
{
    size_t index;
    const WenaCard *selected;
    unsigned int action;
    unsigned int edit_keys;
    int save_clicked;
    int cancel_clicked;

    if (state == NULL) {
        return 0;
    }
    state->interaction.actions = WENA_CARD_DETAILS_NO_ACTION;
    state->interaction.card_id[0] = '\0';
    if (!state->visible) {
        return 0;
    }
    if (context == NULL || (card_count != 0 && cards == NULL) ||
        width <= 0.0f || height <= 0.0f) {
        return 0;
    }
    selected = NULL;
    for (index = 0; index < card_count; ++index) {
        if (strcmp(cards[index].id, state->card_id) == 0) {
            selected = &cards[index];
            break;
        }
    }
    if (selected == NULL || selected->archived ||
        strcmp(selected->board_id, state->board_id) != 0) {
        wena_card_details_close(state);
        return 0;
    }
    action = WENA_CARD_DETAILS_NO_ACTION;
    if (nk_begin_titled(context, "Card details", wena_ui_text(WENA_UI_TEXT_CARD_DETAILS),
                 nk_rect(width * 0.5f, 0.0f, width * 0.5f, height),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        if (state->editing_title) {
            nk_layout_row_dynamic(context, 32.0f, 1);
            edit_keys = wena_title_input_keys(context,
                nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                    state->title_input, &state->title_length,
                    (int)sizeof(state->title_input), nk_filter_default));
            nk_layout_row_dynamic(context, 28.0f, 2);
            save_clicked = nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE));
            cancel_clicked = nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL));
            if (cancel_clicked || (edit_keys & WENA_TITLE_INPUT_CANCEL) != 0u) {
                state->editing_title = 0;
                state->title_error = 0;
                state->title_length = 0;
                state->title_input[0] = '\0';
            } else if (save_clicked || (edit_keys & WENA_TITLE_INPUT_COMMIT) != 0u) {
                if (state->title_length >= 0 &&
                    wena_card_details_title_valid(state->title_input,
                        (size_t)state->title_length)) {
                    state->title_input[state->title_length] = '\0';
                    if (state->save_title != NULL &&
                        state->save_title(state->title_context, state->board_id,
                            state->card_id, state->title_version,
                            state->title_input)) {
                        state->editing_title = 0;
                        state->title_error = 0;
                        ++state->title_version;
                    } else state->title_error = 1;
                } else state->title_error = 1;
            }
            /* A canonical generic failure avoids exposing storage internals. */
            if (state->title_error) {
                nk_layout_row_dynamic(context, 48.0f, 1);
                nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
            }
            nk_layout_row_dynamic(context, 28.0f, 1);
            if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE)))
                action = WENA_CARD_DETAILS_CLOSE;
        } else {
            action = wena_card_details_canvas_render(context, selected);
            if ((wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) != 0u)
                action = WENA_CARD_DETAILS_CLOSE;
            if ((action & WENA_CARD_DETAILS_EDIT_TITLE) != 0u &&
                state->load_title != NULL && state->save_title != NULL) {
                memset(state->title_input, 0, sizeof(state->title_input));
                state->title_version = 0ul;
                if (state->load_title(state->title_context, state->board_id,
                    state->card_id, state->title_input,
                    WENA_CARD_DETAILS_TITLE_CAPACITY, &state->title_version) &&
                    state->title_version != 0ul &&
                    memchr(state->title_input, '\0',
                        WENA_CARD_DETAILS_TITLE_CAPACITY) != NULL &&
                    wena_card_details_title_valid(state->title_input,
                        strlen(state->title_input))) {
                    state->title_length = (int)strlen(state->title_input);
                    state->editing_title = 1;
                    state->title_error = 0;
                } else state->title_error = 1;
            }
            if (state->title_error) {
                nk_layout_row_dynamic(context, 48.0f, 1);
                nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
            }
        }
    }
    nk_end(context);
    if ((action & WENA_CARD_DETAILS_ARCHIVE) != 0u &&
        state->archive_card != NULL) {
        if (state->title_version != 0ul &&
            state->archive_card(state->title_context, state->board_id,
                                state->card_id, state->title_version)) {
            wena_card_details_close(state);
        } else state->title_error = 1;
    }
    if ((action & WENA_CARD_DETAILS_CLOSE) != 0u) {
        wena_card_details_close(state);
    }
    if (action != WENA_CARD_DETAILS_NO_ACTION) {
        state->interaction.actions = action;
        (void)wena_model_set_required(state->interaction.card_id,
            sizeof(state->interaction.card_id), selected->id);
    }
    return 1;
}
