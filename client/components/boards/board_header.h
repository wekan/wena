#ifndef WENA_BOARD_HEADER_H
#define WENA_BOARD_HEADER_H

#include "../../../models/board.h"

struct nk_context;

/* WeKan's header bar: blue, two rows. The first has the board title (click to
 * rename), the star group and, at the right, Filter; the second the user (its menu has the
 * language) and the sidebar toggle - each where WeKan has it
 * (tests/fixtures/wekan-ui/01-board.json). */
#define WENA_BOARD_HEADER_NO_ACTION 0u
#define WENA_BOARD_HEADER_OPEN_MENU 1u     /* the sidebar toggle */
#define WENA_BOARD_HEADER_RENAME 2u        /* the board title */
#define WENA_BOARD_HEADER_FILTER 4u
#define WENA_BOARD_HEADER_MEMBER_MENU 8u   /* the user's name */
#define WENA_BOARD_HEADER_ALL_BOARDS 16u   /* the house before the title */
#define WENA_BOARD_HEADER_STARRED 32u      /* the caret and count: what is starred */
#define WENA_BOARD_HEADER_STAR 64u         /* the board's star */
#define WENA_BOARD_HEADER_MULTI_SELECTION 128u /* Multi-Selection, second row */
#define WENA_BOARD_HEADER_VISIBILITY 256u  /* Private / Public */
#define WENA_BOARD_HEADER_WATCH 512u       /* Watching / Tracking / Muted */
#define WENA_BOARD_HEADER_SEARCH 1024u     /* Search, last on the first row */
#define WENA_BOARD_HEADER_SORT 2048u       /* Sort Cards */
#define WENA_BOARD_HEADER_SORT_RESET 4096u /* the cross removing the sort */
#define WENA_BOARD_HEADER_HEIGHT 88.0f

typedef struct WenaBoardHeaderInfo {
    const char *actor_name;   /* NULL hides the user */
    int filter_active;        /* "Filter is on" */
    int all_boards;           /* WeKan's house to All Boards before the title */
    int star;                 /* the star group: 0 none, 1 not starred, 2 starred */
    int starred_count;        /* places the user keeps starred */
    int board_stars;          /* the board's stars, shown from 2 as WeKan */
    int multi_selection;      /* 0 hides Multi-Selection, 1 shows it, 2 active */
    int permission;           /* 0 hides it, 1 Private, 2 Public */
    int watch;                /* 0 hides it, 1 Watching, 2 Tracking, 3 Muted */
    int search;               /* 0 hides Search, 1 shows it, 2 its sidebar is open */
    int sort;                 /* 0 hides Sort Cards, 1 shows it, 2 a sort is on */
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
