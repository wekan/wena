#include "../server/sqlite_storage.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_backup.h"
#include "../server/sqlite_restore.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Lifecycle {
    sqlite3 *database;
    unsigned char *migration;
    size_t length;
    char hash[65];
    int starts;
} Lifecycle;

static unsigned char *read_all(const char *path, size_t *length)
{
    FILE *file;
    long size;
    unsigned char *bytes;
    file = fopen(path, "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); size = ftell(file); assert(size > 0);
    rewind(file); bytes = (unsigned char *)malloc((size_t)size); assert(bytes);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    fclose(file); *length = (size_t)size; return bytes;
}
static int integer(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *statement;
    int value;
    assert(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    value = sqlite3_column_int(statement, 0); sqlite3_finalize(statement); return value;
}
static void flags(sqlite3 *db, int defensive, int trusted)
{
    int actual;
    assert(sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, -1, &actual) == SQLITE_OK);
    assert(actual == defensive);
    assert(sqlite3_db_config(db, SQLITE_DBCONFIG_TRUSTED_SCHEMA, -1, &actual) == SQLITE_OK);
    assert(actual == trusted);
}
static void side_effect(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    int *calls;
    (void)argc; (void)argv;
    calls = (int *)sqlite3_user_data(context); ++*calls;
    sqlite3_result_int(context, 1);
}
static int space(void *context, const char *path, unsigned long *bytes)
{
    (void)context; (void)path; *bytes = 100000000UL; return 1;
}
static int stop(void *context)
{
    Lifecycle *life;
    life = (Lifecycle *)context;
    if (sqlite3_close(life->database) != SQLITE_OK) return 0;
    life->database = NULL; return 1;
}
static int start(void *context, const char *path)
{
    Lifecycle *life;
    life = (Lifecycle *)context;
    if (!wena_sqlite_open(path, life->migration, life->length,
                          life->hash, &life->database)) return 0;
    flags(life->database, 1, 0); ++life->starts; return 1;
}
int main(int argc, char **argv)
{
    Lifecycle life;
    WenaRestoreLifecycle hooks;
    WenaSqliteBoardSnapshot *snapshot;
    sqlite3 *readonly;
    char path[1024], backup[1024];
    int calls;
    assert(argc == 3);
    memset(&life, 0, sizeof(life));
    life.migration = read_all(argv[1], &life.length);
    wena_sha256_hex(life.migration, life.length, life.hash);
    sprintf(path, "%s/hardened.sqlite", argv[2]);
    sprintf(backup, "%s/backup.sqlite", argv[2]);
    assert(!wena_sqlite_connection_harden(NULL));
    assert(start(&life, path));
    assert(wena_sqlite_connection_harden(life.database)); flags(life.database, 1, 0);
    assert(sqlite3_exec(life.database,
        "INSERT INTO actors(id,display_name,version) VALUES('u','User',1);"
        "INSERT INTO boards(id,title,version) VALUES('b','Before',1);",
        NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(life.database, "PRAGMA writable_schema=ON", NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(life.database,
        "UPDATE sqlite_schema SET sql='invalid SQL' WHERE name='boards'",
        NULL, NULL, NULL) != SQLITE_OK);
    assert(integer(life.database, "SELECT count(*) FROM boards WHERE title='Before'") == 1);
    calls = 0;
    assert(sqlite3_create_function(life.database, "side_effect", 0, SQLITE_UTF8,
        &calls, side_effect, NULL, NULL) == SQLITE_OK);
    assert(integer(life.database, "SELECT side_effect()") == 1 && calls == 1);
    calls = 0;
    assert(sqlite3_exec(life.database,
        "CREATE VIEW hostile_view AS SELECT side_effect();"
        "CREATE TRIGGER hostile_trigger BEFORE UPDATE ON boards BEGIN SELECT side_effect(); END;",
        NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(life.database, "SELECT * FROM hostile_view", NULL, NULL, NULL) != SQLITE_OK);
    assert(sqlite3_exec(life.database, "UPDATE boards SET title='Bad'", NULL, NULL, NULL) != SQLITE_OK);
    assert(calls == 0);
    assert(integer(life.database, "SELECT count(*) FROM boards WHERE title='Before'") == 1);
    assert(sqlite3_exec(life.database,
        "DROP VIEW hostile_view;DROP TRIGGER hostile_trigger;"
        "CREATE TRIGGER rollback_test BEFORE UPDATE ON boards BEGIN SELECT RAISE(ABORT,'injected'); END;",
        NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(life.database, "UPDATE boards SET title='Rejected'", NULL, NULL, NULL) != SQLITE_OK);
    assert(integer(life.database, "SELECT count(*) FROM boards WHERE title='Before'") == 1);
    assert(sqlite3_exec(life.database,
        "DROP TRIGGER rollback_test;UPDATE boards SET title='Committed',version=version+1;",
        NULL, NULL, NULL) == SQLITE_OK);
    /* Explicitly caller-owned read connections retain their own configuration
       when passed to snapshot load and backup source validation. */
    assert(sqlite3_db_config(life.database, SQLITE_DBCONFIG_DEFENSIVE, 0, NULL) == SQLITE_OK);
    assert(sqlite3_db_config(life.database, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 1, NULL) == SQLITE_OK);
    snapshot = (WenaSqliteBoardSnapshot *)malloc(sizeof(*snapshot)); assert(snapshot);
    assert(wena_sqlite_board_load(life.database, "b", snapshot));
    assert(strcmp(snapshot->board.title, "Committed") == 0); free(snapshot);
    flags(life.database, 0, 1);
    assert(wena_sqlite_backup_create(life.database, backup, space, NULL));
    flags(life.database, 0, 1);
    assert(wena_sqlite_connection_harden(life.database));
    assert(sqlite3_open_v2(backup, &readonly, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    assert(wena_sqlite_connection_harden(readonly)); flags(readonly, 1, 0);
    assert(wena_sqlite_integrity(readonly)); assert(sqlite3_close(readonly) == SQLITE_OK);
    assert(sqlite3_exec(life.database, "UPDATE boards SET title='After backup'", NULL, NULL, NULL) == SQLITE_OK);
    hooks.stop = stop; hooks.start = start; hooks.context = &life;
    assert(wena_sqlite_restore(backup, path, life.hash, space, NULL, &hooks));
    assert(life.starts == 2);
    assert(integer(life.database, "SELECT count(*) FROM boards WHERE title='Committed'") == 1);
    flags(life.database, 1, 0);
    assert(stop(&life)); free(life.migration);
    puts("SQLite hardened connection, schema isolation, backup and restore passed");
    return 0;
}
