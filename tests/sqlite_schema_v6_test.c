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
    if (version >= 3) execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position,version) "
        "VALUES('existing','board','card','Existing',0,8);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished,version) "
        "VALUES('existing-item','board','card','existing','Existing',0,1,9)");
    if (version >= 5) execute(db, "INSERT INTO labels(board_id,id,name,color,position,version) "
        "VALUES('board','existing-label','Existing','#aBcDeF',0,3);"
        "INSERT INTO card_labels VALUES('board','card','existing-label')");
}
static void preserved(sqlite3 *db, int version)
{
    assert(number(db, "SELECT version FROM cards WHERE id='card'") == 11);
    assert(number(db, "SELECT version FROM cards WHERE id='card2'") == 12);
    assert(number(db, "SELECT count(*) FROM idempotency_keys WHERE response_checksum='retained'") == 1);
    if (version >= 2) assert(number(db, "SELECT count(*) FROM card_descriptions WHERE "
        "card_id='card' AND description='Exact text  '||char(10)||'line 2'") == 1);
    if (version >= 3) {
        assert(number(db, "SELECT count(*) FROM checklists WHERE id='existing' AND version=8") == 1);
        assert(number(db, "SELECT count(*) FROM checklist_items WHERE id='existing-item' AND version=9 AND is_finished=1") == 1);
    }
    if (version >= 5) {
        assert(number(db, "SELECT count(*) FROM labels WHERE board_id='board' AND id='existing-label' AND color='#aBcDeF' AND version=3") == 1);
        assert(number(db, "SELECT count(*) FROM card_labels WHERE board_id='board' AND card_id='card' AND label_id='existing-label'") == 1);
    }
}
static void settings_data(sqlite3 *db)
{
    assert(number(db, "SELECT count(*) FROM board_settings") == 0);
    execute(db, "INSERT INTO board_settings VALUES('board',1);INSERT INTO board_settings(board_id) VALUES('other')");
}
static void settings_preserved(sqlite3 *db)
{
    assert(number(db, "SELECT count(*) FROM board_settings") == 2);
    assert(number(db, "SELECT show_checklist_count FROM board_settings WHERE board_id='board'") == 1);
    assert(number(db, "SELECT count(*) FROM board_settings WHERE board_id='other' AND typeof(show_checklist_count)='integer' AND show_checklist_count=0") == 1);
    assert(number(db, "SELECT version FROM boards WHERE id='board'") == 7);
}
static void constraint_tests(sqlite3 *db)
{
    static const char *invalid[] = {
        "UPDATE board_settings SET board_id=NULL WHERE board_id='board'", "UPDATE board_settings SET board_id='' WHERE board_id='board'",
        "UPDATE board_settings SET board_id=zeroblob(5) WHERE board_id='board'", "UPDATE board_settings SET board_id=printf('%65s','x') WHERE board_id='board'",
        "UPDATE board_settings SET board_id='board'||char(0) WHERE board_id='board'", "UPDATE board_settings SET board_id='missing' WHERE board_id='board'",
        "UPDATE board_settings SET show_checklist_count=NULL WHERE board_id='board'", "UPDATE board_settings SET show_checklist_count=-1 WHERE board_id='board'",
        "UPDATE board_settings SET show_checklist_count=2 WHERE board_id='board'", "UPDATE board_settings SET show_checklist_count=0.5 WHERE board_id='board'",
        "UPDATE board_settings SET show_checklist_count='invalid' WHERE board_id='board'", "UPDATE board_settings SET show_checklist_count=zeroblob(1) WHERE board_id='board'",
        "INSERT INTO board_settings VALUES('board',0)", "DELETE FROM boards WHERE id='other'"
    };
    size_t index;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
        assert(sqlite3_exec(db, invalid[index], NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    settings_preserved(db);
    execute(db, "SAVEPOINT positive_values;INSERT INTO boards VALUES(printf('%064d',1),'Maximum ID',1);"
        "INSERT INTO board_settings VALUES(printf('%064d',1),1);"
        "UPDATE board_settings SET show_checklist_count=0 WHERE board_id='board';"
        "UPDATE board_settings SET show_checklist_count=1 WHERE board_id='other'");
    assert(number(db, "SELECT count(*) FROM board_settings WHERE length(CAST(board_id AS BLOB))=64") == 1);
    assert(number(db, "SELECT show_checklist_count FROM board_settings WHERE board_id='board'") == 0);
    assert(number(db, "SELECT show_checklist_count FROM board_settings WHERE board_id='other'") == 1);
    execute(db, "ROLLBACK TO positive_values;RELEASE positive_values"); settings_preserved(db);
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
    if (first && action == SQLITE_CREATE_TABLE && strcmp(first, "board_settings") == 0) stage = 1;
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
    Bundle bundles[6]; sqlite3 *db; Life life; WenaRestoreLifecycle lifecycle;
    char path[512], backup[512], live[512], rejected[512], side[528], before[65], after[65];
    int old, mode, stops; size_t index; FILE *file;
#if !defined(_WIN32)
    pid_t child, children[4]; int status, gate[2]; char token;
#endif
    assert(argc == 8);
    for (index = 0; index < 6; ++index) { read_bundle(argv[index + 1], &bundles[index]); assert(wena_sqlite_migration_target(bundles[index].hash) == (int)index + 1); }
    for (index = 0; index < 5; ++index)
        assert(memcmp(bundles[index].bytes, bundles[5].bytes, bundles[index].length) == 0);
    memset(&life, 0, sizeof(life)); lifecycle.stop = stop; lifecycle.start = start; lifecycle.context = &life;
    for (old = 1; old <= 5; ++old) {
        sprintf(path, "%s/upgrade-from-%d.sqlite", argv[7], old); open_database(path, &bundles[old - 1], &db); seed(db, old);
        sprintf(backup, "%s/backup-v%d.sqlite", argv[7], old); assert(wena_sqlite_backup_create(db, backup, free_space, NULL));
        assert(sqlite3_close(db) == SQLITE_OK); hash_file(backup, before);
        open_database(path, &bundles[5], &db); preserved(db, old); settings_data(db); constraint_tests(db);
        assert(wena_sqlite_schema_version(db) == 6); assert(sqlite3_close(db) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[old - 1].bytes, bundles[old - 1].length, bundles[old - 1].hash, &db));
        open_database(path, &bundles[5], &db); preserved(db, old); settings_preserved(db); assert(sqlite3_close(db) == SQLITE_OK);
        sprintf(live, "%s/restored-from-%d.sqlite", argv[7], old); open_database(live, &bundles[5], &life.db);
        execute(life.db, "INSERT INTO boards VALUES('sentinel','Keep live',1)"); stops = life.stops;
        failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_restore(backup, live, bundles[5].hash, free_space, NULL, &lifecycle));
        sqlite3_reset_auto_extension(); failure_mode = 0;
        assert(failure_seen && life.stops == stops); assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        life.fail_start = 1; assert(!wena_sqlite_restore(backup, live, bundles[5].hash, free_space, NULL, &lifecycle));
        assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        assert(wena_sqlite_restore(backup, live, bundles[5].hash, free_space, NULL, &lifecycle));
        assert(wena_sqlite_schema_version(life.db) == 6); preserved(life.db, old);
        assert(number(life.db, "SELECT count(*) FROM board_settings") == 0);
        assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL; hash_file(backup, after); assert(strcmp(before, after) == 0);
    }
    sprintf(path, "%s/fresh-v6.sqlite", argv[7]); open_database(path, &bundles[5], &db); seed(db, 6); settings_data(db);
    assert(number(db, "SELECT count(*) FROM schema_migrations") == 6);
    sprintf(backup, "%s/backup-v6.sqlite", argv[7]); assert(wena_sqlite_backup_create(db, backup, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
    stops = life.stops; assert(!wena_sqlite_restore(backup, live, bundles[1].hash, free_space, NULL, &lifecycle)); assert(life.stops == stops);
    assert(wena_sqlite_restore(backup, live, bundles[5].hash, free_space, NULL, &lifecycle)); settings_preserved(life.db); preserved(life.db, 6);
    assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL;
    for (mode = 1; mode <= 3; ++mode) {
        sprintf(path, "%s/rollback-v6-%d.sqlite", argv[7], mode); open_database(path, &bundles[4], &db); seed(db, 5); assert(sqlite3_close(db) == SQLITE_OK);
        failure_mode = mode; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[5].bytes, bundles[5].length, bundles[5].hash, &db)); assert(failure_seen);
        sqlite3_reset_auto_extension(); failure_mode = 0;
        open_database(path, &bundles[4], &db); preserved(db, 5);
        assert(number(db, "SELECT count(*) FROM sqlite_master WHERE name='board_settings'") == 0); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[5], &db); preserved(db, 5); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/fresh-v6-failure.sqlite", argv[7]);
    failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
    assert(!wena_sqlite_open(path, bundles[5].bytes, bundles[5].length, bundles[5].hash, &db));
    sqlite3_reset_auto_extension(); failure_mode = 0; assert(failure_seen);
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(number(db, "PRAGMA user_version") == 0); assert(number(db, "SELECT count(*) FROM sqlite_master") == 0);
    assert(sqlite3_close(db) == SQLITE_OK);
#if !defined(_WIN32)
    for (mode = 1; mode <= 4; ++mode) {
        sprintf(path, "%s/killed-v6-%d.sqlite", argv[7], mode); open_database(path, &bundles[4], &db); seed(db, 5); assert(sqlite3_close(db) == SQLITE_OK);
        child = fork(); assert(child >= 0);
        if (child == 0) {
            kill_mode = mode; if (sqlite3_auto_extension((void (*)(void))extension) != SQLITE_OK) _exit(2);
            if (!wena_sqlite_open(path, bundles[5].bytes, bundles[5].length, bundles[5].hash, &db)) _exit(3);
            _exit(mode == 4 ? 84 : 4);
        }
        assert(waitpid(child, &status, 0) == child); assert(WIFEXITED(status) && WEXITSTATUS(status) == 80 + mode);
        open_database(path, &bundles[mode == 4 ? 5 : 4], &db); preserved(db, 5);
        assert(wena_sqlite_schema_version(db) == (mode == 4 ? 6 : 5)); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[5], &db); preserved(db, 5); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/concurrent-v6.sqlite", argv[7]); open_database(path, &bundles[4], &db); seed(db, 5);
    assert(sqlite3_close(db) == SQLITE_OK); assert(pipe(gate) == 0);
    for (mode = 0; mode < 4; ++mode) {
        children[mode] = fork(); assert(children[mode] >= 0);
        if (children[mode] == 0) {
            close(gate[1]); if (read(gate[0], &token, 1) != 1) _exit(2); close(gate[0]);
            if (!wena_sqlite_open(path, bundles[5].bytes, bundles[5].length, bundles[5].hash, &db)) _exit(3);
            if (wena_sqlite_schema_version(db) != 6 || sqlite3_close(db) != SQLITE_OK) _exit(4);
            _exit(0);
        }
    }
    close(gate[0]); assert(write(gate[1], "go!!", 4) == 4); close(gate[1]);
    for (mode = 0; mode < 4; ++mode) {
        assert(waitpid(children[mode], &status, 0) == children[mode]);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    open_database(path, &bundles[5], &db); preserved(db, 5);
    assert(number(db, "SELECT count(*) FROM schema_migrations") == 6); assert(sqlite3_close(db) == SQLITE_OK);
#endif
    for (index = 0; index < 17; ++index) {
        static const char *invalid[] = {
            "DELETE FROM schema_migrations WHERE version=5", "UPDATE schema_migrations SET checksum='wrong' WHERE version=6",
            "UPDATE schema_migrations SET checksum=CAST(checksum AS BLOB) WHERE version=6", "PRAGMA user_version=7", "DROP TABLE board_settings",
            "DROP TABLE board_settings;CREATE VIEW board_settings AS SELECT NULL AS board_id",
            "DROP TABLE board_settings;CREATE TABLE board_settings(board_id TEXT,show_checklist_count INTEGER)",
            "INSERT INTO schema_migrations VALUES(7,'future',0)", "UPDATE schema_migrations SET applied_at=-1 WHERE version=6",
            "UPDATE schema_migrations SET applied_at='text' WHERE version=6", "UPDATE schema_migrations SET applied_at=1.5 WHERE version=6",
            "UPDATE schema_migrations SET checksum=checksum||char(0) WHERE version=6",
            "UPDATE schema_migrations SET checksum=substr(checksum,1,63)||char(0) WHERE version=6", "DELETE FROM schema_migrations WHERE version=6",
            "PRAGMA user_version=5", "DROP TABLE board_settings;CREATE TABLE board_settings(board_id TEXT NOT NULL PRIMARY KEY,show_checklist_count TEXT)",
            "DROP TABLE board_settings;CREATE TABLE board_settings(board_id TEXT NOT NULL PRIMARY KEY,show_checklist_count INTEGER DEFAULT 1)"};
        sprintf(path, "%s/bad-v6-%lu.sqlite", argv[7], (unsigned long)index); open_database(path, &bundles[5], &db); execute(db, invalid[index]);
        assert(!wena_sqlite_schema_validate(db, bundles[5].hash)); sprintf(rejected, "%s/rejected-%lu.sqlite", argv[7], (unsigned long)index);
        assert(!wena_sqlite_backup_create(db, rejected, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
        hash_file(path, before); sprintf(side, "%s.sha256", path); file = fopen(side, "wb"); assert(file); assert(fprintf(file, "%s\n", before) == 65); assert(fclose(file) == 0);
        stops = life.stops; assert(!wena_sqlite_restore(path, live, bundles[5].hash, free_space, NULL, &lifecycle)); assert(life.stops == stops);
        assert(!wena_sqlite_open(path, bundles[5].bytes, bundles[5].length, bundles[5].hash, &db));
    }
    for (index = 0; index < 6; ++index) free(bundles[index].bytes);
    puts("schema-v6 opt-in board settings, upgrades, interruption and staged restore tests passed"); return 0;
}
