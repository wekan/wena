#include "../server/sqlite_storage.h"
#include "../server/sqlite_backup.h"
#include "../server/sqlite_restore.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

typedef struct Bundle { unsigned char *bytes; size_t length; char hash[65]; } Bundle;
static void read_bundle(const char *path, Bundle *bundle)
{
    FILE *file; long length;
    file = fopen(path, "rb"); assert(file); assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file); assert(length > 0); rewind(file);
    bundle->length = (size_t)length; bundle->bytes = (unsigned char *)malloc(bundle->length); assert(bundle->bytes);
    assert(fread(bundle->bytes, 1, bundle->length, file) == bundle->length); assert(fclose(file) == 0);
    wena_sha256_hex(bundle->bytes, bundle->length, bundle->hash);
}
static void execute(sqlite3 *db, const char *sql)
{ assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK); }
static int number(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *s; int result;
    assert(sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW); result = sqlite3_column_int(s, 0);
    assert(sqlite3_finalize(s) == SQLITE_OK); return result;
}
static void open_database(const char *path, const Bundle *bundle, sqlite3 **db)
{ assert(wena_sqlite_open(path, bundle->bytes, bundle->length, bundle->hash, db)); }
static void seed(sqlite3 *db, int version)
{
    execute(db, "INSERT INTO actors VALUES('actor','Actor',1);"
        "INSERT INTO boards VALUES('board','Board',7);INSERT INTO boards VALUES('other','Other',1);"
        "INSERT INTO swimlanes VALUES('lane','board','Lane',0,3);"
        "INSERT INTO lists VALUES('list','board','List',0,4);"
        "INSERT INTO cards VALUES('card','board','lane','list','Title',0,0,11);"
        "INSERT INTO cards VALUES('card2','board','lane','list','Title 2',1,0,12);"
        "INSERT INTO idempotency_keys VALUES('actor','/b/board/cards','edit-card-title',17,'retained',1)");
    if (version >= 2) execute(db, "INSERT INTO card_descriptions VALUES('card','board','Exact text  '||char(10)||'line 2')");
}
static void preserved(sqlite3 *db, int description)
{
    assert(number(db, "SELECT version FROM cards WHERE id='card'") == 11);
    assert(number(db, "SELECT version FROM cards WHERE id='card2'") == 12);
    assert(number(db, "SELECT count(*) FROM idempotency_keys WHERE response_checksum='retained'") == 1);
    if (description) assert(number(db, "SELECT count(*) FROM card_descriptions WHERE "
        "card_id='card' AND description='Exact text  '||char(10)||'line 2'") == 1);
}
static void checklist_data(sqlite3 *db)
{
    execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position,version,created_at,updated_at) "
        "VALUES('check','board','card','Checklist',0,2,1700000000123,1700000000456);"
        "INSERT INTO checklists(id,board_id,card_id,title,position,show_on_minicard) "
        "VALUES('next','board','card','Next',1,0);"
        "INSERT INTO checklists(id,board_id,card_id,title,position,show_on_minicard) "
        "VALUES('another','board','card2','Another',0,1)");
    execute(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished,version,created_at,updated_at) "
        "VALUES('item','board','card','check','Item',0,0,4,1700000000123,1700000000456);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) "
        "VALUES('done','board','card','check','Done',1,1)");
}
static void checklist_preserved(sqlite3 *db)
{
    assert(number(db, "SELECT count(*) FROM checklists") == 3);
    assert(number(db, "SELECT count(*) FROM checklist_items") == 2);
    assert(number(db, "SELECT version FROM checklists WHERE id='check'") == 2);
    assert(number(db, "SELECT version FROM checklist_items WHERE id='item'") == 4);
    assert(number(db, "SELECT count(*) FROM checklists WHERE id='check' AND created_at=1700000000123 AND updated_at=1700000000456") == 1);
    assert(number(db, "SELECT count(*) FROM checklist_items WHERE id='item' AND created_at=1700000000123 AND updated_at=1700000000456") == 1);
    assert(number(db, "SELECT count(*) FROM checklists WHERE show_on_minicard IS NULL") == 1);
    assert(number(db, "SELECT count(*) FROM checklists WHERE show_on_minicard=0") == 1);
    assert(number(db, "SELECT count(*) FROM checklists WHERE show_on_minicard=1") == 1);
    assert(number(db, "SELECT sum(is_finished) FROM checklist_items") == 1);
    assert(number(db, "SELECT count(*) FROM pragma_table_info('checklists') WHERE name IN('total_count','finished_count','progress')") == 0);
}
static void constraint_tests(sqlite3 *db)
{
    static const char *invalid[] = {
        "UPDATE checklists SET id=NULL WHERE id='check'", "UPDATE checklists SET id='' WHERE id='check'",
        "UPDATE checklists SET board_id='other' WHERE id='check'", "UPDATE checklists SET card_id='missing' WHERE id='check'",
        "UPDATE checklists SET title='' WHERE id='check'", "UPDATE checklists SET title=zeroblob(128) WHERE id='check'",
        "UPDATE checklists SET title=printf('%129s','x') WHERE id='check'", "UPDATE checklists SET title='x'||char(0) WHERE id='check'",
        "UPDATE checklists SET position=-1 WHERE id='check'", "UPDATE checklists SET position=2147483648 WHERE id='check'",
        "UPDATE checklists SET position=0.5 WHERE id='check'", "UPDATE checklists SET position='invalid' WHERE id='check'",
        "UPDATE checklists SET hide_checked_items=2 WHERE id='check'", "UPDATE checklists SET hide_all_items=NULL WHERE id='check'",
        "UPDATE checklists SET show_on_minicard=2 WHERE id='check'", "UPDATE checklists SET show_on_minicard=0.5 WHERE id='check'",
        "UPDATE checklists SET version=0 WHERE id='check'", "UPDATE checklists SET version=0.5 WHERE id='check'",
        "UPDATE checklists SET created_at=-1 WHERE id='check'", "UPDATE checklists SET updated_at=1.5 WHERE id='check'",
        "UPDATE checklists SET created_at='invalid' WHERE id='check'", "UPDATE checklists SET updated_at=NULL WHERE id='check'",
        "UPDATE checklists SET position=1 WHERE id='check'",
        "UPDATE checklist_items SET id=NULL WHERE id='item'", "UPDATE checklist_items SET board_id='other' WHERE id='item'",
        "UPDATE checklist_items SET card_id='card2' WHERE id='item'", "UPDATE checklist_items SET checklist_id='another' WHERE id='item'",
        "UPDATE checklist_items SET title='' WHERE id='item'", "UPDATE checklist_items SET title=printf('%129s','x') WHERE id='item'",
        "UPDATE checklist_items SET title='x'||char(0) WHERE id='item'", "UPDATE checklist_items SET title=zeroblob(3) WHERE id='item'",
        "UPDATE checklist_items SET position=-1 WHERE id='item'", "UPDATE checklist_items SET position=2147483648 WHERE id='item'",
        "UPDATE checklist_items SET position=0.5 WHERE id='item'", "UPDATE checklist_items SET is_finished=2 WHERE id='item'",
        "UPDATE checklist_items SET is_finished=NULL WHERE id='item'", "UPDATE checklist_items SET version=-1 WHERE id='item'",
        "UPDATE checklist_items SET version='invalid' WHERE id='item'", "UPDATE checklist_items SET created_at=-1 WHERE id='item'",
        "UPDATE checklist_items SET updated_at=1.5 WHERE id='item'", "UPDATE checklist_items SET position=1 WHERE id='item'",
        "DELETE FROM checklists WHERE id='check'", "DELETE FROM cards WHERE id='card2'"
    };
    size_t index;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
        assert(sqlite3_exec(db, invalid[index], NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    checklist_preserved(db);
    execute(db, "UPDATE checklists SET title=printf('%128s','x') WHERE id='check';"
        "UPDATE checklist_items SET title=printf('%128s','x') WHERE id='item';"
        "UPDATE cards SET archived=1 WHERE id='card'");
    checklist_preserved(db);
    assert(number(db, "SELECT version FROM cards WHERE id='card'") == 11);
    assert(wena_sqlite_integrity(db));
}
static int failure_mode, failure_seen;
#if !defined(_WIN32)
static int kill_mode;
#endif
static int authorize(void *context, int action, const char *first,
    const char *second, const char *database, const char *trigger)
{
    int stage;
    (void)context; (void)second; (void)database; (void)trigger;
    stage = 0;
    if (first && action == SQLITE_CREATE_TABLE && strcmp(first, "checklist_items") == 0) stage = 1;
    if (first && action == SQLITE_INSERT && strcmp(first, "schema_migrations") == 0) stage = 2;
    if (first && action == SQLITE_TRANSACTION && strcmp(first, "COMMIT") == 0) stage = 3;
#if !defined(_WIN32)
    if (kill_mode && kill_mode == stage) _exit(80 + stage);
#endif
    if (stage && failure_mode == stage) { failure_seen = 1; return SQLITE_DENY; }
    return SQLITE_OK;
}
static int extension(sqlite3 *db, char **error, const sqlite3_api_routines *api)
{ (void)error; (void)api; return sqlite3_set_authorizer(db, authorize, NULL); }
static int free_space(void *context, const char *path, unsigned long *bytes)
{ (void)context; (void)path; *bytes = (unsigned long)-1; return 1; }
typedef struct Life { sqlite3 *db; int stops, starts, fail_start; } Life;
static int stop(void *context)
{
    Life *life; life = (Life *)context; ++life->stops;
    if (life->db) { assert(sqlite3_close(life->db) == SQLITE_OK); life->db = NULL; }
    return 1;
}
static int start(void *context, const char *path)
{
    Life *life; life = (Life *)context; ++life->starts;
    if (life->fail_start) { life->fail_start = 0; return 0; }
    return sqlite3_open_v2(path, &life->db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
}
static void hash_file(const char *path, char hash[65])
{ Bundle data; read_bundle(path, &data); strcpy(hash, data.hash); free(data.bytes); }

int main(int argc, char **argv)
{
    Bundle bundles[3]; sqlite3 *db; Life life; WenaRestoreLifecycle lifecycle;
    char path[512], backup[512], live[512], rejected[512], side[528], before[65], after[65];
    int old, mode, stops; size_t index; FILE *file;
#if !defined(_WIN32)
    pid_t child, children[4]; int status, gate[2]; char token;
#endif
    assert(argc == 5);
    for (index = 0; index < 3; ++index) { read_bundle(argv[index + 1], &bundles[index]); assert(wena_sqlite_migration_target(bundles[index].hash) == (int)index + 1); }
    assert(memcmp(bundles[0].bytes, bundles[2].bytes, bundles[0].length) == 0);
    assert(memcmp(bundles[1].bytes, bundles[2].bytes, bundles[1].length) == 0);
    memset(&life, 0, sizeof(life)); lifecycle.stop = stop; lifecycle.start = start; lifecycle.context = &life;
    for (old = 1; old <= 2; ++old) {
        sprintf(path, "%s/upgrade-from-%d.sqlite", argv[4], old); open_database(path, &bundles[old - 1], &db); seed(db, old);
        sprintf(backup, "%s/backup-v%d.sqlite", argv[4], old); assert(wena_sqlite_backup_create(db, backup, free_space, NULL));
        assert(sqlite3_close(db) == SQLITE_OK); hash_file(backup, before);
        open_database(path, &bundles[2], &db); preserved(db, old >= 2); checklist_data(db); constraint_tests(db);
        assert(wena_sqlite_schema_version(db) == 3); assert(sqlite3_close(db) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[old - 1].bytes, bundles[old - 1].length, bundles[old - 1].hash, &db));
        open_database(path, &bundles[2], &db); preserved(db, old >= 2); checklist_preserved(db); assert(sqlite3_close(db) == SQLITE_OK);
        sprintf(live, "%s/restored-from-%d.sqlite", argv[4], old); open_database(live, &bundles[2], &life.db);
        execute(life.db, "INSERT INTO boards VALUES('sentinel','Keep live',1)"); stops = life.stops;
        failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_restore(backup, live, bundles[2].hash, free_space, NULL, &lifecycle));
        sqlite3_reset_auto_extension(); failure_mode = 0;
        assert(failure_seen && life.stops == stops); assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        life.fail_start = 1; assert(!wena_sqlite_restore(backup, live, bundles[2].hash, free_space, NULL, &lifecycle));
        assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        assert(wena_sqlite_restore(backup, live, bundles[2].hash, free_space, NULL, &lifecycle));
        assert(wena_sqlite_schema_version(life.db) == 3); preserved(life.db, old >= 2);
        assert(number(life.db, "SELECT count(*) FROM checklists") == 0);
        assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL; hash_file(backup, after); assert(strcmp(before, after) == 0);
    }
    sprintf(path, "%s/fresh-v3.sqlite", argv[4]); open_database(path, &bundles[2], &db); seed(db, 3); checklist_data(db);
    assert(number(db, "SELECT count(*) FROM schema_migrations") == 3);
    sprintf(backup, "%s/backup-v3.sqlite", argv[4]); assert(wena_sqlite_backup_create(db, backup, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
    stops = life.stops; assert(!wena_sqlite_restore(backup, live, bundles[1].hash, free_space, NULL, &lifecycle)); assert(life.stops == stops);
    assert(wena_sqlite_restore(backup, live, bundles[2].hash, free_space, NULL, &lifecycle)); checklist_preserved(life.db);
    assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL;
    for (mode = 1; mode <= 3; ++mode) {
        sprintf(path, "%s/rollback-v3-%d.sqlite", argv[4], mode); open_database(path, &bundles[1], &db); seed(db, 2); assert(sqlite3_close(db) == SQLITE_OK);
        failure_mode = mode; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[2].bytes, bundles[2].length, bundles[2].hash, &db)); assert(failure_seen);
        sqlite3_reset_auto_extension(); failure_mode = 0;
        open_database(path, &bundles[1], &db); preserved(db, 1);
        assert(number(db, "SELECT count(*) FROM sqlite_master WHERE name IN('checklists','checklist_items')") == 0); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[2], &db); preserved(db, 1); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/fresh-v3-failure.sqlite", argv[4]);
    failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
    assert(!wena_sqlite_open(path, bundles[2].bytes, bundles[2].length, bundles[2].hash, &db));
    sqlite3_reset_auto_extension(); failure_mode = 0; assert(failure_seen);
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(number(db, "PRAGMA user_version") == 0); assert(number(db, "SELECT count(*) FROM sqlite_master") == 0);
    assert(sqlite3_close(db) == SQLITE_OK);
#if !defined(_WIN32)
    for (mode = 1; mode <= 4; ++mode) {
        sprintf(path, "%s/killed-v3-%d.sqlite", argv[4], mode); open_database(path, &bundles[1], &db); seed(db, 2); assert(sqlite3_close(db) == SQLITE_OK);
        child = fork(); assert(child >= 0);
        if (child == 0) {
            kill_mode = mode; if (sqlite3_auto_extension((void (*)(void))extension) != SQLITE_OK) _exit(2);
            if (!wena_sqlite_open(path, bundles[2].bytes, bundles[2].length, bundles[2].hash, &db)) _exit(3);
            _exit(mode == 4 ? 84 : 4);
        }
        assert(waitpid(child, &status, 0) == child); assert(WIFEXITED(status) && WEXITSTATUS(status) == 80 + mode);
        open_database(path, &bundles[mode == 4 ? 2 : 1], &db); preserved(db, 1);
        assert(wena_sqlite_schema_version(db) == (mode == 4 ? 3 : 2)); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[2], &db); preserved(db, 1); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/concurrent-v3.sqlite", argv[4]); open_database(path, &bundles[1], &db); seed(db, 2);
    assert(sqlite3_close(db) == SQLITE_OK); assert(pipe(gate) == 0);
    for (mode = 0; mode < 4; ++mode) {
        children[mode] = fork(); assert(children[mode] >= 0);
        if (children[mode] == 0) {
            close(gate[1]); if (read(gate[0], &token, 1) != 1) _exit(2); close(gate[0]);
            if (!wena_sqlite_open(path, bundles[2].bytes, bundles[2].length, bundles[2].hash, &db)) _exit(3);
            if (wena_sqlite_schema_version(db) != 3 || sqlite3_close(db) != SQLITE_OK) _exit(4);
            _exit(0);
        }
    }
    close(gate[0]); assert(write(gate[1], "go!!", 4) == 4); close(gate[1]);
    for (mode = 0; mode < 4; ++mode) {
        assert(waitpid(children[mode], &status, 0) == children[mode]);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    open_database(path, &bundles[2], &db); preserved(db, 1);
    assert(number(db, "SELECT count(*) FROM schema_migrations") == 3); assert(sqlite3_close(db) == SQLITE_OK);
#endif
    for (index = 0; index < 9; ++index) {
        static const char *invalid[] = {"DELETE FROM schema_migrations WHERE version=2", "UPDATE schema_migrations SET checksum='wrong' WHERE version=3",
            "UPDATE schema_migrations SET checksum=CAST(checksum AS BLOB) WHERE version=3", "PRAGMA user_version=4", "DROP TABLE checklist_items",
            "DROP TABLE checklist_items;CREATE VIEW checklist_items AS SELECT NULL AS id",
            "DROP TABLE checklist_items;CREATE TABLE checklist_items(id TEXT NOT NULL PRIMARY KEY)",
            "DROP TABLE checklist_items;DROP TABLE checklists", "INSERT INTO schema_migrations VALUES(4,'future',0)"};
        sprintf(path, "%s/bad-v3-%lu.sqlite", argv[4], (unsigned long)index); open_database(path, &bundles[2], &db); execute(db, invalid[index]);
        assert(!wena_sqlite_schema_validate(db, bundles[2].hash)); sprintf(rejected, "%s/rejected-%lu.sqlite", argv[4], (unsigned long)index);
        assert(!wena_sqlite_backup_create(db, rejected, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
        hash_file(path, before); sprintf(side, "%s.sha256", path); file = fopen(side, "wb"); assert(file); assert(fprintf(file, "%s\n", before) == 65); assert(fclose(file) == 0);
        stops = life.stops; assert(!wena_sqlite_restore(path, live, bundles[2].hash, free_space, NULL, &lifecycle)); assert(life.stops == stops);
        assert(!wena_sqlite_open(path, bundles[2].bytes, bundles[2].length, bundles[2].hash, &db));
    }
    for (index = 0; index < 3; ++index) free(bundles[index].bytes);
    puts("schema-v3 checklist constraints, upgrades, interruption and staged restore tests passed"); return 0;
}
