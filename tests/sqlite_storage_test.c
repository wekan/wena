#include "../server/sqlite_storage.h"
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

static unsigned char *read_all(const char *path, size_t *length)
{
    FILE *file; long size; unsigned char *data;
    file=fopen(path,"rb");assert(file!=NULL);assert(fseek(file,0,SEEK_END)==0);
    size=ftell(file);assert(size>0);assert(fseek(file,0,SEEK_SET)==0);
    data=(unsigned char *)malloc((size_t)size);assert(data!=NULL);
    assert(fread(data,1,(size_t)size,file)==(size_t)size);assert(fclose(file)==0);
    *length=(size_t)size;return data;
}

static int integer(sqlite3 *db,const char *sql)
{ sqlite3_stmt *s;int value;assert(sqlite3_prepare_v2(db,sql,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);value=sqlite3_column_int(s,0);sqlite3_finalize(s);return value; }

static const char *invalid_history[] = {
    "DELETE FROM schema_migrations",
    "DROP TABLE schema_migrations",
    "INSERT INTO schema_migrations VALUES(2,'future',0)",
    "INSERT INTO schema_migrations VALUES(0,'zero',0)",
    "INSERT INTO schema_migrations VALUES(-1,'negative',0)",
    "UPDATE schema_migrations SET version=2",
    "UPDATE schema_migrations SET checksum='wrong'",
    "UPDATE schema_migrations SET checksum=CAST(checksum AS BLOB)",
    "UPDATE schema_migrations SET checksum=checksum||char(0)||'suffix'",
    "UPDATE schema_migrations SET applied_at='not-a-time'",
    "UPDATE schema_migrations SET applied_at=-1",
    "PRAGMA user_version=0",
    "PRAGMA user_version=-1",
    "PRAGMA user_version=2",
    "PRAGMA user_version=2147483647",
    "ALTER TABLE schema_migrations RENAME TO history;"
    "CREATE VIEW schema_migrations AS SELECT * FROM history",
    "ALTER TABLE schema_migrations RENAME TO history;"
    "CREATE TABLE schema_migrations(version,checksum,applied_at);"
    "INSERT INTO schema_migrations SELECT CAST(version AS TEXT),checksum,applied_at FROM history",
    "ALTER TABLE schema_migrations RENAME TO history;"
    "CREATE TABLE schema_migrations(version,checksum,applied_at);"
    "INSERT INTO schema_migrations SELECT 1.0,checksum,applied_at FROM history",
    "ALTER TABLE schema_migrations RENAME TO history;"
    "CREATE TABLE schema_migrations(version,checksum,applied_at);"
    "INSERT INTO schema_migrations SELECT version,NULL,applied_at FROM history",
    "ALTER TABLE schema_migrations RENAME TO history;"
    "CREATE TABLE schema_migrations(version,checksum,applied_at);"
    "INSERT INTO schema_migrations SELECT version,checksum,applied_at FROM history;"
    "INSERT INTO schema_migrations SELECT version,checksum,applied_at FROM history"
};

static void history_tests(const char *directory, const unsigned char *sql,
                          size_t length, const char *expected)
{
    sqlite3 *db;
    char path[512];
    size_t index;
    assert(!wena_sqlite_schema_validate(NULL, NULL));
    for (index = 0; index < sizeof(invalid_history) / sizeof(invalid_history[0]);
        ++index) {
        sprintf(path, "%s/history-%lu.sqlite", directory, (unsigned long)index);
        assert(wena_sqlite_open(path, sql, length, expected, &db));
        assert(wena_sqlite_schema_validate(db, NULL));
        assert(wena_sqlite_schema_validate(db, expected));
        assert(!wena_sqlite_schema_validate(db, "wrong"));
        assert(sqlite3_get_autocommit(db));
        assert(sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK);
        assert(wena_sqlite_schema_validate(db, expected));
        assert(!sqlite3_get_autocommit(db));
        assert(sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK);
        assert(sqlite3_exec(db, invalid_history[index], NULL, NULL, NULL) == SQLITE_OK);
        assert(!wena_sqlite_schema_validate(db, expected));
        assert(sqlite3_get_autocommit(db));
        assert(sqlite3_close(db) == SQLITE_OK);
        db = (sqlite3 *)sql;
        assert(!wena_sqlite_open(path, sql, length, expected, &db));
        assert(db == NULL);
    }
}

static int failure_mode, rollback_seen, failure_seen;

static int deny_migration(void *context, int action, const char *first,
                          const char *second, const char *database,
                          const char *trigger)
{
    (void)context; (void)second; (void)database; (void)trigger;
    if (first != NULL && action == SQLITE_TRANSACTION &&
        strcmp(first, "ROLLBACK") == 0) rollback_seen = 1;
    if (first != NULL &&
        ((failure_mode == 1 && action == SQLITE_CREATE_TABLE &&
          strcmp(first, "cards") == 0) ||
         (failure_mode == 2 && action == SQLITE_INSERT &&
          strcmp(first, "schema_migrations") == 0) ||
         (failure_mode == 3 && action == SQLITE_TRANSACTION &&
          strcmp(first, "COMMIT") == 0))) {
        failure_seen = 1;
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static int failing_extension(sqlite3 *db, char **error,
                              const sqlite3_api_routines *api)
{
    (void)error; (void)api;
    return sqlite3_set_authorizer(db, deny_migration, NULL);
}

static void atomic_tests(const char *directory, const unsigned char *sql,
                         size_t length, const char *expected)
{
    sqlite3 *db;
    char path[512];
    for (failure_mode = 1; failure_mode <= 3; ++failure_mode) {
        sprintf(path, "%s/atomic-%d.sqlite", directory, failure_mode);
        rollback_seen = failure_seen = 0;
        assert(sqlite3_auto_extension((void (*)(void))failing_extension) == SQLITE_OK);
        assert(!wena_sqlite_open(path, sql, length, expected, &db));
        assert(db == NULL && failure_seen && rollback_seen);
        sqlite3_reset_auto_extension();
        assert(sqlite3_open(path, &db) == SQLITE_OK);
        assert(integer(db, "PRAGMA user_version") == 0);
        assert(integer(db, "SELECT count(*) FROM sqlite_master WHERE "
            "name NOT LIKE 'sqlite_%'") == 0);
        assert(sqlite3_close(db) == SQLITE_OK);
        assert(wena_sqlite_open(path, sql, length, expected, &db));
        assert(wena_sqlite_schema_validate(db, expected));
        assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/unrelated.sqlite", directory);
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(sqlite3_exec(db, "CREATE TABLE unrelated(id);INSERT INTO unrelated VALUES(7)",
        NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(!wena_sqlite_open(path, sql, length, expected, &db));
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    assert(integer(db, "PRAGMA user_version") == 0);
    assert(integer(db, "SELECT id FROM unrelated") == 7);
    assert(integer(db, "SELECT count(*) FROM sqlite_master") == 1);
    assert(sqlite3_close(db) == SQLITE_OK);
}

#if !defined(_WIN32)
static void concurrent_tests(const char *directory, const unsigned char *sql,
                             size_t length, const char *expected)
{
    char path[512], token;
    sqlite3 *db;
    pid_t children[4];
    int gate[2], index, status, successes, opened;
    sprintf(path, "%s/concurrent.sqlite", directory);
    assert(pipe(gate) == 0);
    for (index = 0; index < 4; ++index) {
        children[index] = fork();
        assert(children[index] >= 0);
        if (children[index] == 0) {
            close(gate[1]);
            if (read(gate[0], &token, 1) != 1) _exit(2);
            close(gate[0]);
            opened = wena_sqlite_open(path, sql, length, expected, &db);
            if (opened) {
                if (!wena_sqlite_schema_validate(db, expected)) _exit(3);
                if (sqlite3_close(db) != SQLITE_OK) _exit(4);
            } else if (db != NULL) _exit(5);
            _exit(opened ? 0 : 1);
        }
    }
    close(gate[0]);
    assert(write(gate[1], "go!!", 4) == 4);
    close(gate[1]);
    successes = 0;
    for (index = 0; index < 4; ++index) {
        assert(waitpid(children[index], &status, 0) == children[index]);
        assert(WIFEXITED(status));
        assert(WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 1);
        if (WEXITSTATUS(status) == 0) ++successes;
    }
    /* Initial journal-mode contention may fail closed. Successful openers must
     * agree on one complete migration, never partially create the same schema. */
    assert(successes > 0);
    assert(wena_sqlite_open(path, sql, length, expected, &db));
    assert(integer(db, "PRAGMA user_version") == 1);
    assert(integer(db, "SELECT count(*) FROM schema_migrations") == 1);
    assert(wena_sqlite_schema_validate(db, expected));
    assert(sqlite3_close(db) == SQLITE_OK);
}
#endif

int main(int argc,char **argv)
{
    static const char expected[]="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    static const char injection[]="DROP TABLE schema_migrations;--\n";
    unsigned char *sql,*injected;size_t length,injected_length;char hash[65],db_path[512],bad_path[512],corrupt_path[512];sqlite3 *db;FILE *file;
    const unsigned char bad_sql[]="CREATE TABLE partial(id);THIS IS INVALID;";
    assert(argc==3);sql=read_all(argv[1],&length);wena_sha256_hex(sql,length,hash);assert(strcmp(hash,expected)==0);
    wena_sha256_hex((const unsigned char *)"abc",3u,hash);assert(strcmp(hash,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0);
    sprintf(db_path,"%s/wena.sqlite",argv[2]);sprintf(bad_path,"%s/bad.sqlite",argv[2]);sprintf(corrupt_path,"%s/corrupt.sqlite",argv[2]);
    assert(wena_sqlite_open(db_path,sql,length,expected,&db));assert(integer(db,"PRAGMA user_version")==1);assert(wena_sqlite_integrity(db));sqlite3_close(db);
    assert(wena_sqlite_open(db_path,sql,length,expected,&db));assert(integer(db,"SELECT count(*) FROM schema_migrations")==1);
    assert(sqlite3_exec(db,"BEGIN IMMEDIATE;INSERT INTO actors VALUES('crash','Crash',1);",NULL,NULL,NULL)==SQLITE_OK);sqlite3_close(db);
    assert(wena_sqlite_open(db_path,sql,length,expected,&db));assert(integer(db,"SELECT count(*) FROM actors WHERE id='crash'")==0);
    assert(sqlite3_exec(db,"PRAGMA user_version=2",NULL,NULL,NULL)==SQLITE_OK);sqlite3_close(db);assert(!wena_sqlite_open(db_path,sql,length,expected,&db));
    sql[0]^=1u;assert(!wena_sqlite_open(bad_path,sql,length,expected,&db));sql[0]^=1u;
    wena_sha256_hex(bad_sql,sizeof(bad_sql)-1u,hash);assert(!wena_sqlite_open(bad_path,bad_sql,sizeof(bad_sql)-1u,hash,&db));
    injected_length=length+sizeof(injection)-1u;injected=(unsigned char *)malloc(injected_length);assert(injected!=NULL);
    memcpy(injected,sql,length);memcpy(injected+length,injection,sizeof(injection)-1u);
    wena_sha256_hex(injected,injected_length,hash);assert(!wena_sqlite_open(bad_path,injected,injected_length,hash,&db));free(injected);
    assert(sqlite3_open(bad_path,&db)==SQLITE_OK);assert(integer(db,"PRAGMA user_version")==0);sqlite3_close(db);
    file=fopen(corrupt_path,"wb");assert(file!=NULL);assert(fwrite("not sqlite",1,10,file)==10);assert(fclose(file)==0);
    assert(!wena_sqlite_open(corrupt_path,sql,length,expected,&db));
    history_tests(argv[2], sql, length, expected);
    atomic_tests(argv[2], sql, length, expected);
#if !defined(_WIN32)
    concurrent_tests(argv[2], sql, length, expected);
#endif
    free(sql);return 0;
}
