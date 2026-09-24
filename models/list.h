#ifndef WENA_LIST_H
#define WENA_LIST_H

#include "model.h"
#include "color.h"
#include "wip_limit.h"

typedef struct WenaList {
    WenaId id;
    WenaId board_id;
    /* Empty means a board-wide WeKan list, shown in every active swimlane.
       Nonempty retains support for explicitly swimlane-scoped native lists. */
    WenaId swimlane_id;
    WenaTitle title;
    double sort;
    int archived;
    /* Empty uses the theme default; otherwise canonical item color or #RRGGBB. */
    char color[WENA_COLOR_CAPACITY];
    /* Defaults to value 1, disabled, hard. */
    WenaWipLimit wip_limit;
} WenaList;

int wena_list_init(WenaList *list, const char *id, const char *board_id,
                   const char *swimlane_id, const char *title, double sort,
                   int archived);

#endif
