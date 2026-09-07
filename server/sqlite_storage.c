#include "sqlite_storage.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int foreign_keys_clean(sqlite3 *db);

typedef struct WenaCompiledMigration {
    int version;
    const char *sql;
    size_t length;
    const char *sha256;
    size_t bundle_length;
    const char *bundle_sha256;
} WenaCompiledMigration;

typedef struct WenaSchemaObject {
    int version;
    const char *name;
    const char *type;
    const char *sql;
} WenaSchemaObject;

/* Generated, checked-in reviewed SQL is the only program executed by the
 * migration runner. Untrusted artifact bytes must exactly identify its prefix. */
#include "migrations/compiled_registry.h"

#define MIGRATION_COUNT (sizeof(migrations) / sizeof(migrations[0]))

static const WenaCompiledMigration *target_for_hash(const char *sha256)
{
    size_t index;
    if (sha256 == NULL) return NULL;
    for (index = 0; index < MIGRATION_COUNT; ++index)
        if (strcmp(sha256, migrations[index].bundle_sha256) == 0)
            return &migrations[index];
    return NULL;
}

int wena_sqlite_migration_target(const char *sha256)
{
    const WenaCompiledMigration *target;
    target = target_for_hash(sha256);
    return target != NULL ? target->version : 0;
}

static int scalar_text(sqlite3 *db, const char *sql, const char *expected)
{
    sqlite3_stmt *statement; int ok;
    if (sqlite3_prepare_v2(db,sql,-1,&statement,NULL)!=SQLITE_OK) return 0;
    ok=sqlite3_step(statement)==SQLITE_ROW && sqlite3_column_text(statement,0)!=NULL &&
       strcmp((const char *)sqlite3_column_text(statement,0),expected)==0;
    sqlite3_finalize(statement); return ok;
}

int wena_sqlite_integrity(sqlite3 *database)
{
    return database!=NULL && scalar_text(database,"PRAGMA integrity_check","ok") &&
           foreign_keys_clean(database);
}

static int foreign_keys_clean(sqlite3 *db)
{
    sqlite3_stmt *s; int clean;
    if(sqlite3_prepare_v2(db,"PRAGMA foreign_key_check",-1,&s,NULL)!=SQLITE_OK)return 0;
    clean=sqlite3_step(s)==SQLITE_DONE;sqlite3_finalize(s);return clean;
}

static int database_version(sqlite3 *db, int *version)
{
    sqlite3_stmt *statement;
    sqlite3_int64 value;
    int ok;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &statement,
        NULL) != SQLITE_OK) return 0;
    ok = sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER;
    if (ok) {
        value = sqlite3_column_int64(statement, 0);
        ok = value >= 0 && value <= (sqlite3_int64)MIGRATION_COUNT;
        if (ok) *version = (int)value;
    }
    sqlite3_finalize(statement);
    return ok;
}

static int required_objects_valid(sqlite3 *db, int version)
{
    sqlite3_stmt *statement;
    const unsigned char *sql;
    size_t index, length;
    int ok;
    for (index = 0; index < sizeof(schema_objects) / sizeof(schema_objects[0]); ++index) {
        if (schema_objects[index].version > version) continue;
        if (sqlite3_prepare_v2(db, "SELECT sql FROM sqlite_master WHERE name=?1 "
            "AND type=?2", -1, &statement, NULL) != SQLITE_OK) return 0;
        sqlite3_bind_text(statement, 1, schema_objects[index].name, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, schema_objects[index].type, -1, SQLITE_STATIC);
        length = strlen(schema_objects[index].sql);
        ok = sqlite3_step(statement) == SQLITE_ROW &&
            sqlite3_column_type(statement, 0) == SQLITE_TEXT &&
            (size_t)sqlite3_column_bytes(statement, 0) == length;
        if (ok) {
            sql = sqlite3_column_text(statement, 0);
            ok = sql != NULL && memcmp(sql, schema_objects[index].sql, length) == 0;
        }
        if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
        sqlite3_finalize(statement);
        if (!ok) return 0;
    }
    return 1;
}

static int history_valid(sqlite3 *db, const char *expected_sha256)
{
    sqlite3_stmt *statement;
    const unsigned char *checksum;
    int version, step, ok;
    size_t index;
    if (!database_version(db, &version) || version == 0) return 0;
    if (expected_sha256 != NULL &&
        strcmp(expected_sha256, migrations[version - 1].bundle_sha256) != 0) return 0;
    if (!scalar_text(db, "SELECT type FROM sqlite_master WHERE "
        "name='schema_migrations'", "table")) return 0;
    if (sqlite3_prepare_v2(db, "SELECT version,checksum,applied_at FROM "
        "schema_migrations ORDER BY version", -1, &statement,
        NULL) != SQLITE_OK) return 0;
    index = 0;
    ok = 1;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (index >= (size_t)version ||
            sqlite3_column_type(statement, 0) != SQLITE_INTEGER ||
            sqlite3_column_int64(statement, 0) != migrations[index].version ||
            sqlite3_column_type(statement, 1) != SQLITE_TEXT ||
            sqlite3_column_bytes(statement, 1) != 64 ||
            sqlite3_column_type(statement, 2) != SQLITE_INTEGER ||
            sqlite3_column_int64(statement, 2) < 0) {
            ok = 0;
            break;
        }
        checksum = sqlite3_column_text(statement, 1);
        if (checksum == NULL || memcmp(checksum,
            migrations[index].sha256, 64u) != 0) {
            ok = 0;
            break;
        }
        ++index;
    }
    if (step != SQLITE_DONE || index != (size_t)version) ok = 0;
    sqlite3_finalize(statement);
    return ok && required_objects_valid(db, version);
}

