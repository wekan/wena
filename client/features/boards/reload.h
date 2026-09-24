#ifndef WENA_BOARD_RELOAD_H
#define WENA_BOARD_RELOAD_H
#include "../card_mutation.h"
#include "../../../server/sqlite_board.h"
/* Reload a complete owned board snapshot and synchronize its registered card
 * adapter. Failed reads preserve models/counts. Other adapters keep pointing at
 * the same snapshot/arrays; hosts close drafts before calling this function. */
int wena_board_reload(WenaCardMutation *cards,WenaSqliteBoardSnapshot *snapshot);
#endif
