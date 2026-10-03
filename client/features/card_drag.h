#ifndef WENA_CARD_DRAG_H
#define WENA_CARD_DRAG_H
#include "card_mutation.h"
#include "../../models/card_order.h"
#include "../../models/list.h"
#include "../../models/swimlane.h"
#include "../components/common/reorder_drag.h"
typedef struct WenaCardDrag {
    WenaReorderDrag gesture;
    WenaCardOrderSlot *order,*destination_order;
    size_t order_count,destination_count;
    WenaId board_id,list_id,swimlane_id;
    WenaId target_list_id,target_swimlane_id;
    int transfer,inserting;
    int error;
} WenaCardDrag;
void wena_card_drag_cancel(WenaCardDrag *state);
void wena_card_drag_begin(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,unsigned long source_revision);
/* Active gestures offer an exact insertion target before visible cards in
 * other columns. Rendering captures only model data; apply performs the write. */
void wena_card_drag_handle(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,const WenaCard *card,size_t ordinal,
    unsigned long card_revision,int enabled);
/* Explicit append target, including empty columns and another swimlane. */
void wena_card_drag_destination(struct nk_context *context,WenaCardDrag *state,
    const WenaList *list,const WenaSwimlane *lane);
/* WeKan's dragging: the whole minicard (area) is the handle; *clicked is a
 * press and release on it without dragging. A card of another list is where
 * a dragged card is inserted, before it. */
void wena_card_drag_area(struct nk_context *context,WenaCardDrag *state,
    const WenaCard *cards,size_t count,const WenaCard *card,size_t ordinal,
    unsigned long card_revision,int enabled,const struct nk_rect *area,int *clicked);
/* A list's free area: dropped there, the card goes to the end of that list. */
void wena_card_drag_destination_area(struct nk_context *context,WenaCardDrag *state,
    const WenaList *list,const WenaSwimlane *lane,const struct nk_rect *area);
/* The card being dragged, once it has moved; NULL otherwise. */
const char *wena_card_drag_source(const WenaCardDrag *state);
void wena_card_drag_end(struct nk_context *context,WenaCardDrag *state);
/* Consume once after drawing; existing mutation publishes cache only on commit. */
int wena_card_drag_apply(WenaCardDrag *state,WenaCardMutation *adapter);
#endif
