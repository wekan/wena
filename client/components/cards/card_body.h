#ifndef WENA_CARD_BODY_H
#define WENA_CARD_BODY_H

#include "../../../models/card.h"

struct nk_context;

#define WENA_CARD_BODY_NO_ACTION 0u
#define WENA_CARD_BODY_OPEN_DETAILS 1u
#define WENA_CARD_BODY_OPEN_MENU 2u
#define WENA_CARD_BODY_OPEN_LABELS 4u
#define WENA_CARD_BODY_OPEN_CHECKLISTS 8u
#define WENA_CARD_BODY_TOGGLE_SELECTION 16u
#define WENA_CARD_BODY_RANGE_SELECTION 32u

unsigned int wena_card_body_render(struct nk_context *context,
                                   const WenaCard *card);

unsigned int wena_card_body_render_selected(struct nk_context *context,
    const WenaCard *card,int selected);

#endif
