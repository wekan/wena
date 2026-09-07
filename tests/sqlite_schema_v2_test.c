#include "../server/sqlite_storage.h"
#include "../server/sqlite_backup.h"
#include "../server/sqlite_restore.h"
#include "../server/sha256.h"
#include "../server/embedded_migration.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static unsigned char *read_all(const char *path, size_t *length)
{
    FILE *file;
    long size;
    unsigned char *bytes;
    file = fopen(path, "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); size = ftell(file); assert(size > 0);
    rewind(file); bytes = (unsigned char *)malloc((size_t)size); assert(bytes);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    assert(fclose(file) == 0); *length = (size_t)size; return bytes;
}
static void execute(sqlite3 *db, const char *sql)
{ assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK); }
static int integer(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *s; int value;
    assert(sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW); value = sqlite3_column_int(s, 0);
    assert(sqlite3_finalize(s) == SQLITE_OK); return value;
}
static void seed(sqlite3 *db)
{
    execute(db, "INSERT INTO actors VALUES('actor','Actor',1);"
        "INSERT INTO boards VALUES('board','Original board',7);"
        "INSERT INTO boards VALUES('other','Other board',1);"
        "INSERT INTO swimlanes VALUES('lane','board','Lane',0,3);"
        "INSERT INTO lists VALUES('list','board','List',0,4);"
        "INSERT INTO cards VALUES('card','board','lane','list','Title',0,0,11);"
        "INSERT INTO idempotency_keys VALUES('actor','/b/board/cards','edit-card-title',17,'retained',1)");
}
static void preserved(sqlite3 *db)
{
    assert(integer(db, "SELECT version FROM cards WHERE id='card'") == 11);
    assert(integer(db, "SELECT version FROM boards WHERE id='board'") == 7);
    assert(integer(db, "SELECT count(*) FROM idempotency_keys WHERE response_checksum='retained'") == 1);
    assert(integer(db, "SELECT count(*) FROM cards WHERE list_id='list' AND swimlane_id='lane' AND position=0") == 1);
}
static int put(sqlite3 *db, const char *card, const char *board,
                const char *text, int length)
{
    sqlite3_stmt *s; int ok;
    assert(sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO card_descriptions"
        "(card_id,board_id,description) VALUES(?1,?2,?3)", -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_bind_text(s, 1, card, -1, SQLITE_STATIC) == SQLITE_OK);
    assert(sqlite3_bind_text(s, 2, board, -1, SQLITE_STATIC) == SQLITE_OK);
    assert(sqlite3_bind_text(s, 3, text, length, SQLITE_STATIC) == SQLITE_OK);
    ok = sqlite3_step(s) == SQLITE_DONE; sqlite3_finalize(s); return ok;
}
static void description(sqlite3 *db, const char *text, size_t length)
{
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db, "SELECT description FROM card_descriptions WHERE card_id='card'",
        -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW);
    assert(sqlite3_column_type(s, 0) == SQLITE_TEXT);
    assert((size_t)sqlite3_column_bytes(s, 0) == length);
    assert(memcmp(sqlite3_column_text(s, 0), text, length) == 0);
    assert(sqlite3_finalize(s) == SQLITE_OK);
}
static int fail_mode, failure_seen;
#if !defined(_WIN32)
static int kill_mode;
#endif
static int authorize(void *context, int action, const char *first,
    const char *second, const char *database, const char *trigger)
{
    (void)context; (void)second; (void)database; (void)trigger;
#if !defined(_WIN32)
    if (first && ((kill_mode == 1 && action == SQLITE_CREATE_TABLE &&
        strcmp(first, "card_descriptions") == 0) ||
        (kill_mode == 2 && action == SQLITE_INSERT && strcmp(first, "schema_migrations") == 0) ||
        (kill_mode == 3 && action == SQLITE_TRANSACTION && strcmp(first, "COMMIT") == 0)))
        _exit(80 + kill_mode);
#endif
    if (first && ((fail_mode == 1 && action == SQLITE_CREATE_TABLE &&
        strcmp(first, "card_descriptions") == 0) ||
        (fail_mode == 2 && action == SQLITE_INSERT && strcmp(first, "schema_migrations") == 0) ||
        (fail_mode == 3 && action == SQLITE_TRANSACTION && strcmp(first, "COMMIT") == 0))) {
        failure_seen = 1; return SQLITE_DENY;
    }
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
static void file_hash(const char *path, char hash[65])
{ unsigned char *bytes; size_t length; bytes = read_all(path, &length); wena_sha256_hex(bytes, length, hash); free(bytes); }

#if !defined(_WIN32)
static void recovery_tests(const char *directory, const unsigned char *v1,
    size_t n1, const char *h1, const unsigned char *bundle, size_t n2, const char *h2)
{
    char path[512], token;
    sqlite3 *db;
    pid_t child, children[4];
    int mode, status, gate[2], index;
    for (mode = 1; mode <= 4; ++mode) {
        sprintf(path, "%s/killed-upgrade-%d.sqlite", directory, mode);
        assert(wena_sqlite_open(path, v1, n1, h1, &db)); seed(db);
        assert(sqlite3_close(db) == SQLITE_OK);
        child = fork(); assert(child >= 0);
        if (child == 0) {
            kill_mode = mode;
            if (sqlite3_auto_extension((void (*)(void))extension) != SQLITE_OK) _exit(2);
            if (!wena_sqlite_open(path, bundle, n2, h2, &db)) _exit(3);
            /* No sqlite3_close: death after a successful commit must retain v2. */
            _exit(mode == 4 ? 84 : 4);
        }
        assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 80 + mode);
        assert(wena_sqlite_open(path, mode == 4 ? bundle : v1,
            mode == 4 ? n2 : n1, mode == 4 ? h2 : h1, &db));
        preserved(db);
        assert(wena_sqlite_schema_version(db) == (mode == 4 ? 2 : 1));
        if (mode != 4) assert(integer(db, "SELECT count(*) FROM sqlite_master "
            "WHERE name IN('card_descriptions','cards_board_id_unique')") == 0);
        assert(sqlite3_close(db) == SQLITE_OK);
        assert(wena_sqlite_open(path, bundle, n2, h2, &db)); preserved(db);
        assert(wena_sqlite_schema_version(db) == 2); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/concurrent-upgrade.sqlite", directory);
    assert(wena_sqlite_open(path, v1, n1, h1, &db)); seed(db);
    assert(sqlite3_close(db) == SQLITE_OK); assert(pipe(gate) == 0);
    for (index = 0; index < 4; ++index) {
        children[index] = fork(); assert(children[index] >= 0);
        if (children[index] == 0) {
            close(gate[1]);
            if (read(gate[0], &token, 1) != 1) _exit(2);
            close(gate[0]);
            if (!wena_sqlite_open(path, bundle, n2, h2, &db)) _exit(3);
            if (wena_sqlite_schema_version(db) != 2 || sqlite3_close(db) != SQLITE_OK) _exit(4);
            _exit(0);
        }
    }
    close(gate[0]); assert(write(gate[1], "go!!", 4) == 4); close(gate[1]);
    for (index = 0; index < 4; ++index) {
        assert(waitpid(children[index], &status, 0) == children[index]);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    assert(wena_sqlite_open(path, bundle, n2, h2, &db)); preserved(db);
    assert(integer(db, "SELECT count(*) FROM schema_migrations") == 2);
    assert(sqlite3_close(db) == SQLITE_OK);
}
#endif

int main(int argc, char **argv)
{
    unsigned char *v1, *bundle, *bad;
    size_t n1, n2, index;
    char h1[65], h2[65], hash[65], before[65], after[65];
    char path[512], fresh[512], backup[512], restored[512], badpath[512], side[528];
    char maximum[1026];
    const char text[] = "Finnish \303\244\303\266 and \346\227\245\346\234\254\350\252\236  \n\tMarkdown\r\n";
    const char nul[] = {'a', '\0', 'b'};
    sqlite3 *db; FILE *file; int mode;
    Life life; WenaRestoreLifecycle lifecycle; WenaEmbeddedMigration embedded;
    assert(argc == 4);
    v1 = read_all(argv[1], &n1); bundle = read_all(argv[2], &n2);
    assert(n2 > n1 && memcmp(v1, bundle, n1) == 0);
    wena_sha256_hex(v1, n1, h1); wena_sha256_hex(bundle, n2, h2);
    assert(wena_sqlite_migration_target(h1) == 1);
    assert(wena_sqlite_migration_target(h2) == 2);
    assert(!wena_sqlite_migration_target("unknown"));
    sprintf(path, "%s/populated.sqlite", argv[3]);
    sprintf(fresh, "%s/fresh.sqlite", argv[3]);
    sprintf(backup, "%s/backup.sqlite", argv[3]);
    sprintf(restored, "%s/restored.sqlite", argv[3]);
    sprintf(badpath, "%s/untrusted.sqlite", argv[3]);
    assert(wena_sqlite_open(path, v1, n1, h1, &db)); seed(db);
    assert(wena_sqlite_backup_create(db, backup, free_space, NULL));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, bundle, n2, h2, &db)); preserved(db);
    assert(wena_sqlite_schema_version(db) == 2);
    assert(!wena_sqlite_schema_validate(db, h1));
    assert(wena_sqlite_schema_validate(db, h2));
    assert(integer(db, "SELECT count(*) FROM card_descriptions") == 0);
    assert(integer(db, "SELECT count(*) FROM schema_migrations") == 2);
    assert(put(db, "card", "board", text, sizeof(text) - 1)); description(db, text, sizeof(text) - 1);
    assert(!put(db, NULL, "missing-board", "orphan", 6));
    assert(!put(db, "card", NULL, "orphan", 6));
    assert(!put(db, "card", "other", "wrong scope", 11));
    assert(!put(db, "missing", "board", "missing", 7));
    description(db, text, sizeof(text) - 1);
    assert(!put(db, "card", "board", nul, sizeof(nul)));
    memset(maximum, 'x', sizeof(maximum)); maximum[1025] = 0;
    assert(!put(db, "card", "board", maximum, 1025));
    assert(put(db, "card", "board", maximum, 1024)); description(db, maximum, 1024u);
    assert(put(db, "card", "board", "", 0)); description(db, "", 0);
    assert(put(db, "card", "board", text, sizeof(text) - 1));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(!wena_sqlite_open(path, v1, n1, h1, &db)); assert(db == NULL);
    assert(wena_sqlite_open(path, bundle, n2, h2, &db)); preserved(db); description(db, text, sizeof(text) - 1);
    assert(integer(db, "SELECT count(*) FROM schema_migrations") == 2);
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(fresh, bundle, n2, h2, &db));
    assert(wena_sqlite_schema_version(db) == 2); assert(sqlite3_close(db) == SQLITE_OK);
    bad = (unsigned char *)malloc(n2 + 4u); assert(bad); memcpy(bad, bundle, n2);
    bad[n1] ^= 1; wena_sha256_hex(bad, n2, hash);
    assert(!wena_sqlite_open(badpath, bad, n2, h2, &db));
    assert(!wena_sqlite_open(badpath, bad, n2, hash, &db));
    memcpy(bad, bundle + n1, n2 - n1); memcpy(bad + n2 - n1, bundle, n1);
    wena_sha256_hex(bad, n2, hash); assert(!wena_sqlite_open(badpath, bad, n2, hash, &db));
    memcpy(bad, bundle, n2); memcpy(bad + n2, "evil", 4u);
    wena_sha256_hex(bad, n2 + 4u, hash); assert(!wena_sqlite_open(badpath, bad, n2 + 4u, hash, &db));
    wena_sha256_hex(bundle + n1, n2 - n1, hash);
    assert(!wena_sqlite_open(badpath, bundle + n1, n2 - n1, hash, &db));
    file = fopen(badpath, "rb"); assert(file == NULL); free(bad);
    sprintf(fresh, "%s/good-artifact", argv[3]);
    assert(wena_embedded_migration_load(fresh, &embedded));
    assert(embedded.length == n2 && memcmp(embedded.bytes, bundle, n2) == 0);
    assert(wena_sqlite_open(badpath, embedded.bytes, embedded.length, embedded.sha256, &db));
    assert(wena_sqlite_schema_version(db) == 2); assert(sqlite3_close(db) == SQLITE_OK);
    wena_embedded_migration_free(&embedded);
    sprintf(fresh, "%s/self-hashed-untrusted-artifact", argv[3]);
    assert(wena_embedded_migration_load(fresh, &embedded));
    /* Transport hashes are not authority: even a self-consistent footer cannot
     * cause SQL outside the compiled registry to run. */
    assert(!wena_sqlite_open(badpath, embedded.bytes, embedded.length, embedded.sha256, &db));
    wena_embedded_migration_free(&embedded);
    assert(wena_sqlite_open(badpath, bundle, n2, h2, &db));
    assert(wena_sqlite_schema_version(db) == 2); assert(sqlite3_close(db) == SQLITE_OK);
    for (mode = 1; mode <= 3; ++mode) {
        sprintf(badpath, "%s/upgrade-failure-%d.sqlite", argv[3], mode);
        assert(wena_sqlite_open(badpath, v1, n1, h1, &db)); seed(db);
        assert(sqlite3_close(db) == SQLITE_OK);
        fail_mode = mode; failure_seen = 0;
        assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_open(badpath, bundle, n2, h2, &db)); assert(failure_seen);
        sqlite3_reset_auto_extension(); fail_mode = 0;
        assert(wena_sqlite_open(badpath, v1, n1, h1, &db)); preserved(db);
        assert(integer(db, "SELECT count(*) FROM sqlite_master WHERE name IN"
            "('card_descriptions','cards_board_id_unique')") == 0);
        assert(sqlite3_close(db) == SQLITE_OK);
        assert(wena_sqlite_open(badpath, bundle, n2, h2, &db)); preserved(db);
        assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(badpath, "%s/fresh-failure.sqlite", argv[3]);
    fail_mode = 1; failure_seen = 0;
    assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
    assert(!wena_sqlite_open(badpath, bundle, n2, h2, &db)); assert(failure_seen);
    sqlite3_reset_auto_extension(); fail_mode = 0;
    assert(sqlite3_open(badpath, &db) == SQLITE_OK);
    assert(integer(db, "PRAGMA user_version") == 0);
    assert(integer(db, "SELECT count(*) FROM sqlite_master") == 0);
    assert(sqlite3_close(db) == SQLITE_OK);
    memset(&life, 0, sizeof(life)); lifecycle.stop = stop; lifecycle.start = start; lifecycle.context = &life;
    for (index = 0; index < 11u; ++index) {
        static const char *invalid[] = {"DELETE FROM schema_migrations WHERE version=1",
            "UPDATE schema_migrations SET checksum='wrong' WHERE version=2",
            "UPDATE schema_migrations SET checksum=CAST(checksum AS BLOB) WHERE version=2",
            "INSERT INTO schema_migrations VALUES(3,'future',0)", "PRAGMA user_version=3",
            "DROP TABLE card_descriptions", "DROP INDEX cards_board_id_unique",
            "DROP TABLE card_descriptions;CREATE VIEW card_descriptions AS SELECT NULL AS card_id,NULL AS board_id,NULL AS description",
            "DROP TABLE card_descriptions;CREATE TABLE card_descriptions(card_id TEXT NOT NULL PRIMARY KEY,board_id TEXT NOT NULL)",
            "DROP TABLE card_descriptions;CREATE TABLE card_descriptions(card_id TEXT NOT NULL PRIMARY KEY,board_id TEXT NOT NULL,description TEXT)",
            "DROP INDEX cards_board_id_unique;CREATE INDEX cards_board_id_unique ON cards(board_id,id)"};
        sprintf(badpath, "%s/history-v2-%lu.sqlite", argv[3], (unsigned long)index);
        assert(wena_sqlite_open(badpath, bundle, n2, h2, &db)); execute(db, invalid[index]);
        assert(!wena_sqlite_schema_validate(db, h2));
        sprintf(fresh, "%s/rejected-backup-%lu.sqlite", argv[3], (unsigned long)index);
        assert(!wena_sqlite_backup_create(db, fresh, free_space, NULL));
        assert(sqlite3_close(db) == SQLITE_OK);
        file_hash(badpath, hash); sprintf(side, "%s.sha256", badpath);
        file = fopen(side, "wb"); assert(file); assert(fprintf(file, "%s\n", hash) == 65); assert(fclose(file) == 0);
        assert(!wena_sqlite_restore(badpath, path, h2, free_space, NULL, &lifecycle));
        assert(life.stops == 0 && life.starts == 0);
        assert(!wena_sqlite_open(badpath, bundle, n2, h2, &db));
    }
    /* Stage a v1 backup upgrade before touching the live v2 database. */
    memset(&life, 0, sizeof(life)); lifecycle.stop = stop; lifecycle.start = start; lifecycle.context = &life;
    assert(wena_sqlite_open(restored, bundle, n2, h2, &life.db));
    execute(life.db, "INSERT INTO boards VALUES('sentinel','Keep live',1)");
    file_hash(backup, before);
    fail_mode = 1; failure_seen = 0;
    assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
    assert(!wena_sqlite_restore(backup, restored, h2, free_space, NULL, &lifecycle));
    sqlite3_reset_auto_extension(); fail_mode = 0;
    assert(failure_seen && life.stops == 0 && life.starts == 0);
    assert(integer(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
    file_hash(backup, after); assert(strcmp(before, after) == 0);
    life.fail_start = 1;
    assert(!wena_sqlite_restore(backup, restored, h2, free_space, NULL, &lifecycle));
    assert(integer(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
    assert(wena_sqlite_schema_version(life.db) == 2);
    assert(wena_sqlite_restore(backup, restored, h2, free_space, NULL, &lifecycle));
    assert(wena_sqlite_schema_version(life.db) == 2); preserved(life.db);
    assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL;
    file_hash(backup, after); assert(strcmp(before, after) == 0);
    /* A current v2 backup retains descriptions and cannot be restored as v1. */
    sprintf(backup, "%s/backup-v2.sqlite", argv[3]);
    assert(wena_sqlite_open(path, bundle, n2, h2, &db));
    assert(wena_sqlite_backup_create(db, backup, free_space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
    mode = life.stops;
    assert(!wena_sqlite_restore(backup, restored, h1, free_space, NULL, &lifecycle));
    assert(life.stops == mode);
    assert(wena_sqlite_restore(backup, restored, h2, free_space, NULL, &lifecycle));
    description(life.db, text, sizeof(text) - 1); preserved(life.db);
    assert(sqlite3_close(life.db) == SQLITE_OK);
#if !defined(_WIN32)
    recovery_tests(argv[3], v1, n1, h1, bundle, n2, h2);
#endif
    free(v1); free(bundle); puts("schema-v2 description migration, artifact and staged restore tests passed");
    return 0;
}
