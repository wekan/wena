#include "../client/features/card_create.h"
#include "../client/features/card_mutation.h"
#include "../client/components/lists/list_header.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void frame(WenaCardCreateState *state, WenaBoardLayout *layout,
    const char *button, const char *input)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button; context.edit_text = input;
    assert(wena_card_create_render(&context, state, layout, 800, 600));
}

int main(int argc, char **argv)
{
    sqlite3 *db;
    sqlite3_stmt *query;
    WenaCard cards[4];
    size_t count;
    WenaBoard board;
    WenaList list;
    WenaSwimlane lane;
    WenaBoardLayout layout;
    WenaListInteraction intent;
    WenaCardMutation adapter;
    WenaCardCreateState state;
    assert(argc == 2);
    assert(sqlite3_open(argv[1], &db) == SQLITE_OK);
    assert(sqlite3_exec(db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL) == SQLITE_OK);
    memset(cards, 0, sizeof(cards)); count = 0;
    assert(wena_board_init(&board, "board", "Board", 0));
    assert(wena_list_init(&list, "second-list", "board", "", "List", 0, 0));
    assert(wena_swimlane_init(&lane, "second-lane", "board", "Lane", 0, 0));
    memset(&layout, 0, sizeof(layout)); memset(&intent, 0, sizeof(intent));
    layout.board = &board; layout.lists = &list; layout.list_count = 1;
    layout.swimlanes = &lane; layout.swimlane_count = 1;
    layout.cards = cards;
    intent.actions = WENA_LIST_HEADER_ADD_CARD;
    strcpy(intent.board_id, "board"); strcpy(intent.list_id, "second-list");
    strcpy(intent.swimlane_id, "second-lane");
    assert(wena_card_mutation_init(&adapter, db, "actor", "board", cards, count));
    assert(wena_card_mutation_set_create_cache(&adapter, &count, 4));
    wena_card_create_init(&state, wena_card_mutation_create, &adapter);
    assert(wena_card_create_open(&state, &layout, &intent));
    frame(&state, &layout, "Cancel", "Not created");
    assert(count == 0);
    assert(wena_card_create_open(&state, &layout, &intent));
    frame(&state, &layout, "Save", "");
    assert(state.error && count == 0);
    frame(&state, &layout, "Save", "Created & + \303\244");
    assert(!state.visible && count == 1 && adapter.card_count == 1);
    assert(!strcmp(cards[0].title, "Created & + \303\244"));
    assert(!strcmp(cards[0].list_id, "second-list"));
    assert(!strcmp(cards[0].swimlane_id, "second-lane"));
    assert(wena_card_create_open(&state, &layout, &intent));
    assert(sqlite3_exec(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON "
        "idempotency_keys BEGIN SELECT RAISE(ABORT,'failure'); END;",
        NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &layout, "Save", "Rollback");
    assert(state.error && state.visible && count == 1 && adapter.card_count == 1);
    assert(sqlite3_exec(db, "DROP TRIGGER reject_metadata", NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &layout, "Save", NULL);
    assert(!state.visible && count == 2 && adapter.card_count == 2);
    assert(!strcmp(cards[1].title, "Rollback"));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(sqlite3_open(argv[1], &db) == SQLITE_OK);
    assert(sqlite3_prepare_v2(db, "SELECT title,list_id,swimlane_id FROM cards "
        "ORDER BY position", -1, &query, NULL) == SQLITE_OK);
    assert(sqlite3_step(query) == SQLITE_ROW);
    assert(!strcmp((const char *)sqlite3_column_text(query, 0), cards[0].title));
    assert(!strcmp((const char *)sqlite3_column_text(query, 1), "second-list"));
    assert(!strcmp((const char *)sqlite3_column_text(query, 2), "second-lane"));
    assert(sqlite3_step(query) == SQLITE_ROW);
    assert(!strcmp((const char *)sqlite3_column_text(query, 0), "Rollback"));
    assert(sqlite3_step(query) == SQLITE_DONE);
    sqlite3_finalize(query); assert(sqlite3_close(db) == SQLITE_OK);
    puts("Card creation UI and SQLite integration passed");
    return 0;
}
