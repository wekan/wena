#ifndef WENA_CARD_SELECTION_H
#define WENA_CARD_SELECTION_H
#include "card.h"
#define WENA_CARD_SELECTION_CAPACITY 2048u
typedef struct WenaCardSelection {
    WenaId board_id;
    WenaId ids[WENA_CARD_SELECTION_CAPACITY];
    size_t count;
} WenaCardSelection;
/* Caller owns storage. Initialization explicitly switches board and clears IDs.
 * Invalid inputs and allocation failures preserve the complete prior state.
 * No rendering/database dependencies; callers provide a complete board cache. */
int wena_card_selection_init(WenaCardSelection *selection,const char *board);
void wena_card_selection_clear(WenaCardSelection *selection);
int wena_card_selection_contains(const WenaCardSelection *selection,const char *id);
/* Successful updates prune missing/archived cards first. Add is a union, with
 * empty/NULL lane selecting all active cards in the list. Toggle requires an
 * active card in the selection's board. Existing order stays stable. */
int wena_card_selection_add(WenaCardSelection *selection,const WenaCard *cards,
    size_t count,const char *list,const char *lane);
int wena_card_selection_toggle(WenaCardSelection *selection,const WenaCard *cards,
    size_t count,const char *id);
int wena_card_selection_sync(WenaCardSelection *selection,const WenaCard *cards,size_t count);
#endif
