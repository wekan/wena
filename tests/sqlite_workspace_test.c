#define _POSIX_C_SOURCE 200809L
#include "../server/sqlite_workspace.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int rejected_insert;
static int rollback_seen;

static int deny_last_seed(void *context, int action, const char *first,
                           const char *second, const char *database,
                           const char *trigger)
{
    (void)context; (void)second; (void)database; (void)trigger;
    if (action == SQLITE_INSERT && first != NULL && strcmp(first, "lists") == 0) {
        rejected_insert = 1;
        return SQLITE_DENY;
    }
    if (action == SQLITE_TRANSACTION && first != NULL &&
        strcmp(first, "ROLLBACK") == 0) rollback_seen = 1;
    return SQLITE_OK;
}

/* SQLite's supported connection extension mechanism injects a late write error,
   without production test hooks or relying on disk-full/resource timing. */
static int failing_extension(sqlite3 *database, char **error,
                              const sqlite3_api_routines *api)
{
    (void)error; (void)api;
    return sqlite3_set_authorizer(database, deny_last_seed, NULL);
}

static void no_staging(const char *directory)
{
    DIR *handle;
    struct dirent *entry;
    handle = opendir(directory);
    assert(handle != NULL);
    while ((entry = readdir(handle)) != NULL) {
        assert(strstr(entry->d_name, ".wena-") == NULL);
    }
    assert(closedir(handle) == 0);
}

