#ifndef WENA_ALL_BOARDS_H
#define WENA_ALL_BOARDS_H
/* WeKan's All Boards page (client/components/boards/boardsList.jade), the
 * page WeKan opens on: the header, the left menu of sections with their
 * counts - Remaining, Starred, Home, Templates, Archive - and the board
 * tiles in the section's color, "Add Board" first, each with its star.
 * Measured from WeKan (tests/fixtures/wekan-ui/00-all-boards.json). */
#include "../../../server/wekan_sync.h"

struct nk_context;

typedef enum WenaAllBoardsSection {
    WENA_ALL_BOARDS_REMAINING,
    WENA_ALL_BOARDS_STARRED,
    WENA_ALL_BOARDS_HOME,
    WENA_ALL_BOARDS_TEMPLATES,
    WENA_ALL_BOARDS_ARCHIVE,
    WENA_ALL_BOARDS_SECTION_COUNT
} WenaAllBoardsSection;

typedef struct WenaAllBoardsView {
    WenaAllBoardsSection section;
    float scroll;         /* the tiles' vertical scroll */
} WenaAllBoardsView;

#define WENA_ALL_BOARDS_NO_ACTION 0u
#define WENA_ALL_BOARDS_OPEN 1u      /* a board tile: its id in `board` */
#define WENA_ALL_BOARDS_ADD 2u       /* the "Add Board" tile */
#define WENA_ALL_BOARDS_STAR 4u      /* a tile's star: its id in `board` */

/* Whether a board belongs to a section, as boardsList.js sorts them. */
int wena_all_boards_in_section(const WenaWekanBoardTile *tile, WenaAllBoardsSection section);
/* WeKan's tile color for a board color name (--board-theme-accent). */
int wena_all_boards_color(const char *name);

/* The page, the whole window; `user` is shown at the right of the header.
 * Returns WENA_ALL_BOARDS_* and writes the board a tile or star is of. */
unsigned int wena_all_boards_render(struct nk_context *context, WenaAllBoardsView *view,
                                    const WenaWekanBoardTile *tiles, size_t count, const char *user,
                                    float width, float height, char *board, size_t capacity);

#endif
