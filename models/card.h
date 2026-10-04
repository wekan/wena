#ifndef WENA_CARD_H
#define WENA_CARD_H

#include "model.h"

typedef struct WenaCard {
    WenaId id;
    WenaId board_id;
    WenaId swimlane_id;
    WenaId list_id;
    WenaTitle title;
    double sort;
    int archived;
    /* A description is set: WeKan's description badge on the minicard. */
    int has_description;
    /* WeKan's fields Sort Cards orders by, for showing only: due and
     * creation times in milliseconds (0 when not set) and the vote score,
     * positive votes minus negative ones. */
    double due_at;
    double created_at;
    int votes;
    int has_due_at;
    int has_created_at;
} WenaCard;

int wena_card_init(WenaCard *card, const char *id, const char *board_id,
                   const char *swimlane_id, const char *list_id,
                   const char *title, double sort, int archived);

#endif
