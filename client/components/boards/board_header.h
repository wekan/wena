#ifndef WENA_BOARD_HEADER_H
#define WENA_BOARD_HEADER_H

#include "../../../models/board.h"

struct nk_context;

#define WENA_BOARD_HEADER_NO_ACTION 0u
#define WENA_BOARD_HEADER_OPEN_MENU 1u

/* Optional native vector decorator; NULL keeps the text-only baseline. */
typedef void (*WenaBoardTitleRenderer)(struct nk_context *, const char *title);
void wena_board_header_set_title_renderer(WenaBoardTitleRenderer renderer);

unsigned int wena_board_header_render(struct nk_context *context,
                                      const WenaBoard *board);

#endif
