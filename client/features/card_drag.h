#ifndef WENA_CARD_DRAG_H
#define WENA_CARD_DRAG_H
#include "card_mutation.h"
#include "../../models/card_order.h"
#include "../../models/list.h"
#include "../../models/swimlane.h"
#include "../components/common/reorder_drag.h"
typedef struct WenaCardDrag {
    WenaReorderDrag gesture;
    WenaCardOrderSlot *order;
    size_t order_count;
    WenaId board_id,list_id,swimlane_id;
    WenaId target_list_id,target_swimlane_id;
    int transfer;
    int error;
} WenaCardDrag;
void wena_card_drag_cancel(WenaCardDrag *state);
void wena_card_drag_begin(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,unsigned long source_revision);
void wena_card_drag_handle(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,const WenaCard *card,size_t ordinal,
    unsigned long card_revision,int enabled);
/* Explicit append target, including empty columns and another swimlane. */
void wena_card_drag_destination(struct nk_context *context,WenaCardDrag *state,
    const WenaList *list,const WenaSwimlane *lane);
void wena_card_drag_end(struct nk_context *context,WenaCardDrag *state);
/* Consume once after drawing; existing mutation publishes cache only on commit. */
int wena_card_drag_apply(WenaCardDrag *state,WenaCardMutation *adapter);
#endif
