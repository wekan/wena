/* WeKan's All Boards page: sections and their counts, tiles that open, the
 * star, "Add Board", and boards that cannot be opened. */
#include "../client/components/boards/all_boards.h"
#include "../client/components/common/wekan_look.h"
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static WenaWekanBoardTile tiles[4];

static void tile(int index, const char *id, const char *title, int archived, int starred, int template_board, int openable)
{
    memset(&tiles[index], 0, sizeof(tiles[index]));
    strcpy(tiles[index].id, id);
    strcpy(tiles[index].title, title);
    strcpy(tiles[index].color, "nephritis");
    tiles[index].archived = archived;
    tiles[index].starred = starred;
    tiles[index].template_board = template_board;
    tiles[index].openable = openable;
}

static int control(const char *name)
{
    size_t count, index;
    const WenaUiControl *controls = wena_ui_controls(&count);
    for (index = 0; index < count; ++index)
        if (!strcmp(controls[index].name, name)) return 1;
    return 0;
}

static unsigned int frame(WenaAllBoardsView *view, const char *press, char *board)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = press;
    wena_ui_controls_begin();
    return wena_all_boards_render(&context, view, tiles, 4, "Ada", 1024.0f, 720.0f, board, 65);
}

int main(void)
{
    WenaAllBoardsView view;
    char board[65];
    tile(0, "b1", "Plans", 0, 1, 0, 1);
    tile(1, "b2", "Old plans", 1, 0, 0, 0);
    tile(2, "t1", "Templates", 0, 0, 1, 0);
    tile(3, "b3", "Work", 0, 0, 0, 1);
    memset(&view, 0, sizeof(view));
    /* Remaining: Add Board and the two open boards; WeKan's sections and the user. */
    assert(frame(&view, NULL, board) == WENA_ALL_BOARDS_NO_ACTION);
    assert(control("Add Board") && control("Plans") && control("Work") && !control("Old plans"));
    assert(control("Remaining") && control("Starred") && control("Home") && control("Templates") &&
           control("Archives") && control("All Boards") && control("Ada"));
    assert(wena_all_boards_in_section(&tiles[0], WENA_ALL_BOARDS_STARRED) &&
           !wena_all_boards_in_section(&tiles[3], WENA_ALL_BOARDS_STARRED));
    /* A tile opens its board. */
    assert(frame(&view, "Work", board) == WENA_ALL_BOARDS_OPEN && !strcmp(board, "b3"));
    assert(frame(&view, "Add Board", board) == WENA_ALL_BOARDS_ADD);
    /* The star: its board, and not a click that opens it. */
    assert(frame(&view, "Click to star this board. It will show up at top of your boards list.", board) ==
           WENA_ALL_BOARDS_STAR && !strcmp(board, "b1"));
    /* Sections: Archive shows the archived board, which cannot be opened. */
    assert(frame(&view, "Archives", board) == WENA_ALL_BOARDS_NO_ACTION && view.section == WENA_ALL_BOARDS_ARCHIVE);
    assert(frame(&view, NULL, board) == WENA_ALL_BOARDS_NO_ACTION && control("Old plans") && !control("Plans") &&
           !control("Add Board"));
    assert(frame(&view, "Old plans", board) == WENA_ALL_BOARDS_NO_ACTION && board[0] == '\0');
    /* Templates: the template container, and Add Board for them. */
    view.section = WENA_ALL_BOARDS_TEMPLATES;
    assert(frame(&view, NULL, board) == WENA_ALL_BOARDS_NO_ACTION && control("Templates") && control("Add Board"));
    /* Negative: Home has none; an out-of-range section falls back to Remaining. */
    view.section = WENA_ALL_BOARDS_HOME;
    assert(frame(&view, NULL, board) == WENA_ALL_BOARDS_NO_ACTION && !control("Plans") && !control("Add Board"));
    view.section = (WenaAllBoardsSection)42;
    assert(frame(&view, NULL, board) == WENA_ALL_BOARDS_NO_ACTION && view.section == WENA_ALL_BOARDS_REMAINING);
    /* WeKan's board colors, and belize for one it does not know. */
    assert(wena_all_boards_color("nephritis") == 0x27ae60 && wena_all_boards_color("pomegranate") == 0xc0392b);
    assert(wena_all_boards_color("nonsense") == 0x2980b9 && wena_all_boards_color(NULL) == 0x2980b9);
    puts("All Boards: sections, counts, open, star, Add Board, archive and colors passed");
    return 0;
}
