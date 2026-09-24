#include "reload.h"
#include <string.h>
int wena_board_reload(WenaCardMutation *cards,WenaSqliteBoardSnapshot *snapshot)
{
    if (!cards || !snapshot || !cards->persistence.database ||
        cards->cards!=snapshot->cards || strcmp(cards->board_id,snapshot->board.id) ||
        (cards->published_card_count &&
         (cards->published_card_count!=&snapshot->card_count ||
          cards->card_capacity!=WENA_SQLITE_BOARD_MAX_CARDS))) return 0;
    if (!wena_sqlite_board_load(cards->persistence.database,cards->board_id,snapshot)) return 0;
    cards->card_count=snapshot->card_count;
    return 1;
}
