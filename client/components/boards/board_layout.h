#ifndef WENA_BOARD_LAYOUT_H
#define WENA_BOARD_LAYOUT_H

#include "../../../models/wekan_models.h"
#include "../sidebar/board_sidebar.h"

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
    WenaBoardSidebar *sidebar;
    struct WenaListInteraction *list_interaction;
    struct WenaCardInteraction *card_interaction;
} WenaBoardLayout;

typedef struct WenaListInteraction {
    unsigned int actions;
    WenaId list_id;
} WenaListInteraction;

typedef struct WenaCardInteraction {
    unsigned int actions;
    WenaId card_id;
} WenaCardInteraction;

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout);

#endif
