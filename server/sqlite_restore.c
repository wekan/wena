#define _POSIX_C_SOURCE 200809L
#include "sqlite_restore.h"
#include "sha256.h"
#include "sqlite_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#define PATH_MAXIMUM 1024
#define COPY_MARGIN 1048576ul

static int hash_file(const char *path, char output[65], unsigned long *size)
{
    FILE *file;
    unsigned char bytes[16384];
    size_t length;
    unsigned long total;
    WenaSha256 hash;
    file = fopen(path, "rb");
    if (!file) return 0;
    total = 0;
    wena_sha256_init(&hash);
    while ((length = fread(bytes, 1, sizeof(bytes), file)) > 0) {
        if (total > (unsigned long)-1 - (unsigned long)length) {
            fclose(file); return 0;
        }
        total += (unsigned long)length;
        wena_sha256_update(&hash, bytes, length);
    }
    if (ferror(file)) { fclose(file); return 0; }
    if (fclose(file) != 0) return 0;
    wena_sha256_final_hex(&hash, output);
    *size = total;
    return 1;
}

static int sidecar(const char *path, const char *hash)
{
    char name[PATH_MAXIMUM], line[67];
    FILE *file;
    size_t length;
    int ok;
    sprintf(name, "%s.sha256", path);
    file = fopen(name, "rb");
    if (!file) return 0;
    length = fread(line, 1, sizeof(line), file);
    ok = !ferror(file) && length == 65u && line[64] == '\n' &&
        memcmp(line, hash, 64u) == 0;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int preflight(const char *path, int target, unsigned long *size,
                       char hash[65])
{
    sqlite3 *database;
    int version, ok;
    database = NULL;
    if (!hash_file(path, hash, size) || !sidecar(path, hash)) return 0;
    if (sqlite3_open_v2(path, &database, SQLITE_OPEN_READONLY |
        SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
        if (database) sqlite3_close(database);
        return 0;
    }
    if (!wena_sqlite_connection_harden(database)) {
        sqlite3_close(database);
        return 0;
    }
    version = wena_sqlite_schema_version(database);
    ok = version > 0 && version <= target && wena_sqlite_integrity(database);
    if (sqlite3_close(database) != SQLITE_OK) ok = 0;
    return ok;
}

static int exists(const char *path)
{
#if !defined(_WIN32)
    struct stat info;
    return lstat(path, &info) == 0;
#else
    FILE *file;
    file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
#endif
}

static int copy_file(const char *source, const char *destination)
{
    FILE *input, *output;
    unsigned char bytes[16384];
    size_t length;
    int ok;
#if !defined(_WIN32)
    int descriptor;
#endif
    input = fopen(source, "rb");
    if (!input) return 0;
#if !defined(_WIN32)
    descriptor = open(destination, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) { fclose(input); return 0; }
    output = fdopen(descriptor, "wb");
    if (!output) { close(descriptor); remove(destination); fclose(input); return 0; }
#else
    output = fopen(destination, "wb");
    if (!output) { fclose(input); return 0; }
#endif
    ok = 1;
    while ((length = fread(bytes, 1, sizeof(bytes), input)) > 0)
        if (fwrite(bytes, 1, length, output) != length) { ok = 0; break; }
    if (ferror(input) || fflush(output) != 0) ok = 0;
    if (fclose(output) != 0) ok = 0;
    if (fclose(input) != 0) ok = 0;
    if (!ok) remove(destination);
    return ok;
}

static int prepare_copy(const char *path, const char *expected_hash,
                          unsigned long expected_size, const char *target_hash)
{
    char actual[65];
    unsigned long size;
    sqlite3 *database;
    int ok;
    /* Recheck copied bytes to catch a changed backup between preflight and copy. */
    if (!hash_file(path, actual, &size) || size != expected_size ||
        strcmp(actual, expected_hash) != 0) return 0;
    database = NULL;
    if (sqlite3_open_v2(path, &database, SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
        if (database) sqlite3_close(database);
        return 0;
    }
    if (!wena_sqlite_connection_harden(database)) {
        sqlite3_close(database);
        return 0;
    }
    sqlite3_busy_timeout(database, 5000);
    ok = sqlite3_exec(database, "PRAGMA journal_mode=DELETE;"
        "PRAGMA synchronous=FULL;PRAGMA foreign_keys=ON;", NULL, NULL,
        NULL) == SQLITE_OK && wena_sqlite_upgrade(database, target_hash) &&
        wena_sqlite_schema_validate(database, target_hash) &&
        wena_sqlite_integrity(database);
    if (sqlite3_close(database) != SQLITE_OK) ok = 0;
    return ok;
}

int wena_sqlite_restore(const char *backup, const char *database,
    const char *migration, WenaBackupFreeSpace free_space, void *space_context,
    const WenaRestoreLifecycle *lifecycle)
{
    char temporary[PATH_MAXIMUM], original[PATH_MAXIMUM], hash[65];
    unsigned long size, available;
    int target;
    if (!backup || !database || !migration || !free_space || !lifecycle ||
        !lifecycle->stop || !lifecycle->start || backup[0] != '/' ||
        database[0] != '/' || strlen(backup) > PATH_MAXIMUM - 16u ||
        strlen(database) > PATH_MAXIMUM - 24u) return 0;
    target = wena_sqlite_migration_target(migration);
    if (!target) return 0;
    sprintf(temporary, "%s.restore.tmp", database);
    sprintf(original, "%s.restore-original", database);
    if (exists(temporary) || exists(original) || !exists(database) ||
        !preflight(backup, target, &size, hash)) return 0;
    if (size > (unsigned long)-1 - COPY_MARGIN ||
        !free_space(space_context, database, &available) ||
        available < size + COPY_MARGIN) return 0;
    if (!copy_file(backup, temporary)) return 0;
    if (!prepare_copy(temporary, hash, size, migration)) {
        remove(temporary); return 0;
    }
    if (!lifecycle->stop(lifecycle->context)) { remove(temporary); return 0; }
    if (rename(database, original) != 0) {
        remove(temporary);
        lifecycle->start(lifecycle->context, database);
        return 0;
    }
    if (rename(temporary, database) != 0) {
        rename(original, database);
        lifecycle->start(lifecycle->context, database);
        remove(temporary);
        return 0;
    }
    if (lifecycle->start(lifecycle->context, database)) {
        remove(original); return 1;
    }
    remove(database);
    if (rename(original, database) == 0)
        lifecycle->start(lifecycle->context, database);
    return 0;
}
