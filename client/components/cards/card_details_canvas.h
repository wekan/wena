#ifndef WENA_CARD_DETAILS_CANVAS_H
#define WENA_CARD_DETAILS_CANVAS_H

#include "../../../models/card.h"

struct nk_context;

#define WENA_CARD_DETAILS_NO_ACTION 0u
#define WENA_CARD_DETAILS_EDIT_TITLE 1u
#define WENA_CARD_DETAILS_ARCHIVE 2u
#define WENA_CARD_DETAILS_CLOSE 4u
#define WENA_CARD_DETAILS_MOVE 8u
#define WENA_CARD_DETAILS_DESCRIPTION 16u
#define WENA_CARD_DETAILS_CHECKLISTS 32u
#define WENA_CARD_DETAILS_LABELS 64u

unsigned int wena_card_details_canvas_render(struct nk_context *context,
                                             const WenaCard *card);

#endif
