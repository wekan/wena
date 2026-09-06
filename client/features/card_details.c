#include "card_details.h"

#include <nuklear.h>
#include <stddef.h>
#include <string.h>

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
    if (!wena_model_set_required(state->card_id, sizeof(state->card_id),
                                 card->id)) {
        wena_card_details_close(state);
        return 0;
    }
    state->visible = 1;
    return 1;
}

void wena_card_details_close(WenaCardDetailsState *state)
{
    if (state != NULL) {
        state->visible = 0;
        state->card_id[0] = '\0';
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
    if (selected == NULL || selected->archived) {
        wena_card_details_close(state);
        return 0;
    }
    action = WENA_CARD_DETAILS_NO_ACTION;
    if (nk_begin(context, "Card details",
                 nk_rect(width * 0.5f, 0.0f, width * 0.5f, height),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        action = wena_card_details_canvas_render(context, selected);
    }
    nk_end(context);
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
