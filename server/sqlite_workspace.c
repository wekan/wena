#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
/* macOS declares mkdtemp only with its own extensions visible. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#include "sqlite_workspace.h"
#include "sqlite_storage.h"
#include "../models/model.h"

#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32) || \
    defined(__amigaos__) || defined(__AROS__)
#if defined(_WIN32)
#include <windows.h>
#elif defined(__amigaos__) || defined(__AROS__)
#if defined(__amigaos4__)
#define __USE_INLINE__
#endif
#include <proto/dos.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WORKSPACE_PATH_CAPACITY 4096

static int seed_valid(const WenaSqliteWorkspaceSeed *seed)
{
    return seed != NULL && wena_model_identifier_valid(seed->actor_id) &&
        wena_model_title_string_valid(seed->actor_name, WENA_TITLE_CAPACITY) &&
        wena_model_identifier_valid(seed->board_id) &&
        wena_model_title_string_valid(seed->board_title,
                                      WENA_SQLITE_WORKSPACE_TITLE_CAPACITY) &&
        wena_model_identifier_valid(seed->swimlane_id) &&
        wena_model_title_string_valid(seed->swimlane_title,
                                      WENA_SQLITE_WORKSPACE_TITLE_CAPACITY) &&
        wena_model_identifier_valid(seed->list_id) &&
        wena_model_title_string_valid(seed->list_title,
                                      WENA_SQLITE_WORKSPACE_TITLE_CAPACITY);
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

#if defined(_WIN32)
/* Windows: stage beside the target and publish with MoveFileExW without
 * MOVEFILE_REPLACE_EXISTING, which fails when the target exists - the same
 * no-replace guarantee as link() below, including concurrent creators. */
static int wide_path(const char *path, wchar_t *out, int capacity)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, capacity) > 0;
}

static void remove_staging(const char *staging)
{
    static const char *const suffixes[] = {"", "-wal", "-shm", "-journal"};
    char name[WORKSPACE_PATH_CAPACITY + 64];
    wchar_t wide[WORKSPACE_PATH_CAPACITY + 64];
    size_t index;
    for (index = 0; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
        strcpy(name, staging);
        strcat(name, suffixes[index]);
        if (wide_path(name, wide, WORKSPACE_PATH_CAPACITY + 64)) (void)DeleteFileW(wide);
    }
}

int wena_sqlite_workspace_create(const char *path,
                                  const unsigned char *migration, size_t length,
                                  const char *expected_sha256,
                                  const WenaSqliteWorkspaceSeed *seed)
{
    char staging[WORKSPACE_PATH_CAPACITY + 64];
    wchar_t wide_target[WORKSPACE_PATH_CAPACITY], wide_staging[WORKSPACE_PATH_CAPACITY + 64];
    size_t index;
    HANDLE file;
    sqlite3 *database;
    int ok, drive, unc;

    if (path == NULL || !seed_valid(seed) || migration == NULL || length == 0 ||
        expected_sha256 == NULL) return 0;
    drive = ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/');
    unc = path[0] == '\\' && path[1] == '\\' && path[2] != '\0' && path[2] != '\\';
    if (!drive && !unc) return 0;
    for (index = 0; index < WORKSPACE_PATH_CAPACITY && path[index]; ++index) {
        if ((unsigned char)path[index] < 32 || (unsigned char)path[index] == 127)
            return 0;
    }
    if (index < 4 || index >= WORKSPACE_PATH_CAPACITY ||
        path[index - 1] == '\\' || path[index - 1] == '/') return 0;
    if (!wide_path(path, wide_target, WORKSPACE_PATH_CAPACITY)) return 0;
    if (GetFileAttributesW(wide_target) != INVALID_FILE_ATTRIBUTES ||
        (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)) return 0;
    sprintf(staging, "%s.wena-%lu-%lu.tmp", path, (unsigned long)GetCurrentProcessId(),
            (unsigned long)GetTickCount());
    if (!wide_path(staging, wide_staging, WORKSPACE_PATH_CAPACITY + 64)) return 0;
    file = CreateFileW(wide_staging, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    ok = CloseHandle(file) != 0;
    database = NULL;
    if (ok) ok = wena_sqlite_open(staging, migration, length, expected_sha256, &database);
    if (ok) ok = bootstrap(database, seed);
    /* Publish a standalone main file, never a database still dependent on WAL. */
    if (ok) ok = sqlite3_exec(database,
        "PRAGMA wal_checkpoint(TRUNCATE); PRAGMA journal_mode=DELETE;",
        NULL, NULL, NULL) == SQLITE_OK && wena_sqlite_integrity(database);
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) ok = 0;
    if (ok) {
        file = CreateFileW(wide_staging, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) ok = 0;
        else {
            if (!FlushFileBuffers(file)) ok = 0;
            if (!CloseHandle(file)) ok = 0;
        }
    }
    if (ok) ok = MoveFileExW(wide_staging, wide_target, MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) remove_staging(staging);
    return ok;
}
#elif defined(__amigaos__) || defined(__AROS__)
/* AmigaOS and AROS: stage beside the target and publish with dos.library's
 * Rename(), which fails when the target exists - the same no-replace
 * guarantee as link() below. AmigaDOS file systems need not have hard links,
 * and a C library's rename() may delete the target first. */
