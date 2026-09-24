#ifndef WENA_CARD_ORDER_H
#define WENA_CARD_ORDER_H
#include "card.h"
#define WENA_CARD_ORDER_CAPACITY 2048u
typedef struct WenaCardOrderSlot {
    WenaId id;
    double position;
    size_t model_index;
    int archived;
} WenaCardOrderSlot;
/* An ordered immutable column snapshot, including archived cards. Output slots
 * start NULL; success replaces/free old storage, failure preserves both outputs.
 * An empty column succeeds with NULL slots and zero count. Current() accepts
 * that empty snapshot only while the column remains empty.
 * Pure model operations: no database or GUI access. Caller frees slots. */
int wena_card_order_capture(const WenaCard *cards,size_t card_count,
    const char *board,const char *list,const char *lane,
    WenaCardOrderSlot **slots,size_t *slot_count);
int wena_card_order_current(const WenaCardOrderSlot *slots,size_t slot_count,
    const WenaCard *cards,size_t card_count,const char *board,const char *list,
    const char *lane);
/* Plan block insertion at a boundary in the ORIGINAL destination snapshot
 * (0 before its first slot, slot_count to append). IDs already in that column
 * are removed before insertion; other siblings, including archived, keep order.
 * The selected span supplies block order. Its IDs must be unique and active if
 * present in destination; callers validate source scope/revisions separately.
 * Output starts NULL or is owned by this API; success replaces/frees it, failure
 * preserves pointer/count. Inputs may alias prior output. No models are mutated. */
int wena_card_order_insert_selection(const WenaCardOrderSlot *slots,size_t slot_count,
    const WenaId *selected,size_t selected_count,size_t before,
    WenaId **ordered,size_t *ordered_count);
#endif
