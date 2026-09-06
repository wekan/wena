#ifndef WENA_BOARD_H
#define WENA_BOARD_H

#include "model.h"

typedef struct WenaBoard {
    WenaId id;
    WenaTitle title;
    int archived;
} WenaBoard;

int wena_board_init(WenaBoard *board, const char *id, const char *title,
                    int archived);

#endif
