#include "../models/wekan_models.h"

#include <assert.h>
#include <string.h>

static void strict_validation_tests(void)
{
    const char *valid_utf8[] = {
        " ", "~", "\302\240", "\337\277", "\340\240\200",
        "\355\237\277", "\356\200\200", "\357\277\277",
        "\360\220\200\200", "\364\217\277\277", "A\303\244B"
    };
    const char *invalid_utf8[] = {
        "\200", "\277", "\300\257", "\301\277", "\302",
        "\340\200\200", "\341\200", "\341A\200",
        "\355\240\200", "\355\277\277", "\360\200\200\200",
        "\360\220\200", "\364\220\200\200", "\365\200\200\200", "\377"
    };
    char value[WENA_TITLE_CAPACITY + 1];
    char id[WENA_ID_CAPACITY + 1];
    char byte[2];
    char control[3];
    char embedded[3];
    size_t index;
    int expected;
    int code;

    assert(!wena_model_identifier_valid(NULL));
    assert(!wena_model_identifier_valid(""));
    assert(wena_model_identifier_valid("aZ09_-"));
    byte[1] = '\0';
    for (code = 1; code <= 255; ++code) {
        byte[0] = (char)code;
        expected = (code >= 'a' && code <= 'z') ||
            (code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9') ||
            code == '-' || code == '_';
        assert(wena_model_identifier_valid(byte) == expected);
    }
    memset(id, 'x', sizeof(id));
    id[WENA_ID_CAPACITY - 1] = '\0';
    assert(wena_model_identifier_valid(id));
    id[WENA_ID_CAPACITY - 1] = 'x';
    assert(!wena_model_identifier_valid(id));
    id[WENA_ID_CAPACITY] = '\0';
    assert(!wena_model_identifier_valid(id));
    assert(!wena_model_title_valid(NULL, 1, 129));
    assert(!wena_model_title_valid("", 0, 129));
    assert(!wena_model_title_valid("A", 1, 0));
    assert(!wena_model_title_valid("A", 1, 1));
    assert(wena_model_title_valid("A", 1, 2));
    assert(!wena_model_title_string_valid(NULL, 129));
    assert(!wena_model_title_string_valid("A", 0));
    for (code = 1; code < 128; ++code) {
        byte[0] = (char)code;
        expected = code >= 32 && code != 127;
        assert(wena_model_title_valid(byte, 1, 129) == expected);
        assert(wena_model_title_string_valid(byte, 129) == expected);
    }
    control[0] = (char)194; control[2] = '\0';
    for (code = 128; code <= 159; ++code) {
        control[1] = (char)code;
        assert(!wena_model_title_valid(control, 2, 129));
        assert(!wena_model_title_string_valid(control, 129));
    }
    for (index = 0; index < sizeof(valid_utf8) / sizeof(valid_utf8[0]); ++index) {
        assert(wena_model_title_valid(valid_utf8[index], strlen(valid_utf8[index]), 129));
        assert(wena_model_title_string_valid(valid_utf8[index], 129));
    }
    for (index = 0; index < sizeof(invalid_utf8) / sizeof(invalid_utf8[0]); ++index) {
        assert(!wena_model_title_valid(invalid_utf8[index], strlen(invalid_utf8[index]), 129));
        assert(!wena_model_title_string_valid(invalid_utf8[index], 129));
    }
    embedded[0] = 'A'; embedded[1] = '\0'; embedded[2] = 'B';
    assert(!wena_model_title_valid(embedded, sizeof(embedded), 129));
    assert(wena_model_title_string_valid(embedded, sizeof(embedded)));
    /* Caller-selected byte limits remain distinct: edited titles are 128 bytes,
       imported/display models and local actor names still permit 256 bytes. */
    memset(value, 'x', sizeof(value));
    assert(wena_model_title_valid(value, 128, 129));
    assert(!wena_model_title_valid(value, 129, 129));
    assert(wena_model_title_valid(value, 129, WENA_TITLE_CAPACITY));
    assert(wena_model_title_valid(value, 256, WENA_TITLE_CAPACITY));
    assert(!wena_model_title_valid(value, 257, WENA_TITLE_CAPACITY));
    assert(!wena_model_title_string_valid(value, 129));
    assert(!wena_model_title_string_valid(value, WENA_TITLE_CAPACITY));
    value[128] = '\0';
    assert(wena_model_title_string_valid(value, 129));
    value[128] = 'x'; value[256] = '\0';
    assert(!wena_model_title_string_valid(value, 129));
    assert(wena_model_title_string_valid(value, WENA_TITLE_CAPACITY));
    /* Bounded-copy helpers are intentionally not strict wire validators. */
    assert(wena_model_set_required(value, sizeof(value), "Copy\npermitted"));
    assert(wena_model_set_optional(value, sizeof(value), ""));
}

int main(void)
{
    WenaBoard board;
    WenaSwimlane swimlane;
    WenaList list;
    WenaCard card;
    char oversized_id[WENA_ID_CAPACITY + 1];

    strict_validation_tests();
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
    assert(!wena_list_init(&list, "list", board.id, NULL, "List", 1.0, 0));
    assert(list.id[0] == '\0');
    assert(!wena_board_init(&board, "", "Missing ID", 0));
    assert(!wena_card_init(&card, "card-2", "", "", "list-1",
                           "Missing board", 1.0, 0));

    memset(oversized_id, 'x', sizeof(oversized_id) - 1);
    oversized_id[sizeof(oversized_id) - 1] = '\0';
    assert(!wena_list_init(&list, "list", "board", oversized_id,
                            "List", 1.0, 0));
    assert(list.id[0] == '\0' && list.swimlane_id[0] == '\0');
    assert(wena_list_init(&list, "list", "board", "lane", "List", 1.0, 0));
    assert(strcmp(list.swimlane_id, "lane") == 0);
    assert(!wena_list_init(&list, "list", "", "", "List", 1.0, 0));
    assert(!wena_list_init(&list, "list", "board", "", "", 1.0, 0));
    assert(!wena_board_init(&board, oversized_id, "Too long", 0));
    assert(board.id[0] == '\0');
    return 0;
}
