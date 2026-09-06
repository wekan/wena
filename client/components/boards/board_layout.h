#ifndef WENA_BOARD_LAYOUT_H
#define WENA_BOARD_LAYOUT_H

#include "../../../models/wekan_models.h"

#include <stddef.h>

struct nk_context;

typedef struct WenaBoardLayout {
    const WenaBoard *board;
    const WenaSwimlane *swimlanes;
    size_t swimlane_count;
    const WenaList *lists;
    size_t list_count;
    const WenaCard *cards;
    size_t card_count;
} WenaBoardLayout;

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout);

#endif