static int checked_schema_version(sqlite3 *database,
                                    const char *expected_sha256)
{
    int owns_transaction, ok, version;
    if (database == NULL) return 0;
    owns_transaction = sqlite3_get_autocommit(database) != 0;
    if (owns_transaction && sqlite3_exec(database, "BEGIN", NULL, NULL,
        NULL) != SQLITE_OK) return 0;
    ok = history_valid(database, expected_sha256) &&
        database_version(database, &version);
    if (owns_transaction) {
        if (ok && sqlite3_exec(database, "COMMIT", NULL, NULL,
            NULL) != SQLITE_OK) ok = 0;
        if (!ok) sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
    }
    return ok ? version : 0;
}

int wena_sqlite_schema_validate(sqlite3 *database,
                                const char *expected_sha256)
{
    return checked_schema_version(database, expected_sha256) != 0;
}

int wena_sqlite_schema_version(sqlite3 *database)
{
    return checked_schema_version(database, NULL);
}

static int empty_schema(sqlite3 *db)
{
    return scalar_text(db, "SELECT count(*) FROM sqlite_master WHERE "
        "name NOT LIKE 'sqlite_%'", "0");
}

static int apply_migration(sqlite3 *db, const WenaCompiledMigration *migration)
{
    sqlite3_stmt *statement;
    char pragma[48];
    int ok;
    if (sqlite3_exec(db, migration->sql, NULL, NULL, NULL) != SQLITE_OK)
        return 0;
    if (sqlite3_prepare_v2(db, "INSERT INTO schema_migrations"
        "(version,checksum,applied_at) VALUES(?1,?2,strftime('%s','now'))",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    ok = sqlite3_bind_int(statement, 1, migration->version) == SQLITE_OK &&
        sqlite3_bind_text(statement, 2, migration->sha256, -1,
            SQLITE_STATIC) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok) return 0;
    sprintf(pragma, "PRAGMA user_version=%d", migration->version);
    return sqlite3_exec(db, pragma, NULL, NULL, NULL) == SQLITE_OK;
}

int wena_sqlite_connection_harden(sqlite3 *database)
{
#if defined(SQLITE_DBCONFIG_DEFENSIVE) && defined(SQLITE_DBCONFIG_TRUSTED_SCHEMA)
    int enabled;
    if (database == NULL) return 0;
    enabled = 0;
    if (sqlite3_db_config(database, SQLITE_DBCONFIG_DEFENSIVE, 1,
                          &enabled) != SQLITE_OK || enabled != 1) return 0;
    enabled = 1;
    if (sqlite3_db_config(database, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0,
                          &enabled) != SQLITE_OK || enabled != 0) return 0;
    return 1;
#else
    /* Do not silently open an unhardened application connection when a
       platform's SQLite headers cannot express the required protections. */
    (void)database;
    return 0;
#endif
}

static int migrate(sqlite3 *db, const WenaCompiledMigration *target,
                    int allow_empty)
{
    int version;
    size_t index;
    if (!db || !target || !sqlite3_get_autocommit(db)) return 0;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK)
        return 0;
    if (!database_version(db, &version) || version > target->version) goto fail;
    if (version == 0) {
        if (!allow_empty || !empty_schema(db)) goto fail;
    } else if (!history_valid(db, NULL)) goto fail;
    for (index = (size_t)version; index < (size_t)target->version; ++index)
        if (!apply_migration(db, &migrations[index])) goto fail;
    if (!history_valid(db, target->bundle_sha256) || !foreign_keys_clean(db)) goto fail;
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) goto fail;
    return 1;
fail:
    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}

int wena_sqlite_upgrade(sqlite3 *database, const char *target_sha256)
{
    const WenaCompiledMigration *target;
    target = target_for_hash(target_sha256);
    if (!database || !target || !sqlite3_get_autocommit(database)) return 0;
    if (sqlite3_exec(database, "PRAGMA foreign_keys=ON", NULL, NULL,
        NULL) != SQLITE_OK || !wena_sqlite_integrity(database)) return 0;
    return migrate(database, target, 0);
}

int wena_sqlite_open(const char *path, const unsigned char *migration, size_t length,
                     const char *expected_sha256, sqlite3 **database)
{
    sqlite3 *db; char actual[65];
    const WenaCompiledMigration *target;
    size_t index, offset;
    if(database!=NULL)*database=NULL;
    if(path==NULL||migration==NULL||length==0||expected_sha256==NULL||database==NULL)return 0;
    target = target_for_hash(expected_sha256);
    if (target == NULL || length != target->bundle_length) return 0;
    wena_sha256_hex(migration,length,actual);
    if(strcmp(actual,target->bundle_sha256)!=0)return 0;
    offset = 0;
    for (index = 0; index < (size_t)target->version; ++index) {
        if (migrations[index].length > length - offset ||
            memcmp(migration + offset, migrations[index].sql,
                migrations[index].length) != 0) return 0;
        offset += migrations[index].length;
    }
    if (offset != length) return 0;
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK){if(db)sqlite3_close(db);return 0;}
    if (!wena_sqlite_connection_harden(db)) goto fail;
    sqlite3_busy_timeout(db,5000);
    if(sqlite3_exec(db,"PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;",NULL,NULL,NULL)!=SQLITE_OK||!scalar_text(db,"PRAGMA quick_check","ok"))goto fail;
    /* Read version/history only after obtaining the writer lock. A concurrent
     * creator may have finished the migration while this opener was waiting. */
    if(!migrate(db,target,1))goto fail;
    *database=db;return 1;
fail: sqlite3_close(db);return 0;
}