static void remove_staging(const char *staging)
{
    char sidecar[WORKSPACE_PATH_CAPACITY + 64];
    (void)remove(staging);
    strcpy(sidecar, staging);
    strcat(sidecar, "-journal");
    (void)remove(sidecar);
}

int wena_sqlite_workspace_create(const char *path,
                                  const unsigned char *migration, size_t length,
                                  const char *expected_sha256,
                                  const WenaSqliteWorkspaceSeed *seed)
{
    char staging[WORKSPACE_PATH_CAPACITY + 64];
    const char *colon;
    size_t index;
    struct stat existing;
    sqlite3 *database;
    int descriptor;
    int ok;

    if (path == NULL || !seed_valid(seed) || migration == NULL || length == 0 ||
        expected_sha256 == NULL) return 0;
    /* "Volume:name" or "ASSIGN:drawer/name", as wena_path_absolute(). */
    colon = strchr(path, ':');
    if (colon == NULL || colon == path || strchr(colon + 1, ':') != NULL ||
        memchr(path, '/', (size_t)(colon - path)) != NULL) return 0;
    for (index = 0; index < WORKSPACE_PATH_CAPACITY && path[index]; ++index) {
        if ((unsigned char)path[index] < 32 || (unsigned char)path[index] == 127)
            return 0;
    }
    if (index >= WORKSPACE_PATH_CAPACITY || path[index - 1] == '/' ||
        path[index - 1] == ':') return 0;
    if (stat(path, &existing) == 0 || errno != ENOENT) return 0;
    /* Short enough for the 30-character names of the original FFS. */
    sprintf(staging, "%s.%lx.tmp", path, (unsigned long)time(NULL) & 0xffffffUL);
    descriptor = open(staging, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (descriptor < 0) return 0;
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
    /* Close() has written the file out; AmigaDOS has no separate fsync(). */
    if (ok) ok = Rename((CONST_STRPTR)staging, (CONST_STRPTR)path) != 0;
    if (!ok) remove_staging(staging);
    return ok;
}
#else
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
#if defined(__ANDROID__)
    /* Android's SELinux policy refuses hard links in an app's data (EACCES),
     * and renameat2's no-replace is not in every Android's seccomp filter.
     * Only this app can write its private folder, and its one activity is
     * singleInstance, so nothing can create the board between this check and
     * the rename. */
    if (ok) ok = lstat(path, &existing) != 0 && rename(staging, path) == 0;
#else
    /* link() is an atomic no-replace operation, including concurrent creators. */
    if (ok) ok = link(staging, path) == 0;
#endif
    cleanup(directory, staging);
    return ok;
}
#endif
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
