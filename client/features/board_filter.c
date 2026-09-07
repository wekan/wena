#include "board_filter.h"
#include "card_details.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>

void wena_board_filter_init(WenaBoardFilterState *state)
{ if (state != NULL) memset(state, 0, sizeof(*state)); }
int wena_board_filter_sync(WenaBoardFilterState *state, const char *board_id)
{
    if (!state || !wena_model_identifier_valid(board_id)) return 0;
    if (strcmp(state->board_id, board_id)) {
        wena_board_filter_init(state); strcpy(state->board_id, board_id);
    }
    return 1;
}
int wena_board_filter_apply(WenaBoardFilterState *state)
{
    if (!state || !wena_model_identifier_valid(state->board_id)) return 0;
    if (state->length < 0 || state->length >= WENA_BOARD_FILTER_CAPACITY ||
        (state->length && !wena_model_title_valid(state->input,
            (size_t)state->length, WENA_BOARD_FILTER_CAPACITY))) {
        state->error = 1; return 0;
    }
    state->input[state->length] = '\0';
    memcpy(state->query, state->input, (size_t)state->length + 1u);
    state->error = 0; return 1;
}
void wena_board_filter_clear(WenaBoardFilterState *state)
{
    if (!state) return;
    state->query[0] = 0; state->input[0] = 0; state->length = 0; state->error = 0;
}
void wena_board_filter_cancel(WenaBoardFilterState *state)
{
    if (!state) return;
    strcpy(state->input, state->query);
    state->length = (int)strlen(state->query); state->error = 0;
}
static unsigned char folded(unsigned char byte)
{ return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte; }
int wena_board_filter_matches(void *context, const WenaCard *card)
{
    WenaBoardFilterState *state;
    size_t start, index, title_length, query_length;
    state = (WenaBoardFilterState *)context;
    if (!state || !card || card->archived ||
        !wena_model_identifier_valid(state->board_id) ||
        !wena_model_identifier_valid(card->board_id) ||
        strcmp(state->board_id, card->board_id) ||
        !wena_model_title_string_valid(card->title, sizeof(card->title))) return 0;
    query_length = strlen(state->query); title_length = strlen(card->title);
    if (!query_length) return 1;
    if (query_length > title_length) return 0;
    for (start = 0; start <= title_length - query_length; ++start) {
        for (index = 0; index < query_length; ++index)
            if (folded((unsigned char)card->title[start + index]) !=
                folded((unsigned char)state->query[index])) break;
        if (index == query_length) return 1;
    }
    return 0;
}
int wena_board_filter_render(struct nk_context *context, WenaBoardFilterState *state)
{
    char previous[WENA_BOARD_FILTER_CAPACITY];
    unsigned int keys, edit;
    int apply, clear;
    if (!context || !state || !wena_model_identifier_valid(state->board_id)) return 0;
    strcpy(previous, state->query);
    nk_layout_row_dynamic(context, 22, 1);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_FILTER_CARD_TITLE), NK_TEXT_LEFT);
    nk_layout_row_begin(context, NK_DYNAMIC, 28, 3);
    nk_layout_row_push(context, 0.6f);
    edit = nk_edit_string(context, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, state->input,
        &state->length, (int)sizeof(state->input), nk_filter_default);
    keys = wena_title_input_keys(context, edit);
    nk_layout_row_push(context, 0.2f);
    apply = nk_button_label(context, wena_ui_text(WENA_UI_TEXT_FILTER));
    nk_layout_row_push(context, 0.2f);
    clear = nk_button_label(context, wena_ui_text(WENA_UI_TEXT_FILTER_CLEAR));
    nk_layout_row_end(context);
    if (keys & WENA_TITLE_INPUT_CANCEL) wena_board_filter_cancel(state);
    else if (clear) wena_board_filter_clear(state);
    else if (apply || (keys & WENA_TITLE_INPUT_COMMIT)) (void)wena_board_filter_apply(state);
    if (state->error) {
        nk_layout_row_dynamic(context, 28, 1);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
    }
    return strcmp(previous, state->query) != 0;
}
