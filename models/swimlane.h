#ifndef WENA_SWIMLANE_H
#define WENA_SWIMLANE_H

#include "model.h"
#include "color.h"

typedef struct WenaSwimlane {
    WenaId id;
    WenaId board_id;
    WenaTitle title;
    double sort;
    int archived;
    /* Empty uses the theme default; otherwise canonical item color or #RRGGBB. */
    char color[WENA_COLOR_CAPACITY];
} WenaSwimlane;

int wena_swimlane_init(WenaSwimlane *swimlane, const char *id,
                       const char *board_id, const char *title, double sort,
                       int archived);

#endif
