#include "../client/features/card_mutation.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
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
    int value;
    assert(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    value = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

int main(int argc, char **argv)
{
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE *file;
    unsigned char *sql;
    long length;
    sqlite3 *db;
    WenaCardMutation adapter, other;
    WenaCard cards[3], before[3];
    WenaDomainCommand command;
    WenaRegionResponse response;
    char path[512], title[129];
    size_t count;
    unsigned long version;
    assert(argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length); fclose(file);
    sprintf(path, "%s/restore-card.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    execute(db, "INSERT INTO actors VALUES('u1','One',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO boards VALUES('b2','Other',1);"
        "INSERT INTO swimlanes VALUES('s1','b1','Lane',0,1);"
        "INSERT INTO lists VALUES('l1','b1','List',0,1);"
        "INSERT INTO cards VALUES('c1','b1','s1','l1','Archived',5,1,1);"
        "INSERT INTO cards VALUES('c2','b1','s1','l1','Active',0,0,1);");
    memset(cards, 0, sizeof(cards));
    assert(wena_card_init(&cards[0], "c2", "b1", "s1", "l1", "Active", 0, 0));
    assert(wena_card_init(&cards[1], "c1", "b1", "s1", "l1", "Old cached title", 5, 1));
    count = 2;
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    assert(wena_card_mutation_set_create_cache(&adapter, &count, 3));
    assert(wena_card_mutation_load_archived(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(!strcmp(title, "Archived") && version == 1);
    assert(!wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    strcpy(title, "Unchanged"); version = 99;
    assert(!wena_card_mutation_load_archived(&adapter, "b1", "c1", title, 2, &version));
    assert(!strcmp(title, "Unchanged") && version == 99);
    assert(!wena_card_mutation_load_archived(&adapter, "b1", "c2", title, sizeof(title), &version));
    assert(!wena_card_mutation_load_archived(&adapter, "b2", "c1", title, sizeof(title), &version));
    assert(!wena_card_mutation_load_archived(&adapter, "b1", "missing", title, sizeof(title), &version));
    assert(!wena_card_mutation_restore(&adapter, "b1", "c2", 1));
    assert(!wena_card_mutation_restore(&adapter, "b1", "missing", 1));
    assert(!wena_card_mutation_restore(&adapter, "b2", "c1", 1));
    assert(!wena_card_mutation_restore(&adapter, "b1", "c1", 0));
    assert(!wena_card_mutation_restore_request(&adapter, "b1", "c1", 1, 0));
    ++count;
    assert(!wena_card_mutation_restore(&adapter, "b1", "c1", 1));
    --count;
    assert(wena_card_mutation_init(&other, db, "u1", "b1", cards, 1));
    assert(!wena_card_mutation_restore(&other, "b1", "c1", 1));
    cards[2] = cards[1];
    assert(wena_card_mutation_init(&other, db, "u1", "b1", cards, 3));
    assert(!wena_card_mutation_restore(&other, "b1", "c1", 1));
    assert(wena_card_mutation_init(&other, db, "unknown", "b1", cards, 2));
    assert(!wena_card_mutation_load_archived(&other, "b1", "c1", title, sizeof(title), &version));
    assert(!wena_card_mutation_restore(&other, "b1", "c1", 1));
    execute(db, "UPDATE cards SET version=2 WHERE id='c1'");
    memcpy(before, cards, sizeof(cards));
    assert(!wena_card_mutation_restore(&adapter, "b1", "c1", 1));
    assert(!memcmp(before, cards, sizeof(cards)));
    assert(wena_card_mutation_load_archived(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(version == 2);
    execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'injected'); END;");
    assert(!wena_card_mutation_restore(&adapter, "b1", "c1", version));
    assert(!memcmp(before, cards, sizeof(cards)));
    assert(scalar(db, "SELECT count(*) FROM cards WHERE id='c1' AND archived=1 AND version=2 AND position=5") == 1);
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 0);
    execute(db, "DROP TRIGGER reject_metadata");
    memset(&command, 0, sizeof(command)); command.operation = WENA_DOMAIN_RESTORE_CARD;
    command.request_version = 40; strcpy(command.user_id, "u1");
    strcpy(command.route, "/b/b2/native");
    strcpy(command.form_body, "cardId=c1&expectedVersion=2");
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 0);
    assert(wena_card_mutation_restore_request(&adapter, "b1", "c1", 2, 40));
    assert(!cards[1].archived && cards[1].sort == 5 && count == 2);
    assert(!strcmp(cards[0].id, "c2") && !strcmp(cards[1].id, "c1"));
    assert(!wena_card_mutation_restore_request(&adapter, "b1", "c1", 3, 40));
    /* A stale archived cache cannot restore a row already active in SQLite. */
    cards[1].archived = 1;
    assert(!wena_card_mutation_restore(&adapter, "b1", "c1", 3));
    assert(cards[1].archived == 1);
    cards[1].archived = 0;
    memset(&command, 0, sizeof(command)); command.operation = WENA_DOMAIN_RESTORE_CARD;
    command.request_version = 41; strcpy(command.user_id, "u1");
    strcpy(command.route, "/b/b1/native");
    strcpy(command.form_body, "cardId=c1&expectedVersion=3");
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    assert(scalar(db, "SELECT count(*) FROM cards WHERE id='c1' AND archived=0 AND version=3 AND position=5") == 1);
    assert(wena_card_mutation_archive(&adapter, "b1", "c1", 3));
    assert(cards[1].archived == 1);
    assert(!wena_card_mutation_restore_request(&adapter, "b1", "c1", 4, 40));
    assert(wena_card_mutation_restore(&adapter, "b1", "c1", 4));
    assert(!cards[1].archived && cards[1].sort == 5);
    assert(scalar(db, "SELECT max(request_version) FROM idempotency_keys WHERE operation='restore-card'") == 41);
    assert(scalar(db, "SELECT version FROM cards WHERE id='c1'") == 5);
    assert(sqlite3_close(db) == SQLITE_OK); free(sql);
    puts("native archived card restoration tests passed");
    return 0;
}
