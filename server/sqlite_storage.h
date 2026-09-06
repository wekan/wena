#ifndef WENA_SERVER_SQLITE_STORAGE_H
#define WENA_SERVER_SQLITE_STORAGE_H

#include <stddef.h>
#include <sqlite3.h>

int wena_sqlite_open(const char *path, const unsigned char *migration, size_t length,
                     const char *expected_sha256, sqlite3 **database);
int wena_sqlite_integrity(sqlite3 *database);

#endif
