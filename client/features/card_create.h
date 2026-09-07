#ifndef WENA_CARD_CREATE_H
#define WENA_CARD_CREATE_H

#include "card_details.h"
#include "../components/boards/board_layout.h"

typedef int (*WenaCardCreateApply)(void *context, const char *board_id,
    const char *list_id, const char *swimlane_id, const char *title);

typedef struct WenaCardCreateState {
    int visible;
    int error;
    int title_length;
    char title_input[WENA_CARD_DETAILS_TITLE_CAPACITY + 1];
    WenaId board_id;
    WenaId list_id;
    WenaId swimlane_id;
    WenaCardCreateApply apply;
    void *context;
} WenaCardCreateState;

void wena_card_create_init(WenaCardCreateState *state,
    WenaCardCreateApply apply, void *context);
void wena_card_create_close(WenaCardCreateState *state);
int wena_card_create_open(WenaCardCreateState *state,
    const WenaBoardLayout *layout, const WenaListInteraction *interaction);
int wena_card_create_render(struct nk_context *context,
    WenaCardCreateState *state, const WenaBoardLayout *layout,
    float width, float height);
#endif
