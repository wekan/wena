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
} WenaCard;

int wena_card_init(WenaCard *card, const char *id, const char *board_id,
                   const char *swimlane_id, const char *list_id,
                   const char *title, double sort, int archived);

#endif
