#include "../models/wekan_models.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    WenaBoard board;
    WenaSwimlane swimlane;
    WenaList list;
    WenaCard card;
    char oversized_id[WENA_ID_CAPACITY + 1];

    assert(wena_board_init(&board, "board-1", "Demo board", 0));
    assert(wena_swimlane_init(&swimlane, "swimlane-1", board.id,
                              "Default", 1.0, 0));
    assert(wena_list_init(&list, "list-1", board.id, swimlane.id,
                          "Doing", 2.0, 0));
    assert(wena_card_init(&card, "card-1", board.id, swimlane.id, list.id,
                          "Native card", 3.0, 2));
    assert(strcmp(card.board_id, board.id) == 0);
    assert(strcmp(card.swimlane_id, swimlane.id) == 0);
    assert(strcmp(card.list_id, list.id) == 0);
    assert(card.archived == 1);

    assert(wena_list_init(&list, "shared-list", board.id, "",
                          "Shared", 4.0, 0));
    assert(list.swimlane_id[0] == '\0');
    assert(!wena_board_init(&board, "", "Missing ID", 0));
    assert(!wena_card_init(&card, "card-2", "", "", "list-1",
                           "Missing board", 1.0, 0));

    memset(oversized_id, 'x', sizeof(oversized_id) - 1);
    oversized_id[sizeof(oversized_id) - 1] = '\0';
    assert(!wena_board_init(&board, oversized_id, "Too long", 0));
    assert(board.id[0] == '\0');
    return 0;
}
