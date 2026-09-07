#include "card_description.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>

void wena_card_description_init(WenaCardDescriptionState *state,
    WenaCardDescriptionLoad load, WenaCardDescriptionSave save, void *context)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->load = load; state->save = save; state->context = context;
}

void wena_card_description_close(WenaCardDescriptionState *state)
{
    if (state == NULL) return;
    state->visible = 0; state->error = 0; state->length = 0; state->version = 0;
    state->board_id[0] = '\0'; state->card_id[0] = '\0'; state->input[0] = '\0';
}

int wena_card_description_open(WenaCardDescriptionState *state, const WenaCard *card)
{
    if (state == NULL) return 0;
    wena_card_description_close(state);
    if (card == NULL || card->archived || state->load == NULL ||
        !wena_model_set_required(state->board_id, sizeof(state->board_id), card->board_id) ||
        !wena_model_set_required(state->card_id, sizeof(state->card_id), card->id)) return 0;
    memset(state->input, 0, sizeof(state->input));
    if (!state->load(state->context, state->board_id, state->card_id,
        state->input, WENA_DESCRIPTION_CAPACITY, &state->version) || state->version == 0 ||
        memchr(state->input, '\0', WENA_DESCRIPTION_CAPACITY) == NULL ||
        !wena_model_description_valid(state->input, strlen(state->input))) {
        wena_card_description_close(state); return 0;
    }
    state->length = (int)strlen(state->input); state->visible = 1; return 1;
}

int wena_card_description_render(struct nk_context *context,
    WenaCardDescriptionState *state, const WenaCard *cards, size_t card_count,
    float width, float height)
{
    size_t i;
    const WenaCard *selected;
    int save, cancel;
    unsigned int flags;
    float input_height;
    if (state == NULL || !state->visible) return 0;
    if (card_count != 0 && cards == NULL) return 0;
    selected = NULL;
    for (i = 0; i < card_count; ++i)
        if (!cards[i].archived && !strcmp(cards[i].id, state->card_id) &&
            !strcmp(cards[i].board_id, state->board_id)) {
            if (selected != NULL) { wena_card_description_close(state); return 0; }
            selected = &cards[i];
        }
    if (selected == NULL) { wena_card_description_close(state); return 0; }
    if (context == NULL || width <= 0 || height <= 0) return 0;
    save = 0; cancel = 0;
    if (nk_begin_titled(context, "Card description", wena_ui_control_text(WENA_UI_EDIT_DESCRIPTION),
        nk_rect(width * 0.3f, 0, width * 0.7f, height), NK_WINDOW_BORDER)) {
        if ((wena_title_input_keys(context, 0u) & WENA_TITLE_INPUT_CANCEL) != 0u) {
            nk_end(context); wena_card_description_close(state); return 1;
        }
        nk_layout_row_dynamic(context, 28, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_DESCRIPTION), NK_TEXT_LEFT);
        input_height = height - 160.0f;
        if (input_height < 80.0f) input_height = 80.0f;
        nk_layout_row_dynamic(context, input_height, 1);
        flags = NK_EDIT_BOX;
        if (state->save == NULL) flags |= NK_EDIT_READ_ONLY;
        /* Plain Enter belongs to multiline text, never to the Save action. */
        (void)nk_edit_string(context, flags, state->input, &state->length,
            (int)sizeof(state->input), nk_filter_default);
        nk_layout_row_dynamic(context, 28, state->save == NULL ? 2 : 3);
        if (state->save != NULL) save = nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE));
        cancel = nk_button_label(context, wena_ui_control_text(WENA_UI_CANCEL));
        if (nk_button_label(context, wena_ui_control_text(WENA_UI_CLOSE))) cancel = 1;
        if (state->error) {
            nk_layout_row_dynamic(context, 48, 1);
            nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        }
    }
    nk_end(context);
    if (cancel) wena_card_description_close(state);
    else if (save) {
        if (state->length >= 0 && wena_model_description_valid(state->input, (size_t)state->length)) {
            state->input[state->length] = '\0';
            if (state->save != NULL && state->save(state->context, state->board_id,
                state->card_id, state->version, state->input)) {
                wena_card_description_close(state); return 1;
            }
        }
        state->error = 1;
    }
    return 1;
}
