#ifndef WENA_CARD_DETAILS_FEATURE_H
#define WENA_CARD_DETAILS_FEATURE_H

#include "../../models/card.h"
#include "../components/cards/card_details_canvas.h"

#include <stddef.h>

struct nk_context;

typedef struct WenaCardDetailsInteraction {
    unsigned int actions;
    WenaId card_id;
} WenaCardDetailsInteraction;

typedef struct WenaCardDetailsState {
    int visible;
    WenaId card_id;
    WenaCardDetailsInteraction interaction;
} WenaCardDetailsState;

void wena_card_details_init(WenaCardDetailsState *state);
int wena_card_details_open(WenaCardDetailsState *state, const WenaCard *card);
void wena_card_details_close(WenaCardDetailsState *state);
int wena_card_details_render(struct nk_context *context,
                             WenaCardDetailsState *state,
                             const WenaCard *cards, size_t card_count,
                             float width, float height);

#endif
