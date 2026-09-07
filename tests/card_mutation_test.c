#include "../client/features/card_mutation.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void execute(sqlite3 *db, const char *sql)
{
    assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
}

static int scalar(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *statement;
    int n;
    assert(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    n = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return n;
}

int main(int argc, char **argv)
{
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE *file;
    unsigned char *sql;
    long length;
    sqlite3 *db;
    WenaCardMutation adapter;
    WenaCard card;
    WenaDomainCommand command;
    WenaRegionResponse response;
    char path[512], title[129], oversized[130];
    unsigned long version;
    size_t index;
    const char *bad_forms[] = {
        "title=%", "title=%0", "title=%GG", "title=%00", "title=%0A",
        "title=%7F", "title=One&title=Two", "title=%C0%AF", "title=%FF",
        "title=OK&broken", "title=", "title=one&&title=two",
        "title=%C2%80", "title=Bad%C2%85Title", "title=%C2%9F",
        "title=Bad\302\205Title"
    };
    assert(argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length); fclose(file);
    sprintf(path, "%s/native.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    execute(db, "INSERT INTO actors VALUES('u1','User',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO boards VALUES('b2','Other',1);"
        "INSERT INTO swimlanes VALUES('s1','b1','Lane',0,1);"
        "INSERT INTO lists VALUES('l1','b1','List',0,1);"
        "INSERT INTO cards VALUES('c1','b1','s1','l1','Original',0,0,1);");
    memset(&card, 0, sizeof(card)); strcpy(card.id, "c1");
    strcpy(card.board_id, "b1"); strcpy(card.title, "Stale cache");
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    assert(wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(!strcmp(title, "Original") && version == 1);
    /* Loading/cancelling never changes the cache or persistent version. */
    assert(!strcmp(card.title, "Stale cache"));
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 0);
    memset(oversized, 'x', sizeof(oversized)); oversized[129] = 0;
    assert(!wena_card_mutation_save_request(&adapter, "b1", "c1", 1, 0, "Bad id"));
    assert(!wena_card_mutation_save_request(&adapter, "b1", "c1", 1, ULONG_MAX, "Bad id"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 0, "Bad version"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", ULONG_MAX, "Bad version"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, "\302\200"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, ""));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, oversized));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, "bad\n"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, "bad\177"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, "\300\257"));
    assert(!wena_card_mutation_save(&adapter, "b2", "c1", 1, "Wrong"));
    assert(!wena_card_mutation_save(&adapter, "b1", "missing", 1, "Wrong"));
    assert(!wena_card_mutation_load(&adapter, "b2", "c1", title, sizeof(title), &version));
    assert(!wena_card_mutation_load(&adapter, "b1", "missing", title, sizeof(title), &version));
    assert(!wena_card_mutation_load(&adapter, "b1", "c1", title, 2, &version));
    assert(wena_card_mutation_save_request(&adapter, "b1", "c1", 1, 40, "A&B + 100% = \303\244"));
    assert(!strcmp(card.title, "A&B + 100% = \303\244"));
    assert(wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(version == 2 && !strcmp(title, card.title));
    assert(!wena_card_mutation_save_request(&adapter, "b1", "c1", 2, 40, "Replay"));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 1, "Stale"));
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 1);
    execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'injected'); END;");
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 2, "Rollback"));
    assert(!strcmp(card.title, "A&B + 100% = \303\244"));
    assert(scalar(db, "SELECT version FROM cards") == 2);
    execute(db, "DROP TRIGGER reject_metadata;");
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    assert(wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(version == 2 && !strcmp(title, card.title));
    assert(wena_card_mutation_save(&adapter, "b1", "c1", version, "Reopened"));
    assert(scalar(db, "SELECT max(request_version) FROM idempotency_keys") == 41);
    assert(!wena_card_mutation_save_request(&adapter, "b1", "c1", 3, 40, "Old replay"));
    assert(wena_card_mutation_init(&adapter, db, "unknown", "b1", &card, 1));
    assert(!wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 3, "Forbidden"));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    memset(&command, 0, sizeof(command));
    command.operation = WENA_DOMAIN_EDIT_CARD_TITLE;
    command.request_version = 99;
    strcpy(command.user_id, "u1"); strcpy(command.route, "/b/b1/native");
    for (index = 0; index < sizeof(bad_forms) / sizeof(bad_forms[0]); ++index) {
        sprintf(command.form_body, "cardId=c1&expectedVersion=3&%s", bad_forms[index]);
        command.form_body_length = strlen(command.form_body);
        assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    }
    assert(scalar(db, "SELECT version FROM cards") == 3);
    strcpy(command.form_body, "cardId=c1&expectedVersion=3&title=With+spaces%2Bsign%C2%A0");
    command.form_body_length = strlen(command.form_body);
    assert(wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    assert(!strcmp(response.regions[0].content, "With spaces+sign\302\240"));
    oversized[128] = 0;
    assert(wena_card_mutation_save(&adapter, "b1", "c1", 4, oversized));
    assert(strlen(card.title) == 128);
    assert(!wena_card_mutation_archive(&adapter, "b1", "c1", 4));
    assert(!wena_card_mutation_archive(&adapter, "b2", "c1", 5));
    assert(!wena_card_mutation_archive(&adapter, "b1", "missing", 5));
    execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'injected'); END;");
    assert(!wena_card_mutation_archive(&adapter, "b1", "c1", 5));
    assert(card.archived == 0);
    assert(scalar(db, "SELECT archived FROM cards") == 0);
    assert(scalar(db, "SELECT version FROM cards") == 5);
    execute(db, "DROP TRIGGER reject_metadata;");
    assert(wena_card_mutation_archive_request(&adapter, "b1", "c1", 5, 90));
    assert(card.archived == 1);
    assert(!wena_card_mutation_archive_request(&adapter, "b1", "c1", 6, 90));
    assert(!wena_card_mutation_archive(&adapter, "b1", "c1", 6));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", &card, 1));
    assert(scalar(db, "SELECT archived FROM cards") == 1);
    assert(scalar(db, "SELECT version FROM cards") == 6);
    assert(!wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(!wena_card_mutation_save(&adapter, "b1", "c1", 5, "Archived"));
    assert(sqlite3_close(db) == SQLITE_OK); free(sql);
    puts("native card mutation tests passed");
    return 0;
}
