#include "card.h"

#include <string.h>

int wena_card_init(WenaCard *card, const char *id, const char *board_id,
                   const char *swimlane_id, const char *list_id,
                   const char *title, double sort, int archived)
{
    if (card == NULL) {
        return 0;
    }
    memset(card, 0, sizeof(*card));
    if (!wena_model_set_required(card->id, sizeof(card->id), id) ||
        !wena_model_set_required(card->board_id, sizeof(card->board_id),
                                 board_id) ||
        !wena_model_set_optional(card->swimlane_id,
                                 sizeof(card->swimlane_id), swimlane_id) ||
        !wena_model_set_required(card->list_id, sizeof(card->list_id),
                                 list_id) ||
        !wena_model_set_required(card->title, sizeof(card->title), title)) {
        memset(card, 0, sizeof(*card));
        return 0;
    }
    card->sort = sort;
    card->archived = archived != 0;
    return 1;
}
