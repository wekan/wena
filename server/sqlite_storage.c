#include "sqlite_storage.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int foreign_keys_clean(sqlite3 *db);

/* This reviewed literal is the only schema program the migration runner executes.
 * The artifact footer remains useful for integrity and version discovery, but its
 * bytes are untrusted input until they exactly match this compiled program. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif
static const char migration_v1[] =
"CREATE TABLE schema_migrations (\n"
"  version INTEGER PRIMARY KEY,\n"
"  checksum TEXT NOT NULL UNIQUE,\n"
"  applied_at INTEGER NOT NULL\n"
");\n"
"CREATE TABLE actors (\n"
"  id TEXT PRIMARY KEY,\n"
"  display_name TEXT NOT NULL,\n"
"  version INTEGER NOT NULL CHECK (version > 0)\n"
");\n"
"CREATE TABLE sessions (\n"
"  id TEXT PRIMARY KEY,\n"
"  actor_id TEXT NOT NULL REFERENCES actors(id) ON DELETE RESTRICT,\n"
"  expires_at INTEGER NOT NULL,\n"
"  revoked INTEGER NOT NULL DEFAULT 0 CHECK (revoked IN (0, 1))\n"
");\n"
"CREATE INDEX sessions_actor_idx ON sessions(actor_id);\n"
"CREATE TABLE boards (\n"
"  id TEXT PRIMARY KEY,\n"
"  title TEXT NOT NULL,\n"
"  version INTEGER NOT NULL CHECK (version > 0)\n"
");\n"
"CREATE TABLE swimlanes (\n"
"  id TEXT PRIMARY KEY,\n"
"  board_id TEXT NOT NULL REFERENCES boards(id) ON DELETE RESTRICT,\n"
"  title TEXT NOT NULL,\n"
"  position INTEGER NOT NULL CHECK (position >= 0),\n"
"  version INTEGER NOT NULL CHECK (version > 0),\n"
"  UNIQUE (board_id, id),\n"
"  UNIQUE (board_id, position)\n"
");\n"
"CREATE TABLE lists (\n"
"  id TEXT PRIMARY KEY,\n"
"  board_id TEXT NOT NULL REFERENCES boards(id) ON DELETE RESTRICT,\n"
"  title TEXT NOT NULL,\n"
"  position INTEGER NOT NULL CHECK (position >= 0),\n"
"  version INTEGER NOT NULL CHECK (version > 0),\n"
"  UNIQUE (board_id, id),\n"
"  UNIQUE (board_id, position)\n"
");\n"
"CREATE TABLE cards (\n"
"  id TEXT PRIMARY KEY,\n"
"  board_id TEXT NOT NULL REFERENCES boards(id) ON DELETE RESTRICT,\n"
"  swimlane_id TEXT NOT NULL,\n"
"  list_id TEXT NOT NULL,\n"
"  title TEXT NOT NULL,\n"
"  position INTEGER NOT NULL CHECK (position >= 0),\n"
"  archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),\n"
"  version INTEGER NOT NULL CHECK (version > 0),\n"
"  UNIQUE (list_id, swimlane_id, position),\n"
"  FOREIGN KEY (board_id, swimlane_id) REFERENCES swimlanes(board_id, id) ON DELETE RESTRICT,\n"
"  FOREIGN KEY (board_id, list_id) REFERENCES lists(board_id, id) ON DELETE RESTRICT\n"
");\n"
"CREATE INDEX cards_board_idx ON cards(board_id);\n"
"CREATE INDEX cards_swimlane_idx ON cards(swimlane_id);\n"
"CREATE TABLE idempotency_keys (\n"
"  actor_id TEXT NOT NULL REFERENCES actors(id) ON DELETE RESTRICT,\n"
"  route TEXT NOT NULL,\n"
"  operation TEXT NOT NULL,\n"
"  request_version INTEGER NOT NULL CHECK (request_version > 0),\n"
"  response_checksum TEXT NOT NULL,\n"
"  committed_at INTEGER NOT NULL,\n"
"  PRIMARY KEY (actor_id, route, operation, request_version)\n"
");\n";
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static const char migration_v1_sha256[] =
"e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";

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

int wena_sqlite_open(const char *path, const unsigned char *migration, size_t length,
                     const char *expected_sha256, sqlite3 **database)
{
    sqlite3 *db; sqlite3_stmt *s; int version; char actual[65];
    if(database!=NULL)*database=NULL;
    if(path==NULL||migration==NULL||length==0||expected_sha256==NULL||database==NULL)return 0;
    wena_sha256_hex(migration,length,actual);
    if(strcmp(expected_sha256,migration_v1_sha256)!=0||
       strcmp(actual,migration_v1_sha256)!=0||length!=sizeof(migration_v1)-1u||
       memcmp(migration,migration_v1,length)!=0)return 0;
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK){if(db)sqlite3_close(db);return 0;}
    sqlite3_busy_timeout(db,5000);
    if(sqlite3_exec(db,"PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;",NULL,NULL,NULL)!=SQLITE_OK||!scalar_text(db,"PRAGMA quick_check","ok"))goto fail;
    if(sqlite3_prepare_v2(db,"PRAGMA user_version",-1,&s,NULL)!=SQLITE_OK)goto fail;
    if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);goto fail;}version=sqlite3_column_int(s,0);sqlite3_finalize(s);
    if(version==0){if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)goto fail;if(sqlite3_exec(db,migration_v1,NULL,NULL,NULL)!=SQLITE_OK)goto rollback;
        if(sqlite3_prepare_v2(db,"INSERT INTO schema_migrations VALUES(1,?1,strftime('%s','now'))",-1,&s,NULL)!=SQLITE_OK)goto rollback;
        if(sqlite3_bind_text(s,1,migration_v1_sha256,-1,SQLITE_STATIC)!=SQLITE_OK||sqlite3_step(s)!=SQLITE_DONE){sqlite3_finalize(s);goto rollback;}sqlite3_finalize(s);
        if(sqlite3_exec(db,"PRAGMA user_version=1",NULL,NULL,NULL)!=SQLITE_OK||sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto rollback;
    }else if(version==1){if(sqlite3_prepare_v2(db,"SELECT checksum FROM schema_migrations WHERE version=1",-1,&s,NULL)!=SQLITE_OK)goto fail;if(sqlite3_step(s)!=SQLITE_ROW||sqlite3_column_text(s,0)==NULL||strcmp((const char *)sqlite3_column_text(s,0),actual)!=0){sqlite3_finalize(s);goto fail;}sqlite3_finalize(s);
    }else goto fail;
    if(!foreign_keys_clean(db)){goto fail;}*database=db;return 1;
rollback: sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);
fail: sqlite3_close(db);return 0;
}
