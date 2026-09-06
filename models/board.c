#include "board.h"

#include <string.h>

int wena_board_init(WenaBoard *board, const char *id, const char *title,
                    int archived)
{
    if (board == NULL) {
        return 0;
    }
    memset(board, 0, sizeof(*board));
    if (!wena_model_set_required(board->id, sizeof(board->id), id) ||
        !wena_model_set_required(board->title, sizeof(board->title), title)) {
        memset(board, 0, sizeof(*board));
        return 0;
    }
    board->archived = archived != 0;
    return 1;
}
