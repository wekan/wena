#ifndef WENA_CARD_MOVE_H
#define WENA_CARD_MOVE_H
#include "card_details.h"
#include "../../models/card_order.h"
#include "../components/boards/board_layout.h"

typedef int (*WenaCardMoveApply)(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version,
    const char *target_list_id, const char *target_swimlane_id);

#define WENA_CARD_MOVE_ORDER_CAPACITY WENA_CARD_ORDER_CAPACITY

typedef int (*WenaCardMoveReorder)(void *context, const char *board_id,
    const char *card_id, unsigned long expected_version, unsigned long target_position);
typedef WenaCardOrderSlot WenaCardMoveSlot;

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
    WenaCardMoveReorder reorder;
    WenaCardMoveSlot *order;
    size_t order_count;
    int reorder_choice;
    void *context;
} WenaCardMoveState;

void wena_card_move_init(WenaCardMoveState *state,
    WenaCardDetailsLoadTitle load, WenaCardMoveApply apply, void *context);
/* Optional same-column ordinal reordering. Shares the mutation context.
 * State owns a bounded heap snapshot after open; close it before reinitializing.
 * Choice zero preserves legacy append; positive choices include archived slots. */
void wena_card_move_set_reorder_adapter(WenaCardMoveState *state,
    WenaCardMoveReorder reorder);
void wena_card_move_close(WenaCardMoveState *state);
int wena_card_move_open(WenaCardMoveState *state,
    const WenaBoardLayout *layout, const char *card_id);
int wena_card_move_render(struct nk_context *context, WenaCardMoveState *state,
    const WenaBoardLayout *layout, float width, float height);
#endif