static void scalar(sqlite3 *database, const char *sql, const char *expected)
{
    sqlite3_stmt *statement;
    assert(sqlite3_prepare_v2(database, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    assert(sqlite3_column_text(statement, 0) != NULL);
    assert(strcmp((const char *)sqlite3_column_text(statement, 0), expected) == 0);
    assert(sqlite3_step(statement) == SQLITE_DONE);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
}

int main(int argc, char **argv)
{
    WenaSqliteWorkspaceSeed seed;
    WenaSqliteWorkspaceSeed invalid;
    unsigned char migration[8192];
    size_t length;
    char hash[65];
    char path[4096];
    char race[4096];
    char sentinel[4096];
    char link_path[4096];
    char buffer[32];
    char oversized_id[66];
    char oversized_title[258];
    char max_title[129];
    char oversized_native_title[130];
    char max_actor_name[257];
    FILE *file;
    sqlite3 *database;
    struct stat info;
    pid_t children[4];
    int gate[2];
    int status;
    int success;
    int index;

    assert(argc == 3 && strlen(argv[2]) < 4000);
    file = fopen(argv[1], "rb");
    assert(file != NULL);
    length = fread(migration, 1, sizeof(migration), file);
    assert(length > 0 && length < sizeof(migration));
    assert(fclose(file) == 0);
    wena_sha256_hex(migration, length, hash);
    (void)sprintf(path, "%s/workspace.sqlite", argv[2]);
    (void)sprintf(race, "%s/race.sqlite", argv[2]);
    (void)sprintf(sentinel, "%s/sentinel", argv[2]);
    (void)sprintf(link_path, "%s/symlink.sqlite", argv[2]);
    seed.actor_id = "local_actor";
    seed.actor_name = "Local user";
    seed.board_id = "local-board";
    seed.board_title = "Board '); DROP TABLE actors; --";
    seed.swimlane_id = "default-lane";
    seed.swimlane_title = "Ty\303\266";
    seed.list_id = "default-list";
    seed.list_title = "Todo";

    assert(!wena_sqlite_workspace_create("relative.sqlite", migration, length, hash, &seed));
    assert(!wena_sqlite_workspace_create(NULL, migration, length, hash, &seed));
    assert(!wena_sqlite_workspace_create("/", migration, length, hash, &seed));
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, NULL));
    assert(!wena_sqlite_workspace_create(path, NULL, length, hash, &seed));
    invalid = seed;
    invalid.actor_id = "";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid = seed;
    invalid.board_id = "board';DROP TABLE boards;--";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid = seed;
    invalid.swimlane_id = "lane/other";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid = seed;
    invalid.list_id = NULL;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    memset(oversized_id, 'x', sizeof(oversized_id));
    oversized_id[sizeof(oversized_id) - 1] = '\0';
    invalid = seed;
    invalid.actor_id = oversized_id;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    memset(oversized_title, 'x', sizeof(oversized_title));
    oversized_title[sizeof(oversized_title) - 1] = '\0';
    invalid = seed;
    invalid.board_title = oversized_title;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    memset(oversized_native_title, 'x', sizeof(oversized_native_title) - 1);
    oversized_native_title[sizeof(oversized_native_title) - 1] = '\0';
    invalid = seed;
    invalid.board_title = oversized_native_title;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid = seed;
    invalid.list_title = oversized_native_title;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid = seed;
    invalid.swimlane_title = oversized_native_title;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid = seed;
    invalid.board_title = "";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "Bad\nTitle";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "Bad\177Title";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "\300\257";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "\302\205";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "\355\240\200";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "\364\220\200\200";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    invalid.board_title = "\342\202";
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &invalid));
    assert(access(path, F_OK) != 0);
    no_staging(argv[2]);

    /* Existing migration gate rejects tampering and cleans the private stage. */
    migration[0] ^= 1;
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &seed));
    migration[0] ^= 1;
    assert(access(path, F_OK) != 0);
    no_staging(argv[2]);
    assert(!wena_sqlite_workspace_create(path, migration, length, "bad", &seed));
    no_staging(argv[2]);

    assert(sqlite3_auto_extension((void (*)(void))failing_extension) == SQLITE_OK);
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &seed));
    sqlite3_reset_auto_extension();
    assert(rejected_insert && rollback_seen);
    assert(access(path, F_OK) != 0);
    no_staging(argv[2]);

    assert(wena_sqlite_workspace_create(path, migration, length, hash, &seed));
    assert(stat(path, &info) == 0 && (info.st_mode & 0777) == 0600);
    no_staging(argv[2]);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    assert(wena_sqlite_integrity(database));
    scalar(database, "SELECT display_name FROM actors WHERE id='local_actor'", "Local user");
    scalar(database, "SELECT title FROM boards WHERE id='local-board'", seed.board_title);
    scalar(database, "SELECT title FROM swimlanes WHERE board_id='local-board'", seed.swimlane_title);
    scalar(database, "SELECT title FROM lists WHERE board_id='local-board'", "Todo");
    scalar(database, "SELECT count(*) FROM cards", "0");
    scalar(database, "SELECT count(*) FROM sessions", "0");
    scalar(database, "PRAGMA user_version", "1");
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(!wena_sqlite_workspace_create(path, migration, length, hash, &seed));
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    scalar(database, "SELECT count(*) FROM boards", "1");
    scalar(database, "SELECT title FROM boards", seed.board_title);
    assert(sqlite3_close(database) == SQLITE_OK);

    file = fopen(sentinel, "wb");
    assert(file != NULL && fputs("do not overwrite", file) >= 0);
    assert(fclose(file) == 0);
    assert(!wena_sqlite_workspace_create(sentinel, migration, length, hash, &seed));
    assert(symlink(sentinel, link_path) == 0);
    assert(!wena_sqlite_workspace_create(link_path, migration, length, hash, &seed));
    file = fopen(sentinel, "rb");
    assert(file != NULL);
    memset(buffer, 0, sizeof(buffer));
    assert(fread(buffer, 1, sizeof(buffer), file) == 16);
    assert(strcmp(buffer, "do not overwrite") == 0);
    assert(fclose(file) == 0);
    assert(!wena_sqlite_workspace_create(argv[2], migration, length, hash, &seed));
    assert(unlink(link_path) == 0);
    assert(symlink("missing-target", link_path) == 0);
    assert(!wena_sqlite_workspace_create(link_path, migration, length, hash, &seed));

    /* Four synchronized creators compete; exactly one complete file wins. */
    memset(max_title, 'x', sizeof(max_title) - 1);
    max_title[sizeof(max_title) - 1] = '\0';
    seed.board_title = max_title;
    seed.swimlane_title = max_title;
    seed.list_title = max_title;
    memset(max_actor_name, 'x', sizeof(max_actor_name) - 1);
    max_actor_name[sizeof(max_actor_name) - 1] = '\0';
    seed.actor_name = max_actor_name;
    assert(pipe(gate) == 0);
    for (index = 0; index < 4; ++index) {
        children[index] = fork();
        assert(children[index] >= 0);
        if (children[index] == 0) {
            assert(close(gate[1]) == 0);
            assert(read(gate[0], buffer, 1) == 1);
            assert(close(gate[0]) == 0);
            _exit(wena_sqlite_workspace_create(race, migration, length, hash, &seed) ? 0 : 1);
        }
    }
    assert(close(gate[0]) == 0);
    assert(write(gate[1], "1234", 4) == 4);
    assert(close(gate[1]) == 0);
    success = 0;
    for (index = 0; index < 4; ++index) {
        assert(waitpid(children[index], &status, 0) == children[index]);
        assert(WIFEXITED(status));
        if (WEXITSTATUS(status) == 0) ++success;
        else assert(WEXITSTATUS(status) == 1);
    }
    assert(success == 1);
    no_staging(argv[2]);
    assert(wena_sqlite_open(race, migration, length, hash, &database));
    assert(wena_sqlite_integrity(database));
    scalar(database, "SELECT count(*) FROM actors", "1");
    scalar(database, "SELECT count(*) FROM lists", "1");
    scalar(database, "SELECT title FROM lists", max_title);
    scalar(database, "SELECT title FROM boards", max_title);
    scalar(database, "SELECT title FROM swimlanes", max_title);
    scalar(database, "SELECT display_name FROM actors", max_actor_name);
    assert(sqlite3_close(database) == SQLITE_OK);
    puts("atomic local SQLite workspace tests passed");
    return 0;
}
