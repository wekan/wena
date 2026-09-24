#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void execute(sqlite3 *db, const char *sql)
{
    assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
}

static void rejected(sqlite3 *db, WenaSqliteBoardSnapshot *out,
                     const WenaSqliteBoardSnapshot *original)
{
    assert(!wena_sqlite_board_load(db, "b1", out));
    assert(!memcmp(out, original, sizeof(*out)));
    assert(sqlite3_get_autocommit(db));
}

typedef struct SnapshotRace {
    sqlite3 *writer;
    int fired;
} SnapshotRace;

static int commit_during_read(void *context, int action, const char *table,
    const char *column, const char *database, const char *trigger)
{
    SnapshotRace *race;
    (void)column; (void)database; (void)trigger;
    race = (SnapshotRace *)context;
    if (!race->fired && action == SQLITE_READ && table && !strcmp(table, "lists")) {
        race->fired = 1;
        execute(race->writer, "BEGIN IMMEDIATE;"
            "UPDATE boards SET title='Concurrent board' WHERE id='b1';"
            "UPDATE cards SET title='Concurrent card' WHERE id='c1';COMMIT;");
    }
    return SQLITE_OK;
}

int main(int argc, char **argv)
{
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE *file;
    unsigned char *sql;
    long length;
    sqlite3 *db, *writer;
    SnapshotRace race;
    WenaSqliteBoardSnapshot *out, *original;
    char path[512];
    const char *invalid[] = {
        "UPDATE cards SET title='' WHERE id='c1'",
        "UPDATE cards SET title=char(10) WHERE id='c1'",
        "UPDATE cards SET title=CAST(x'C080' AS TEXT) WHERE id='c1'",
        "UPDATE cards SET title=CAST(x'EDA080' AS TEXT) WHERE id='c1'",
        "UPDATE cards SET title=CAST(x'610062' AS TEXT) WHERE id='c1'",
        "UPDATE cards SET title=zeroblob(10) WHERE id='c1'",
        "UPDATE cards SET title=printf('%0257d',1) WHERE id='c1'",
        "UPDATE cards SET position='invalid' WHERE id='c1'",
        "UPDATE cards SET position=0.5 WHERE id='c1'",
        "UPDATE cards SET position=9007199254740992 WHERE id='c1'",
        "UPDATE cards SET version='invalid' WHERE id='c1'",
        "UPDATE cards SET list_id='foreign-list' WHERE id='c1'",
        "UPDATE cards SET swimlane_id='foreign-lane' WHERE id='c1'"
    };
    size_t i;
    assert(argc == 3);
    out = (WenaSqliteBoardSnapshot *)malloc(sizeof(*out)); assert(out);
    original = (WenaSqliteBoardSnapshot *)malloc(sizeof(*original)); assert(original);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length); fclose(file);
    sprintf(path, "%s/board.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    execute(db, "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO boards VALUES('b2','Other',1);"
        "INSERT INTO swimlanes VALUES('s2','b1','Second',1,1);"
        "INSERT INTO swimlanes VALUES('s1','b1','First',0,1);"
        "INSERT INTO swimlanes VALUES('foreign-lane','b2','Other',0,1);"
        "INSERT INTO lists VALUES('l2','b1','Second',1,1);"
        "INSERT INTO lists VALUES('l1','b1','First',0,1);"
        "INSERT INTO lists VALUES('foreign-list','b2','Other',0,1);"
        "");
    execute(db, "INSERT INTO cards VALUES('c2','b1','s1','l1','Archived',1,1,2);"
        "INSERT INTO cards VALUES('c1','b1','s1','l1','First card',0,0,1);"
        "INSERT INTO cards VALUES('foreign-card','b2','foreign-lane','foreign-list','Other',0,0,1);");
    memset(out, 0xa5, sizeof(*out));
    assert(wena_sqlite_board_load(db, "b1", out));
    for (i = 0; i < out->list_count; ++i)
        assert(out->lists[i].wip_limit.value == 1 && !out->lists[i].wip_limit.enabled && !out->lists[i].wip_limit.soft);
    assert(!strcmp(out->board.id, "b1") && !strcmp(out->board.title, "Board"));
    assert(out->swimlane_count == 2 && out->list_count == 2 && out->card_count == 2);
    assert(!strcmp(out->swimlanes[0].id, "s1") && !strcmp(out->lists[0].id, "l1"));
    assert(!out->lists[0].swimlane_id[0]);
    assert(!strcmp(out->cards[0].id, "c1") && !out->cards[0].archived);
    assert(!strcmp(out->cards[1].id, "c2") && out->cards[1].archived);
    memcpy(original, out, sizeof(*out));
    assert(!wena_sqlite_board_load(db, "missing", out));
    assert(!wena_sqlite_board_load(db, "b1' OR 1=1", out));
    assert(!wena_sqlite_board_load(NULL, "b1", out));
    assert(!memcmp(out, original, sizeof(*out)));
    execute(db, "BEGIN");
    assert(!wena_sqlite_board_load(db, "b1", out));
    assert(!sqlite3_get_autocommit(db));
    execute(db, "ROLLBACK");
    execute(db, "PRAGMA foreign_keys=OFF");
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        execute(db, invalid[i]);
        rejected(db, out, original);
        execute(db, "DELETE FROM cards WHERE id='c1';"
            "INSERT INTO cards VALUES('c1','b1','s1','l1','First card',0,0,1)");
    }
    execute(db, "PRAGMA ignore_check_constraints=ON");
    execute(db, "UPDATE cards SET archived=4294967296 WHERE id='c1'");
    rejected(db, out, original);
    execute(db, "UPDATE cards SET archived=0,version=0 WHERE id='c1'");
    rejected(db, out, original);
    execute(db, "UPDATE cards SET version=1,position=-1 WHERE id='c1'");
    rejected(db, out, original);
    execute(db, "UPDATE cards SET position=0 WHERE id='c1'");
    execute(db, "PRAGMA ignore_check_constraints=OFF");
    execute(db, "UPDATE boards SET title=char(127) WHERE id='b1'");
    rejected(db, out, original);
    execute(db, "UPDATE boards SET title='Board' WHERE id='b1'");
    execute(db, "UPDATE lists SET title='' WHERE id='l1'");
    rejected(db, out, original);
    execute(db, "UPDATE lists SET title='First' WHERE id='l1'");
    execute(db, "UPDATE swimlanes SET position=1.5 WHERE id='s1'");
    rejected(db, out, original);
    execute(db, "UPDATE swimlanes SET position=0 WHERE id='s1'");
    execute(db, "WITH RECURSIVE n(x) AS (SELECT 2 UNION ALL SELECT x+1 FROM n WHERE x<64) "
        "INSERT INTO swimlanes SELECT 'extra-'||x,'b1','Extra',x,1 FROM n");
    rejected(db, out, original);
    execute(db, "DELETE FROM swimlanes WHERE id LIKE 'extra-%'");
    execute(db, "WITH RECURSIVE n(x) AS (SELECT 2 UNION ALL SELECT x+1 FROM n WHERE x<128) "
        "INSERT INTO lists SELECT 'extra-'||x,'b1','Extra',x,1 FROM n");
    rejected(db, out, original);
    execute(db, "DELETE FROM lists WHERE id LIKE 'extra-%'");
    execute(db, "WITH RECURSIVE n(x) AS (SELECT 2 UNION ALL SELECT x+1 FROM n WHERE x<2048) "
        "INSERT INTO cards SELECT 'extra-'||x,'b1','s1','l1','Extra',x,0,1 FROM n");
    rejected(db, out, original);
    execute(db, "DELETE FROM cards WHERE id LIKE 'extra-%'");
    execute(db, "PRAGMA foreign_keys=ON");
    assert(wena_sqlite_board_load(db, "b1", out));
    assert(!memcmp(out, original, sizeof(*out)));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_sqlite_board_load(db, "b1", out));
    assert(!memcmp(out, original, sizeof(*out)));
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &writer));
    race.writer = writer; race.fired = 0;
    assert(sqlite3_set_authorizer(db, commit_during_read, &race) == SQLITE_OK);
    assert(wena_sqlite_board_load(db, "b1", out));
    assert(race.fired && !memcmp(out, original, sizeof(*out)));
    assert(sqlite3_set_authorizer(db, NULL, NULL) == SQLITE_OK);
    assert(wena_sqlite_board_load(db, "b1", out));
    assert(!strcmp(out->board.title, "Concurrent board"));
    assert(!strcmp(out->cards[0].title, "Concurrent card"));
    assert(sqlite3_close(writer) == SQLITE_OK);
    execute(db, "INSERT INTO boards VALUES('empty','Empty',1)");
    assert(wena_sqlite_board_load(db, "empty", out));
    assert(!out->swimlane_count && !out->list_count && !out->card_count);
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    assert(wena_sqlite_board_load(db, "b1", out));
    assert(!strcmp(out->cards[0].title, "Concurrent card"));
    assert(sqlite3_close(db) == SQLITE_OK);
    free(sql); free(original); free(out);
    puts("SQLite board snapshot tests passed");
    return 0;
}
