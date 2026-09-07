#ifndef WENA_CARD_MOVE_H
#define WENA_CARD_MOVE_H
#include "card_details.h"
#include "../components/boards/board_layout.h"

typedef int (*WenaCardMoveApply)(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version,
    const char *target_list_id, const char *target_swimlane_id);

typedef struct WenaCardMoveState {
    int visible;
    int error;
    WenaId board_id;
    WenaId card_id;
    WenaId source_list_id;
    WenaId source_swimlane_id;
    WenaId target_list_id;
    WenaId target_swimlane_id;
    unsigned long version;
    WenaCardDetailsLoadTitle load;
    WenaCardMoveApply apply;
    void *context;
} WenaCardMoveState;

void wena_card_move_init(WenaCardMoveState *state,
    WenaCardDetailsLoadTitle load, WenaCardMoveApply apply, void *context);
void wena_card_move_close(WenaCardMoveState *state);
int wena_card_move_open(WenaCardMoveState *state,
    const WenaBoardLayout *layout, const char *card_id);
int wena_card_move_render(struct nk_context *context, WenaCardMoveState *state,
    const WenaBoardLayout *layout, float width, float height);
#endif
