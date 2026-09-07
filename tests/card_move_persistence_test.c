#include "../client/features/card_mutation.h"
#include "../server/sqlite_storage.h"
#include "../server/sqlite_board.h"
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
    WenaCard cards[2], original, unchanged[2];
    WenaSqliteBoardSnapshot *snapshot;
    char path[512], title[129];
    size_t count, index, visible;
    unsigned long version;
    assert(argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length); fclose(file);
    sprintf(path, "%s/move.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    execute(db, "INSERT INTO actors VALUES('u1','One',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO boards VALUES('b2','Other',1);"
        "INSERT INTO swimlanes VALUES('s1','b1','First',0,1);"
        "INSERT INTO swimlanes VALUES('s2','b1','Second',1,1);"
        "INSERT INTO lists VALUES('l1','b1','First',0,1);"
        "INSERT INTO lists VALUES('l2','b1','Second',1,1);");
    execute(db, "INSERT INTO swimlanes VALUES('foreign-lane','b2','Other',0,1);"
        "INSERT INTO lists VALUES('foreign-list','b2','Other',0,1);"
        "INSERT INTO cards VALUES('c1','b1','s1','l1','Original',0,0,1);"
        "INSERT INTO cards VALUES('active','b1','s2','l2','Active',4,0,1);"
        "INSERT INTO cards VALUES('archived','b1','s2','l2','Archived',9,1,1);");
    memset(cards, 0, sizeof(cards));
    assert(wena_card_init(&cards[0], "c1", "b1", "s1", "l1", "Original", 0, 0));
    original = cards[0]; count = 1;
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    assert(wena_card_mutation_set_create_cache(&adapter, &count, 2));
    assert(wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(version == 1);
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 0, "l2", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", ULONG_MAX, "l2", "s2"));
    assert(!wena_card_mutation_move_request(&adapter, "b1", "c1", 1, 0, "l2", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b2", "c1", 1, "l2", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "foreign-list", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "foreign-lane"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "missing", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "missing"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2&x=y", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b1", "missing", 1, "l2", "s2"));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 99, "l2", "s2"));
    ++count;
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "s2"));
    --count;
    cards[0].archived = 1;
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "s2"));
    cards[0].archived = 0;
    cards[1] = cards[0];
    assert(wena_card_mutation_init(&other, db, "u1", "b1", cards, 2));
    assert(!wena_card_mutation_move(&other, "b1", "c1", 1, "l2", "s2"));
    assert(wena_card_mutation_init(&other, db, "unknown", "b1", cards, 1));
    assert(!wena_card_mutation_move(&other, "b1", "c1", 1, "l2", "s2"));
    execute(db, "UPDATE cards SET archived=1 WHERE id='c1'");
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "s2"));
    execute(db, "UPDATE cards SET archived=0 WHERE id='c1'");
    execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'injected'); END;");
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "s2"));
    assert(adapter.persistence.moved_card_position == 0);
    assert(!memcmp(&cards[0], &original, sizeof(original)));
    assert(scalar(db, "SELECT version FROM cards WHERE id='c1'") == 1);
    assert(scalar(db, "SELECT count(*) FROM cards WHERE id='c1' AND list_id='l1' AND swimlane_id='s1' AND position=0") == 1);
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 0);
    execute(db, "DROP TRIGGER reject_metadata");
    /* Reject positions that cannot be represented exactly by native models. */
    execute(db, "UPDATE cards SET position=9007199254740991 WHERE id='archived'");
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 1, "l2", "s2"));
    assert(scalar(db, "SELECT version FROM cards WHERE id='c1'") == 1);
    assert(!memcmp(&cards[0], &original, sizeof(original)));
    execute(db, "UPDATE cards SET position=9 WHERE id='archived'");
    assert(wena_card_mutation_move_request(&adapter, "b1", "c1", 1, 40, "l2", "s2"));
    assert(!strcmp(cards[0].list_id, "l2") && !strcmp(cards[0].swimlane_id, "s2"));
    assert(cards[0].sort == 10 && count == 1 && !strcmp(cards[0].title, "Original"));
    assert(scalar(db, "SELECT position FROM cards WHERE id='c1'") == 10);
    assert(!wena_card_mutation_move_request(&adapter, "b1", "c1", 2, 40, "l1", "s1"));
    assert(cards[0].sort == 10 && !strcmp(cards[0].list_id, "l2"));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    assert(wena_card_mutation_load(&adapter, "b1", "c1", title, sizeof(title), &version));
    assert(version == 2 && scalar(db, "SELECT position FROM cards WHERE id='c1'") == 10);
    assert(!wena_card_mutation_move_request(&adapter, "b1", "c1", 2, 40, "l1", "s1"));
    assert(wena_card_mutation_move(&adapter, "b1", "c1", version, "l1", "s1"));
    assert(cards[0].sort == 0 && !strcmp(cards[0].list_id, "l1"));
    assert(scalar(db, "SELECT max(request_version) FROM idempotency_keys") == 41);
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 2);
    /* Moving cache index 0 to an occupied destination must render after its
     * existing card immediately, and in the same order after a DB reload. */
    assert(wena_card_init(&cards[1], "active", "b1", "s2", "l2", "Active", 4, 0));
    count = 2;
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    memcpy(unchanged, cards, sizeof(cards));
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 2, "l2", "s2"));
    assert(!memcmp(unchanged, cards, sizeof(cards)));
    assert(wena_card_mutation_move_request(&adapter, "b1", "c1", 3, 50, "l2", "s2"));
    assert(!strcmp(cards[0].id, "active") && !strcmp(cards[1].id, "c1"));
    assert(cards[1].sort == 10 && cards[0].sort == 4);
    /* Same-column moves also append, preserving the other card's order. */
    assert(wena_card_mutation_move_request(&adapter, "b1", "active", 1, 51, "l2", "s2"));
    assert(!strcmp(cards[0].id, "c1") && !strcmp(cards[1].id, "active"));
    assert(cards[0].sort == 10 && cards[1].sort == 11);
    memcpy(unchanged, cards, sizeof(cards));
    execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'injected'); END;");
    assert(!wena_card_mutation_move(&adapter, "b1", "c1", 4, "l1", "s1"));
    assert(!memcmp(unchanged, cards, sizeof(cards)));
    execute(db, "DROP TRIGGER reject_metadata");
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    snapshot = (WenaSqliteBoardSnapshot *)malloc(sizeof(*snapshot)); assert(snapshot);
    assert(wena_sqlite_board_load(db, "b1", snapshot));
    visible = 0;
    for (index = 0; index < snapshot->card_count; ++index) {
        if (!snapshot->cards[index].archived &&
            !strcmp(snapshot->cards[index].list_id, "l2") &&
            !strcmp(snapshot->cards[index].swimlane_id, "s2")) {
            assert(visible < count);
            assert(!strcmp(snapshot->cards[index].id, cards[visible].id));
            ++visible;
        }
    }
    assert(visible == count); free(snapshot);
    assert(sqlite3_close(db) == SQLITE_OK); free(sql);
    puts("native card movement persistence tests passed");
    return 0;
}
