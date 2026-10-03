#include "support/test_files.h"
#include "../client/features/card_details.h"
#include "../client/features/card_mutation.h"
#include "../client/features/card_actions.h"
#include "../server/sqlite_storage.h"
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void frame(WenaCardDetailsState *state, WenaCard *card,
    const char *button, const char *input)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    context.edit_text = input;
    assert(wena_card_details_render(&context, state, card, 1, 800, 600));
}

int main(int argc, char **argv)
{
    unsigned char *sql;
    long length;
    sqlite3 *db;
    WenaCard card;
    WenaCardDetailsState state;
    WenaCardMutation adapter;
    WenaCardMoveState move;
    WenaBoardLayout layout;
    WenaBoard board;
    char path[512], title[129];
    unsigned long version;
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    assert(argc == 3);
    sql = (unsigned char *)wena_test_file_copy(argv[1], &length);
    assert(length > 0);
    assert(strlen(argv[2]) < 450);
    sprintf(path, "%s/editor.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(sqlite3_exec(db,
        "INSERT INTO actors VALUES('u1','User',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO swimlanes VALUES('s1','b1','Lane',0,1);"
        "INSERT INTO lists VALUES('l1','b1','List',0,1);"
        "INSERT INTO cards VALUES('c1','b1','s1','l1','Stored title',0,0,1);",
        NULL, NULL, NULL) == SQLITE_OK);
    assert(wena_card_init(&card, "c1", "b1", "s1", "l1", "Old cache", 0, 0));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    wena_card_details_init(&state);
    wena_card_details_set_title_adapter(&state, wena_card_mutation_load,
                                       wena_card_mutation_save, &adapter);
    assert(wena_card_details_open(&state, &card));
    frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(state.editing_title && strcmp(state.title_input, "Stored title") == 0);
    frame(&state, &card, "Cancel", "Discarded");
    assert(!state.editing_title && strcmp(card.title, "Old cache") == 0);
    frame(&state, &card, card.title, NULL); /* the title edits itself */
    frame(&state, &card, "Save", "A&B + \303\244");
    assert(!state.editing_title && !strcmp(card.title, "A&B + \303\244"));
    frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(sqlite3_exec(db, "UPDATE cards SET version=version+1;",
                       NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &card, "Save", "Stale");
    assert(state.editing_title && state.title_error);
    assert(!strcmp(card.title, "A&B + \303\244"));
    frame(&state, &card, "Cancel", NULL);
    frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(sqlite3_exec(db, "CREATE TRIGGER reject_metadata BEFORE INSERT "
        "ON idempotency_keys BEGIN SELECT RAISE(ABORT,'failure'); END;",
        NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &card, "Save", "Rollback");
    assert(state.editing_title && state.title_error);
    assert(!strcmp(card.title, "A&B + \303\244"));
    assert(sqlite3_exec(db, "DROP TRIGGER reject_metadata;", NULL, NULL, NULL)
           == SQLITE_OK);
    frame(&state, &card, "Save", "Persisted");
    assert(!state.editing_title && !strcmp(card.title, "Persisted"));
    wena_card_details_close(&state);
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    assert(wena_card_mutation_load(&adapter, "b1", "c1", title,
                                   sizeof(title), &version));
    assert(!strcmp(title, "Persisted") && version == 4ul);
    assert(wena_card_details_open(&state, &card));
    frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(state.title_version == 4ul && !strcmp(state.title_input, "Persisted"));
    frame(&state, &card, "Save", "After reopen");
    assert(!state.editing_title && !strcmp(card.title, "After reopen"));
    /* Archive is in WeKan's Card Actions, not the details panel: a write
     * that rolls back leaves the card and the open details as they were, and
     * one that succeeds archives it and closes them. */
    wena_card_details_set_archive_adapter(&state, wena_card_mutation_archive);
    assert(wena_card_details_open(&state, &card));
    memset(&layout, 0, sizeof(layout));
    assert(wena_board_init(&board, "b1", "Board", 0));
    layout.board = &board; layout.cards = &card; layout.card_count = 1;
    wena_card_move_init(&move, NULL, NULL, NULL);
    assert(sqlite3_exec(db, "CREATE TRIGGER reject_archive BEFORE INSERT "
        "ON idempotency_keys BEGIN SELECT RAISE(ABORT,'failure'); END;",
        NULL, NULL, NULL) == SQLITE_OK);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_ARCHIVE, &move, &state, &layout, "c1") ==
           WENA_CARD_ACTIONS_FAILED);
    frame(&state, &card, NULL, NULL);
    assert(state.visible && !card.archived);
    assert(sqlite3_exec(db, "DROP TRIGGER reject_archive;", NULL, NULL, NULL)
           == SQLITE_OK);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_ARCHIVE, &move, &state, &layout, "c1") ==
           WENA_CARD_ACTIONS_DONE);
    assert(card.archived);
    {
        struct nk_context context;
        memset(&context, 0, sizeof(context));
        assert(!wena_card_details_render(&context, &state, &card, 1, 800, 600));
        assert(!state.visible);
    }
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    assert(!wena_card_mutation_load(&adapter, "b1", "c1", title,
                                    sizeof(title), &version));
    assert(!wena_card_details_open(&state, &card));
    assert(sqlite3_close(db) == SQLITE_OK);
    free(sql);
    puts("card editor SQLite integration passed");
    return 0;
}
