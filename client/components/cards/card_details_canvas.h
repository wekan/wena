#ifndef WENA_CARD_DETAILS_CANVAS_H
#define WENA_CARD_DETAILS_CANVAS_H

#include "../../../models/card.h"

struct nk_context;

#define WENA_CARD_DETAILS_NO_ACTION 0u
#define WENA_CARD_DETAILS_EDIT_TITLE 1u
#define WENA_CARD_DETAILS_ARCHIVE 2u
#define WENA_CARD_DETAILS_CLOSE 4u

unsigned int wena_card_details_canvas_render(struct nk_context *context,
                                             const WenaCard *card);

#endif
