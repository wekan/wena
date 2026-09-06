#ifndef WENA_CARD_DETAILS_FEATURE_H
#define WENA_CARD_DETAILS_FEATURE_H

#include "../../models/card.h"

typedef struct WenaCardDetailsState {
    int visible;
    WenaId card_id;
} WenaCardDetailsState;

void wena_card_details_init(WenaCardDetailsState *state);
int wena_card_details_open(WenaCardDetailsState *state, const WenaCard *card);
void wena_card_details_close(WenaCardDetailsState *state);

#endif
