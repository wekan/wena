#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "sqlite_workspace.h"
#include "sqlite_storage.h"
#include "../models/model.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WORKSPACE_PATH_CAPACITY 4096

static int identifier(const char *text)
{
    size_t index;
    unsigned char c;
    if (text == NULL || text[0] == '\0') return 0;
    for (index = 0; index < WENA_ID_CAPACITY; ++index) {
        c = (unsigned char)text[index];
        if (c == 0) return 1;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 0;
}

static int title(const char *text, size_t capacity)
{
    size_t length;
    size_t index;
    unsigned long c, code, need, minimum;
    if (text == NULL) return 0;
    for (length = 0; length < capacity && text[length]; ++length) {}
    if (length == 0 || length == capacity) return 0;
    index = 0;
    while (index < length) {
        c = (unsigned char)text[index++];
        if (c < 32 || c == 127) return 0;
        if (c < 128) continue;
        if (c >= 194 && c <= 223) {
            code = c & 31; need = 1; minimum = 128;
        } else if (c >= 224 && c <= 239) {
            code = c & 15; need = 2; minimum = 2048;
        } else if (c >= 240 && c <= 244) {
            code = c & 7; need = 3; minimum = 65536;
        } else return 0;
        while (need) {
            if (index >= length) return 0;
            c = (unsigned char)text[index++];
            if ((c & 192) != 128) return 0;
            code = (code << 6) | (c & 63);
            --need;
        }
        if (code < minimum || code > 1114111 ||
            (code >= 55296 && code <= 57343) ||
            (code >= 128 && code <= 159)) return 0;
    }
    return 1;
}

static int seed_valid(const WenaSqliteWorkspaceSeed *seed)
{
    return seed != NULL && identifier(seed->actor_id) &&
        title(seed->actor_name, WENA_TITLE_CAPACITY) && identifier(seed->board_id) &&
        title(seed->board_title, WENA_SQLITE_WORKSPACE_TITLE_CAPACITY) && identifier(seed->swimlane_id) &&
        title(seed->swimlane_title, WENA_SQLITE_WORKSPACE_TITLE_CAPACITY) && identifier(seed->list_id) &&
        title(seed->list_title, WENA_SQLITE_WORKSPACE_TITLE_CAPACITY);
}

static int insert(sqlite3 *database, const char *sql,
                   const char *id, const char *title_text, const char *board)
{
    sqlite3_stmt *statement;
    int ok;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, NULL) != SQLITE_OK)
        return 0;
    ok = sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 2, title_text, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    if (ok && board != NULL)
        ok = sqlite3_bind_text(statement, 3, board, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
    if (sqlite3_finalize(statement) != SQLITE_OK) ok = 0;
    return ok;
}

static int bootstrap(sqlite3 *database, const WenaSqliteWorkspaceSeed *seed)
{
    int ok;
    if (sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK)
        return 0;
    ok = insert(database, "INSERT INTO actors(id,display_name,version) VALUES(?1,?2,1)",
                  seed->actor_id, seed->actor_name, NULL) &&
        insert(database, "INSERT INTO boards(id,title,version) VALUES(?1,?2,1)",
                 seed->board_id, seed->board_title, NULL) &&
        insert(database, "INSERT INTO swimlanes(id,title,board_id,position,version) VALUES(?1,?2,?3,0,1)",
                 seed->swimlane_id, seed->swimlane_title, seed->board_id) &&
        insert(database, "INSERT INTO lists(id,title,board_id,position,version) VALUES(?1,?2,?3,0,1)",
                 seed->list_id, seed->list_title, seed->board_id) &&
        wena_sqlite_integrity(database) &&
        sqlite3_exec(database, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) (void)sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    return ok;
}

static void cleanup(const char *directory, const char *database_path)
{
    char sidecar[WORKSPACE_PATH_CAPACITY + 64];
    (void)unlink(database_path);
    strcpy(sidecar, database_path);
    strcat(sidecar, "-wal");
    (void)unlink(sidecar);
    strcpy(sidecar, database_path);
    strcat(sidecar, "-shm");
    (void)unlink(sidecar);
    strcpy(sidecar, database_path);
    strcat(sidecar, "-journal");
    (void)unlink(sidecar);
    (void)rmdir(directory);
}

int wena_sqlite_workspace_create(const char *path,
                                  const unsigned char *migration, size_t length,
                                  const char *expected_sha256,
                                  const WenaSqliteWorkspaceSeed *seed)
{
    char directory[WORKSPACE_PATH_CAPACITY + 32];
    char staging[WORKSPACE_PATH_CAPACITY + 48];
    size_t index;
    struct stat existing;
    sqlite3 *database;
    int descriptor;
    int ok;

    if (path == NULL || path[0] != '/' || !seed_valid(seed) ||
        migration == NULL || length == 0 || expected_sha256 == NULL) return 0;
    for (index = 0; index < WORKSPACE_PATH_CAPACITY && path[index]; ++index) {
        if ((unsigned char)path[index] < 32 || (unsigned char)path[index] == 127)
            return 0;
    }
    if (index < 2 || index >= WORKSPACE_PATH_CAPACITY || path[index - 1] == '/')
        return 0;
    if (lstat(path, &existing) == 0) return 0;
    strcpy(directory, path);
    strcat(directory, ".wena-XXXXXX");
    if (mkdtemp(directory) == NULL) return 0;
    strcpy(staging, directory);
    strcat(staging, "/workspace.sqlite");
    descriptor = open(staging, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (descriptor < 0) {
        (void)rmdir(directory);
        return 0;
    }
    ok = close(descriptor) == 0;
    database = NULL;
    if (ok) ok = wena_sqlite_open(staging, migration, length, expected_sha256,
                                  &database);
    if (ok) ok = bootstrap(database, seed);
    /* Publish a standalone main file, never a database still dependent on WAL. */
    if (ok) ok = sqlite3_exec(database,
        "PRAGMA wal_checkpoint(TRUNCATE); PRAGMA journal_mode=DELETE;",
        NULL, NULL, NULL) == SQLITE_OK && wena_sqlite_integrity(database);
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) ok = 0;
    if (ok) {
        descriptor = open(staging, O_RDONLY);
        if (descriptor < 0) ok = 0;
        else {
            if (fsync(descriptor) != 0) ok = 0;
            if (close(descriptor) != 0) ok = 0;
        }
    }
    /* link() is an atomic no-replace operation, including concurrent creators. */
    if (ok) ok = link(staging, path) == 0;
    cleanup(directory, staging);
    return ok;
}
#else
int wena_sqlite_workspace_create(const char *path,
                                  const unsigned char *migration, size_t length,
                                  const char *expected_sha256,
                                  const WenaSqliteWorkspaceSeed *seed)
{
    (void)path; (void)migration; (void)length; (void)expected_sha256; (void)seed;
    return 0;
}
#endif
