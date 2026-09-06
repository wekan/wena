#include "../models/wekan_models.h"

#include <assert.h>
#include <string.h>

static void fill_oversized(char *value, size_t size)
{
    memset(value, 'x', size - 1);
    value[size - 1] = '\0';
}

int main(void)
{
    WenaBoard board;
    WenaSwimlane swimlane;
    WenaList list;
    WenaCard card;
    char oversized_id[WENA_ID_CAPACITY + 1];
    char oversized_title[WENA_TITLE_CAPACITY + 1];
    char destination[4];

    fill_oversized(oversized_id, sizeof(oversized_id));
    fill_oversized(oversized_title, sizeof(oversized_title));

    assert(!wena_model_set_required(NULL, 1, "x"));
    assert(!wena_model_set_required(destination, 0, "x"));
    assert(!wena_model_set_required(destination, sizeof(destination), NULL));
    assert(!wena_model_set_required(destination, sizeof(destination), ""));
    assert(wena_model_set_optional(destination, sizeof(destination), ""));
    assert(destination[0] == '\0');
    assert(!wena_model_set_optional(destination, sizeof(destination), "long"));
    assert(destination[0] == '\0');

    assert(!wena_board_init(NULL, "id", "title", 0));
    assert(!wena_board_init(&board, NULL, "title", 0));
    assert(!wena_board_init(&board, "id", NULL, 0));
    assert(!wena_board_init(&board, oversized_id, "title", 0));
    assert(!wena_board_init(&board, "id", oversized_title, 0));
    assert(board.id[0] == '\0' && board.title[0] == '\0');

    assert(!wena_swimlane_init(NULL, "id", "board", "title", 0.0, 0));
    assert(!wena_swimlane_init(&swimlane, "", "board", "title", 0.0, 0));
    assert(!wena_swimlane_init(&swimlane, "id", "", "title", 0.0, 0));
    assert(!wena_swimlane_init(&swimlane, "id", "board", "", 0.0, 0));
    assert(swimlane.id[0] == '\0' && swimlane.board_id[0] == '\0');

    assert(!wena_list_init(NULL, "id", "board", "", "title", 0.0, 0));
    assert(!wena_list_init(&list, "id", "", "", "title", 0.0, 0));
    assert(!wena_list_init(&list, "id", "board", oversized_id,
                           "title", 0.0, 0));
    assert(!wena_list_init(&list, "id", "board", "", "", 0.0, 0));
    assert(list.id[0] == '\0' && list.board_id[0] == '\0');

    assert(!wena_card_init(NULL, "id", "board", "", "list",
                           "title", 0.0, 0));
    assert(!wena_card_init(&card, "id", "board", "", "", "title",
                           0.0, 0));
    assert(!wena_card_init(&card, "id", "board", oversized_id, "list",
                           "title", 0.0, 0));
    assert(!wena_card_init(&card, "id", "board", "", "list", NULL,
                           0.0, 0));
    assert(card.id[0] == '\0' && card.list_id[0] == '\0');
    return 0;
}
