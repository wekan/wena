#ifndef WENA_SWIMLANE_H
#define WENA_SWIMLANE_H

#include "model.h"

typedef struct WenaSwimlane {
    WenaId id;
    WenaId board_id;
    WenaTitle title;
    double sort;
    int archived;
} WenaSwimlane;

int wena_swimlane_init(WenaSwimlane *swimlane, const char *id,
                       const char *board_id, const char *title, double sort,
                       int archived);

#endif
