#include "../client/features/card_details.h"
#include "../client/features/card_mutation.h"
#include "../server/sqlite_storage.h"
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
    FILE *file;
    unsigned char *sql;
    long length;
    sqlite3 *db;
    WenaCard card;
    WenaCardDetailsState state;
    WenaCardMutation adapter;
    char path[512], title[129];
    unsigned long version;
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    assert(argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
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
    frame(&state, &card, "Edit title", NULL);
    assert(state.editing_title && strcmp(state.title_input, "Stored title") == 0);
    frame(&state, &card, "Cancel", "Discarded");
    assert(!state.editing_title && strcmp(card.title, "Old cache") == 0);
    frame(&state, &card, "Edit title", NULL);
    frame(&state, &card, "Save", "A&B + \303\244");
    assert(!state.editing_title && !strcmp(card.title, "A&B + \303\244"));
    frame(&state, &card, "Edit title", NULL);
    assert(sqlite3_exec(db, "UPDATE cards SET version=version+1;",
                       NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &card, "Save", "Stale");
    assert(state.editing_title && state.title_error);
    assert(!strcmp(card.title, "A&B + \303\244"));
    frame(&state, &card, "Cancel", NULL);
    frame(&state, &card, "Edit title", NULL);
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
    frame(&state, &card, "Edit title", NULL);
    assert(state.title_version == 4ul && !strcmp(state.title_input, "Persisted"));
    frame(&state, &card, "Save", "After reopen");
    assert(!state.editing_title && !strcmp(card.title, "After reopen"));
    wena_card_details_set_archive_adapter(&state, wena_card_mutation_archive);
    assert(wena_card_details_open(&state, &card));
    assert(sqlite3_exec(db, "UPDATE cards SET version=version+1;",
                       NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &card, "Archive card", NULL);
    assert(state.visible && state.title_error && !card.archived);
    assert(wena_card_details_open(&state, &card));
    assert(sqlite3_exec(db, "CREATE TRIGGER reject_archive BEFORE INSERT "
        "ON idempotency_keys BEGIN SELECT RAISE(ABORT,'failure'); END;",
        NULL, NULL, NULL) == SQLITE_OK);
    frame(&state, &card, "Archive card", NULL);
    assert(state.visible && state.title_error && !card.archived);
    assert(sqlite3_exec(db, "DROP TRIGGER reject_archive;", NULL, NULL, NULL)
           == SQLITE_OK);
    frame(&state, &card, "Archive card", NULL);
    assert(!state.visible && card.archived);
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
