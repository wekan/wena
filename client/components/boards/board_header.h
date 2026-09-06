#ifndef WENA_BOARD_HEADER_H
#define WENA_BOARD_HEADER_H

#include "../../../models/board.h"

struct nk_context;

#define WENA_BOARD_HEADER_NO_ACTION 0u
#define WENA_BOARD_HEADER_OPEN_MENU 1u

unsigned int wena_board_header_render(struct nk_context *context,
                                      const WenaBoard *board);

#endif
