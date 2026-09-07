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
    if (version >= 3) execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position,version,created_at,updated_at) "
        "VALUES('existing','board','card','Existing checklist',0,8,1700000000123,1700000000456);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished,version) "
        "VALUES('existing-item','board','card','existing','Existing item',0,1,9)");
}
static void preserved(sqlite3 *db, int version)
{
    assert(number(db, "SELECT version FROM cards WHERE id='card'") == 11);
    assert(number(db, "SELECT version FROM cards WHERE id='card2'") == 12);
    assert(number(db, "SELECT count(*) FROM idempotency_keys WHERE response_checksum='retained'") == 1);
    if (version >= 2) assert(number(db, "SELECT count(*) FROM card_descriptions WHERE "
        "card_id='card' AND description='Exact text  '||char(10)||'line 2'") == 1);
    if (version >= 3) {
        assert(number(db, "SELECT count(*) FROM checklists WHERE id='existing' AND version=8 AND "
            "created_at=1700000000123 AND updated_at=1700000000456") == 1);
        assert(number(db, "SELECT count(*) FROM checklist_items WHERE id='existing-item' AND version=9 AND is_finished=1") == 1);
    }
}
static void label_data(sqlite3 *db)
{
    execute(db, "INSERT INTO labels(board_id,id,name,color,position,version,created_at,updated_at) "
        "VALUES('board','shared','Urgent','red',0,2,1700000000123,1700000000456);"
        "INSERT INTO labels(board_id,id,name,color,position) VALUES('board','next','','#aBcDeF',1);"
        "INSERT INTO labels(board_id,id,position) VALUES('other','shared',0);"
        "INSERT INTO labels(board_id,id,name,color,position) VALUES('other','named','Urgent','red',1);"
        );
    execute(db, "INSERT INTO card_labels VALUES('board','card','shared');"
        "INSERT INTO card_labels VALUES('board','card2','shared');"
        "INSERT INTO card_labels VALUES('board','card2','next')");
}
static void label_preserved(sqlite3 *db)
{
    assert(number(db, "SELECT count(*) FROM labels") == 4);
    assert(number(db, "SELECT count(*) FROM card_labels") == 3);
    assert(number(db, "SELECT version FROM labels WHERE board_id='board' AND id='shared'") == 2);
    assert(number(db, "SELECT count(*) FROM labels WHERE board_id='board' AND id='shared' "
        "AND created_at=1700000000123 AND updated_at=1700000000456") == 1);
    assert(number(db, "SELECT count(*) FROM labels WHERE board_id='other' AND id='shared' "
        "AND name='' AND color='' AND version=1 AND created_at=0 AND updated_at=0") == 1);
    assert(number(db, "SELECT count(*) FROM labels WHERE board_id='board' AND id='next' AND color='#aBcDeF'") == 1);
    assert(number(db, "SELECT count(*) FROM card_labels WHERE board_id='board' AND card_id='card'") == 1);
}
static void constraint_tests(sqlite3 *db)
{
    static const char *invalid[] = {
        "UPDATE labels SET board_id=NULL WHERE board_id='board'", "UPDATE labels SET board_id='' WHERE board_id='board'",
        "UPDATE labels SET board_id=zeroblob(3) WHERE board_id='board'", "UPDATE labels SET board_id='missing' WHERE board_id='board'",
        "UPDATE labels SET id=NULL WHERE id='next'", "UPDATE labels SET id='' WHERE id='next'",
        "UPDATE labels SET id=printf('%65s','x') WHERE id='next'", "UPDATE labels SET id='x'||char(0) WHERE id='next'",
        "UPDATE labels SET id=zeroblob(3) WHERE id='next'", "UPDATE labels SET name=NULL WHERE id='next'",
        "UPDATE labels SET name=zeroblob(3) WHERE id='next'",
        "UPDATE labels SET name=replace(printf('%65s',''),' ',char(196)) WHERE id='next'", "UPDATE labels SET name=printf('%129s','x') WHERE id='next'",
        "UPDATE labels SET name='x'||char(0) WHERE id='next'", "UPDATE labels SET color=NULL WHERE id='next'",
        "UPDATE labels SET color=zeroblob(7) WHERE id='next'", "UPDATE labels SET color='unknown' WHERE id='next'",
        "UPDATE labels SET color='RED' WHERE id='next'", "UPDATE labels SET color='#12345' WHERE id='next'",
        "UPDATE labels SET color='#1234567' WHERE id='next'", "UPDATE labels SET color='#12345g' WHERE id='next'",
        "UPDATE labels SET color='#00000'||char(0) WHERE id='next'", "UPDATE labels SET color='#'||char(0)||'00000' WHERE id='next'",
        "UPDATE labels SET color='#12345'||char(10) WHERE id='next'", "UPDATE labels SET color='#12345 ' WHERE id='next'",
        "UPDATE labels SET position=NULL WHERE id='next'", "UPDATE labels SET position=-1 WHERE id='next'",
        "UPDATE labels SET position=2147483648 WHERE id='next'", "UPDATE labels SET position=0.5 WHERE id='next'",
        "UPDATE labels SET position='invalid' WHERE id='next'", "UPDATE labels SET position=0 WHERE id='next'",
        "UPDATE labels SET version=NULL WHERE id='next'", "UPDATE labels SET version=0 WHERE id='next'",
        "UPDATE labels SET version=0.5 WHERE id='next'", "UPDATE labels SET version=zeroblob(3) WHERE id='next'",
        "UPDATE labels SET created_at=-1 WHERE id='next'", "UPDATE labels SET created_at=0.5 WHERE id='next'",
        "UPDATE labels SET created_at='invalid' WHERE id='next'", "UPDATE labels SET created_at=NULL WHERE id='next'",
        "UPDATE labels SET created_at=1 WHERE id='next'", "UPDATE labels SET updated_at=-1 WHERE id='next'",
        "UPDATE labels SET updated_at=0.5 WHERE id='next'", "UPDATE labels SET updated_at='invalid' WHERE id='next'",
        "UPDATE labels SET updated_at=NULL WHERE id='next'", "UPDATE labels SET name='Urgent',color='red' WHERE id='next'",
        "INSERT INTO labels(board_id,id,name,color,position) VALUES('board','shared','Duplicate','blue',2)",
        "UPDATE card_labels SET board_id=NULL WHERE card_id='card'", "UPDATE card_labels SET board_id='other' WHERE card_id='card'",
        "UPDATE card_labels SET board_id='' WHERE card_id='card'", "UPDATE card_labels SET board_id=zeroblob(5) WHERE card_id='card'",
        "UPDATE card_labels SET card_id=NULL WHERE card_id='card'", "UPDATE card_labels SET card_id='missing' WHERE card_id='card'",
        "UPDATE card_labels SET card_id='' WHERE card_id='card'", "UPDATE card_labels SET card_id=zeroblob(5) WHERE card_id='card'",
        "UPDATE card_labels SET label_id=NULL WHERE card_id='card'", "UPDATE card_labels SET label_id='named' WHERE card_id='card'",
        "UPDATE card_labels SET label_id='' WHERE card_id='card'", "UPDATE card_labels SET label_id=zeroblob(5) WHERE card_id='card'",
        "UPDATE card_labels SET label_id='shared'||char(0) WHERE card_id='card'",
        "INSERT INTO card_labels VALUES('board','card','shared')", "DELETE FROM labels WHERE board_id='board' AND id='shared'",
        "DELETE FROM cards WHERE id='card'", "DELETE FROM boards WHERE id='other'"
    };
    static const char *colors[] = {"white", "green", "yellow", "orange", "red", "purple", "blue", "sky", "lime", "pink", "black", "silver", "peachpuff", "crimson", "plum", "darkgreen", "slateblue", "magenta", "gold", "navy", "gray", "saddlebrown", "paleturquoise", "mistyrose", "indigo", "", "#abcdef", "#ABCDEF", "#012345"};
    size_t index; sqlite3_stmt *statement;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        int result; result = sqlite3_exec(db, invalid[index], NULL, NULL, NULL);
        if (result != SQLITE_CONSTRAINT) fprintf(stderr, "constraint case %lu failed: %s (%d)\n", (unsigned long)index, invalid[index], result);
        assert(result == SQLITE_CONSTRAINT);
    }
    label_preserved(db);
    execute(db, "SAVEPOINT positive_values;UPDATE labels SET name=printf('%128s','x'),position=2147483647," 
        "version=9223372036854775807,updated_at=9223372036854775807 WHERE id='next'");
    assert(sqlite3_prepare_v2(db, "UPDATE labels SET color=?1 WHERE id='next'", -1, &statement, NULL) == SQLITE_OK);
    for (index = 0; index < sizeof(colors) / sizeof(colors[0]); ++index) {
        assert(sqlite3_bind_text(statement, 1, colors[index], -1, SQLITE_STATIC) == SQLITE_OK);
        assert(sqlite3_step(statement) == SQLITE_DONE); assert(sqlite3_reset(statement) == SQLITE_OK);
    }
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    execute(db, "UPDATE labels SET name=replace(printf('%64s',''),' ',char(196)) WHERE id='next';"
        "UPDATE labels SET id=printf('%064d',1) WHERE id='named'");
    assert(number(db, "SELECT length(CAST(name AS BLOB)) FROM labels WHERE id='next'") == 128);
    assert(number(db, "SELECT count(*) FROM labels WHERE length(CAST(id AS BLOB))=64") == 1);
    execute(db, "ROLLBACK TO positive_values;RELEASE positive_values;UPDATE cards SET archived=1 WHERE id='card'");
    label_preserved(db); assert(number(db, "SELECT version FROM cards WHERE id='card'") == 11);
    assert(wena_sqlite_integrity(db));
}
static int query_steps(sqlite3 *db, const char *query, int count, const char *second)
{
    sqlite3_stmt *statement; int rows, result, steps;
    assert(sqlite3_prepare_v2(db, query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_bind_text(statement, 1, "board", -1, SQLITE_STATIC) == SQLITE_OK);
    if (second) assert(sqlite3_bind_text(statement, 2, second, -1, SQLITE_STATIC) == SQLITE_OK);
    rows = 0;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        if (sqlite3_column_count(statement) == 9) {
            assert(sqlite3_column_type(statement, 7) == SQLITE_INTEGER);
            assert(sqlite3_column_int(statement, 7) == (rows == 0 ? 2 : 1));
            assert(sqlite3_column_int(statement, 8) == sqlite3_column_int(statement, 7));
        }
        ++rows;
    }
    assert(result == SQLITE_DONE && rows == count);
    assert(sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_FULLSCAN_STEP, 0) == 0);
    assert(sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_SORT, 0) == 0);
    steps = sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_VM_STEP, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK); return steps;
}
static void query_work(sqlite3 *db)
{
    static const char labels_query[] = "SELECT id,name,color,position,version,created_at,updated_at,(SELECT count(*) FROM card_labels cl WHERE cl.board_id=labels.board_id AND cl.label_id=labels.id),(SELECT count(*) FROM card_labels cl JOIN cards c ON c.board_id=cl.board_id AND c.id=cl.card_id WHERE cl.board_id=labels.board_id AND cl.label_id=labels.id) FROM labels WHERE board_id=?1 ORDER BY position,id";
    static const char selected_query[] = "SELECT board_id,label_id FROM card_labels WHERE card_id=?2 ORDER BY label_id";
    static const char reverse_query[] = "SELECT card_id FROM card_labels WHERE board_id=?1 AND label_id=?2 ORDER BY card_id";
    int before[3], after[3], index, rows; char sql[512]; sqlite3_stmt *statement;
    before[0] = query_steps(db, labels_query, 2, NULL); before[1] = query_steps(db, selected_query, 1, "card"); before[2] = query_steps(db, reverse_query, 2, "shared");
    execute(db, "SAVEPOINT unrelated_labels;INSERT INTO swimlanes VALUES('other-lane','other','Other',0,1);"
        "INSERT INTO lists VALUES('other-list','other','Other',0,1);"
        "INSERT INTO cards VALUES('other-card','other','other-lane','other-list','Unrelated',0,0,1)");
    for (index = 0; index < 10000; ++index) {
        sprintf(sql, "INSERT INTO labels(board_id,id,name,color,position) VALUES('other','bulk%d','Bulk %d','blue',%d)", index, index, index + 2);
        execute(db, sql);
        sprintf(sql, "INSERT INTO card_labels VALUES('other','other-card','bulk%d')", index);
        execute(db, sql);
    }
    after[0] = query_steps(db, labels_query, 2, NULL); after[1] = query_steps(db, selected_query, 1, "card"); after[2] = query_steps(db, reverse_query, 2, "shared");
    for (index = 0; index < 3; ++index) assert(after[index] <= before[index] + 8 && after[index] < 250);
    printf("Label query VM steps before/after 10000 foreign labels and assignments: %d/%d %d/%d %d/%d\n", before[0], after[0], before[1], after[1], before[2], after[2]);
    execute(db, "DROP INDEX card_labels_card_order_idx");
    assert(sqlite3_prepare_v2(db, selected_query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_bind_text(statement, 2, "card", -1, SQLITE_STATIC) == SQLITE_OK);
    rows = 0; while (sqlite3_step(statement) == SQLITE_ROW) ++rows;
    assert(rows == 1 && sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_FULLSCAN_STEP, 0) >= 10000);
    assert(sqlite3_stmt_status(statement, SQLITE_STMTSTATUS_VM_STEP, 0) > 30000);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    execute(db, "ROLLBACK TO unrelated_labels;RELEASE unrelated_labels"); label_preserved(db);
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
    if (first && action == SQLITE_CREATE_TABLE && strcmp(first, "card_labels") == 0) stage = 1;
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
    Bundle bundles[5]; sqlite3 *db; Life life; WenaRestoreLifecycle lifecycle;
    char path[512], backup[512], live[512], rejected[512], side[528], before[65], after[65];
    int old, mode, stops; size_t index; FILE *file;
#if !defined(_WIN32)
    pid_t child, children[4]; int status, gate[2]; char token;
#endif
    assert(argc == 7);
    for (index = 0; index < 5; ++index) { read_bundle(argv[index + 1], &bundles[index]); assert(wena_sqlite_migration_target(bundles[index].hash) == (int)index + 1); }
    for (index = 0; index < 4; ++index)
        assert(memcmp(bundles[index].bytes, bundles[4].bytes, bundles[index].length) == 0);
    memset(&life, 0, sizeof(life)); lifecycle.stop = stop; lifecycle.start = start; lifecycle.context = &life;
    for (old = 1; old <= 4; ++old) {
        sprintf(path, "%s/upgrade-from-%d.sqlite", argv[6], old); open_database(path, &bundles[old - 1], &db); seed(db, old);
        sprintf(backup, "%s/backup-v%d.sqlite", argv[6], old); assert(wena_sqlite_backup_create(db, backup, free_space, NULL));
        assert(sqlite3_close(db) == SQLITE_OK); hash_file(backup, before);
        open_database(path, &bundles[4], &db); preserved(db, old); label_data(db); constraint_tests(db);
        assert(wena_sqlite_schema_version(db) == 5); assert(sqlite3_close(db) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[old - 1].bytes, bundles[old - 1].length, bundles[old - 1].hash, &db));
        open_database(path, &bundles[4], &db); preserved(db, old); label_preserved(db); assert(sqlite3_close(db) == SQLITE_OK);
        sprintf(live, "%s/restored-from-%d.sqlite", argv[6], old); open_database(live, &bundles[4], &life.db);
        execute(life.db, "INSERT INTO boards VALUES('sentinel','Keep live',1)"); stops = life.stops;
        failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_restore(backup, live, bundles[4].hash, free_space, NULL, &lifecycle));
        sqlite3_reset_auto_extension(); failure_mode = 0;
        assert(failure_seen && life.stops == stops); assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        life.fail_start = 1; assert(!wena_sqlite_restore(backup, live, bundles[4].hash, free_space, NULL, &lifecycle));
        assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        assert(wena_sqlite_restore(backup, live, bundles[4].hash, free_space, NULL, &lifecycle));
        assert(wena_sqlite_schema_version(life.db) == 5); preserved(life.db, old);
        assert(number(life.db, "SELECT count(*) FROM labels") == 0);
        assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL; hash_file(backup, after); assert(strcmp(before, after) == 0);
    }
    sprintf(path, "%s/fresh-v5.sqlite", argv[6]); open_database(path, &bundles[4], &db); seed(db, 5); label_data(db); query_work(db);
    assert(number(db, "SELECT count(*) FROM schema_migrations") == 5);
    sprintf(backup, "%s/backup-v5.sqlite", argv[6]); assert(wena_sqlite_backup_create(db, backup, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
    stops = life.stops; assert(!wena_sqlite_restore(backup, live, bundles[1].hash, free_space, NULL, &lifecycle)); assert(life.stops == stops);
    assert(wena_sqlite_restore(backup, live, bundles[4].hash, free_space, NULL, &lifecycle)); label_preserved(life.db); preserved(life.db, 5);
    assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL;
    for (mode = 1; mode <= 3; ++mode) {
        sprintf(path, "%s/rollback-v5-%d.sqlite", argv[6], mode); open_database(path, &bundles[3], &db); seed(db, 4); assert(sqlite3_close(db) == SQLITE_OK);
        failure_mode = mode; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[4].bytes, bundles[4].length, bundles[4].hash, &db)); assert(failure_seen);
        sqlite3_reset_auto_extension(); failure_mode = 0;
        open_database(path, &bundles[3], &db); preserved(db, 4);
        assert(number(db, "SELECT count(*) FROM sqlite_master WHERE name IN('labels','card_labels')") == 0); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[4], &db); preserved(db, 4); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/fresh-v5-failure.sqlite", argv[6]);
    failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
    assert(!wena_sqlite_open(path, bundles[4].bytes, bundles[4].length, bundles[4].hash, &db));
    sqlite3_reset_auto_extension(); failure_mode = 0; assert(failure_seen);
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(number(db, "PRAGMA user_version") == 0); assert(number(db, "SELECT count(*) FROM sqlite_master") == 0);
    assert(sqlite3_close(db) == SQLITE_OK);
