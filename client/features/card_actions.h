#ifndef WENA_CARD_ACTIONS_H
#define WENA_CARD_ACTIONS_H
/* WeKan's Card Actions popup (cardDetailsActionsPopup), opened from a
 * minicard's and the card details' hamburger: the items Wena carries out, in
 * WeKan's order and groups, and what each one does. */
#include "card_details.h"
#include "card_move.h"
#include "../components/common/wekan_look.h"

typedef enum WenaCardAction {
    WENA_CARD_ACTION_MOVE_TO_TOP,
    WENA_CARD_ACTION_MOVE_TO_BOTTOM,
    WENA_CARD_ACTION_MOVE,
    WENA_CARD_ACTION_ARCHIVE,
    WENA_CARD_ACTION_COUNT
} WenaCardAction;

/* Fills `items` (WENA_CARD_ACTION_COUNT of them); an item whose adapter is
 * missing is shown disabled. Returns the count. */
size_t wena_card_actions_items(WenaWekanMenuItem *items, const WenaCardMoveState *move,
                               const WenaCardDetailsState *details);

#define WENA_CARD_ACTIONS_FAILED 0
#define WENA_CARD_ACTIONS_DONE 1
#define WENA_CARD_ACTIONS_OPEN_MOVE 2   /* the Move Card panel is open */
/* Carries out the chosen item for the card. */
int wena_card_actions_apply(WenaCardAction action, WenaCardMoveState *move,
                            const WenaCardDetailsState *details,
                            const WenaBoardLayout *layout, const char *card_id);
#endif
