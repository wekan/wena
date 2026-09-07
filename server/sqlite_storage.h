#ifndef WENA_SERVER_SQLITE_STORAGE_H
#define WENA_SERVER_SQLITE_STORAGE_H

#include <stddef.h>
#include <sqlite3.h>

/* Configure an application-owned connection before reading its schema:
 * defensive SQL enabled, trusted schema disabled. On failure, close the
 * connection. Does not change triggers or caller-owned snapshot connections. */
int wena_sqlite_connection_harden(sqlite3 *database);

int wena_sqlite_open(const char *path, const unsigned char *migration, size_t length,
                     const char *expected_sha256, sqlite3 **database);
int wena_sqlite_integrity(sqlite3 *database);
/* Read-only, consistent validation of every recorded migration against the
 * compiled ordered registry. NULL accepts any supported schema; a checksum
 * requires that exact compiled target. Empty/unknown/inconsistent histories
 * fail. Also checks exact compiled definitions of newly added v2 objects;
 * historical v1 domain-table structure and arbitrary rows are not fingerprinted. */
int wena_sqlite_schema_validate(sqlite3 *database,
                                const char *expected_sha256);
/* Supported bundle target, or zero for an unknown checksum. */
int wena_sqlite_migration_target(const char *sha256);
/* Validated current schema, or zero for an empty/invalid/unsupported schema. */
int wena_sqlite_schema_version(sqlite3 *database);
/* Upgrade a known existing database using compiled SQL only. The checksum
 * selects a reviewed target; arbitrary SQL is never accepted. Refuses an
 * existing caller transaction, empty schema and downgrade. */
int wena_sqlite_upgrade(sqlite3 *database, const char *target_sha256);

#endif