#if !defined(_WIN32)
    for (mode = 1; mode <= 4; ++mode) {
        sprintf(path, "%s/killed-v5-%d.sqlite", argv[6], mode); open_database(path, &bundles[3], &db); seed(db, 4); assert(sqlite3_close(db) == SQLITE_OK);
        child = fork(); assert(child >= 0);
        if (child == 0) {
            kill_mode = mode; if (sqlite3_auto_extension((void (*)(void))extension) != SQLITE_OK) _exit(2);
            if (!wena_sqlite_open(path, bundles[4].bytes, bundles[4].length, bundles[4].hash, &db)) _exit(3);
            _exit(mode == 4 ? 84 : 4);
        }
        assert(waitpid(child, &status, 0) == child); assert(WIFEXITED(status) && WEXITSTATUS(status) == 80 + mode);
        open_database(path, &bundles[mode == 4 ? 4 : 3], &db); preserved(db, 4);
        assert(wena_sqlite_schema_version(db) == (mode == 4 ? 5 : 4)); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[4], &db); preserved(db, 4); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/concurrent-v5.sqlite", argv[6]); open_database(path, &bundles[3], &db); seed(db, 4);
    assert(sqlite3_close(db) == SQLITE_OK); assert(pipe(gate) == 0);
    for (mode = 0; mode < 4; ++mode) {
        children[mode] = fork(); assert(children[mode] >= 0);
        if (children[mode] == 0) {
            close(gate[1]); if (read(gate[0], &token, 1) != 1) _exit(2); close(gate[0]);
            if (!wena_sqlite_open(path, bundles[4].bytes, bundles[4].length, bundles[4].hash, &db)) _exit(3);
            if (wena_sqlite_schema_version(db) != 5 || sqlite3_close(db) != SQLITE_OK) _exit(4);
            _exit(0);
        }
    }
    close(gate[0]); assert(write(gate[1], "go!!", 4) == 4); close(gate[1]);
    for (mode = 0; mode < 4; ++mode) {
        assert(waitpid(children[mode], &status, 0) == children[mode]);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    open_database(path, &bundles[4], &db); preserved(db, 4);
    assert(number(db, "SELECT count(*) FROM schema_migrations") == 5); assert(sqlite3_close(db) == SQLITE_OK);
#endif
    for (index = 0; index < 21; ++index) {
        static const char *invalid[] = {
            "DELETE FROM schema_migrations WHERE version=2", "UPDATE schema_migrations SET checksum='wrong' WHERE version=5",
            "UPDATE schema_migrations SET checksum=CAST(checksum AS BLOB) WHERE version=5", "PRAGMA user_version=6", "DROP TABLE card_labels",
            "DROP TABLE card_labels;CREATE VIEW card_labels AS SELECT NULL AS board_id",
            "DROP TABLE card_labels;CREATE TABLE card_labels(board_id TEXT NOT NULL,card_id TEXT,label_id TEXT)",
            "DROP TABLE card_labels;DROP TABLE labels", "INSERT INTO schema_migrations VALUES(6,'future',0)",
            "DROP INDEX card_labels_label_cards_idx", "DROP INDEX card_labels_label_cards_idx;CREATE INDEX card_labels_label_cards_idx ON card_labels(label_id)",
            "UPDATE schema_migrations SET applied_at=-1 WHERE version=5", "UPDATE schema_migrations SET applied_at='text' WHERE version=5",
            "UPDATE schema_migrations SET applied_at=1.5 WHERE version=5", "UPDATE schema_migrations SET checksum=checksum||char(0) WHERE version=5",
            "UPDATE schema_migrations SET checksum=substr(checksum,1,63)||char(0) WHERE version=5", "DELETE FROM schema_migrations WHERE version=5",
            "DROP INDEX card_labels_card_order_idx", "DROP INDEX card_labels_card_order_idx;CREATE INDEX card_labels_card_order_idx ON card_labels(board_id)",
            "PRAGMA user_version=4", "DROP TABLE card_labels;DROP TABLE labels;CREATE TABLE labels(board_id TEXT,id TEXT,PRIMARY KEY(board_id,id))"};
        sprintf(path, "%s/bad-v5-%lu.sqlite", argv[6], (unsigned long)index); open_database(path, &bundles[4], &db); execute(db, invalid[index]);
        assert(!wena_sqlite_schema_validate(db, bundles[4].hash)); sprintf(rejected, "%s/rejected-%lu.sqlite", argv[6], (unsigned long)index);
        assert(!wena_sqlite_backup_create(db, rejected, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
        hash_file(path, before); sprintf(side, "%s.sha256", path); file = fopen(side, "wb"); assert(file); assert(fprintf(file, "%s\n", before) == 65); assert(fclose(file) == 0);
        stops = life.stops; assert(!wena_sqlite_restore(path, live, bundles[4].hash, free_space, NULL, &lifecycle)); assert(life.stops == stops);
        assert(!wena_sqlite_open(path, bundles[4].bytes, bundles[4].length, bundles[4].hash, &db));
    }
    for (index = 0; index < 5; ++index) free(bundles[index].bytes);
    puts("schema-v5 label constraints, bounded queries, upgrades, interruption and staged restore tests passed"); return 0;
}
