#include "card_details.h"

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
    }
}
