#ifndef WENA_BOARD_HEADER_H
#define WENA_BOARD_HEADER_H

#include "../../../models/board.h"

struct nk_context;

/* WeKan's header bar: blue, two rows. The first has the board title (click to
 * rename) and, at the right, Filter; the second the user (its menu has the
 * language) and the sidebar toggle - each where WeKan has it
 * (tests/fixtures/wekan-ui/01-board.json). */
#define WENA_BOARD_HEADER_NO_ACTION 0u
#define WENA_BOARD_HEADER_OPEN_MENU 1u     /* the sidebar toggle */
#define WENA_BOARD_HEADER_RENAME 2u        /* the board title */
#define WENA_BOARD_HEADER_FILTER 4u
#define WENA_BOARD_HEADER_MEMBER_MENU 8u   /* the user's name */
#define WENA_BOARD_HEADER_ALL_BOARDS 16u   /* the house before the title */
#define WENA_BOARD_HEADER_HEIGHT 88.0f

typedef struct WenaBoardHeaderInfo {
    const char *actor_name;   /* NULL hides the user */
    int filter_active;        /* "Filter is on" */
    int all_boards;           /* WeKan's house to All Boards before the title */
} WenaBoardHeaderInfo;

/* Optional native vector decorator; NULL keeps the text-only baseline. */
typedef void (*WenaBoardTitleRenderer)(struct nk_context *, const char *title);
void wena_board_header_set_title_renderer(WenaBoardTitleRenderer renderer);

unsigned int wena_board_header_render(struct nk_context *context,
                                      const WenaBoard *board);
unsigned int wena_board_header_render_info(struct nk_context *context,
                                           const WenaBoard *board,
                                           const WenaBoardHeaderInfo *info);

#endif
