#ifndef WENA_LABEL_BADGES_H
#define WENA_LABEL_BADGES_H

#include "store.h"
#include "../../../models/card.h"

struct nk_context;
/* Uses a validated, complete cached snapshot. No persistence reads during draw.
 * Returns WENA_CARD_BODY_OPEN_LABELS when an assigned label is clicked. */
unsigned int wena_label_badges_render(struct nk_context *context,
    const WenaLabelBoardSnapshot *snapshot, const WenaCard *card);

#endif
