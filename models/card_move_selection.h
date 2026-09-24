#ifndef WENA_CARD_MOVE_SELECTION_H
#define WENA_CARD_MOVE_SELECTION_H
#include "card_revision.h"
#include "card_order.h"
/* Immutable native move capture; caller owns storage. The fingerprint covers
 * every card on the board, while cards[] preserves the chosen block order. */
typedef struct WenaCardMoveSelection {
    WenaId board_id;
    char fingerprint[65];
    size_t count;
    WenaCardRevision cards[WENA_CARD_ORDER_CAPACITY];
} WenaCardMoveSelection;
/* Both board guards belong to the same read snapshot as source.cards[]. */
typedef struct WenaCardTransferSelection {
    WenaCardMoveSelection source;
    WenaId target_board_id;
    char target_fingerprint[65];
    unsigned long source_board_version,target_board_version;
} WenaCardTransferSelection;
#endif
