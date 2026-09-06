#include "sqlite_storage.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int foreign_keys_clean(sqlite3 *db);

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
    sqlite3 *db; sqlite3_stmt *s; int version; char actual[65]; char insert[256]; char *sql;
    if(database!=NULL)*database=NULL;
    if(path==NULL||migration==NULL||length==0||expected_sha256==NULL||database==NULL)return 0;
    wena_sha256_hex(migration,length,actual);if(strcmp(actual,expected_sha256)!=0)return 0;
    sql=(char *)malloc(length+1u);if(sql==NULL)return 0;memcpy(sql,migration,length);sql[length]='\0';
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK){if(db)sqlite3_close(db);free(sql);return 0;}
    sqlite3_busy_timeout(db,5000);
    if(sqlite3_exec(db,"PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;",NULL,NULL,NULL)!=SQLITE_OK||!scalar_text(db,"PRAGMA quick_check","ok"))goto fail;
    if(sqlite3_prepare_v2(db,"PRAGMA user_version",-1,&s,NULL)!=SQLITE_OK)goto fail;
    if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);goto fail;}version=sqlite3_column_int(s,0);sqlite3_finalize(s);
    if(version==0){if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)goto fail;if(sqlite3_exec(db,sql,NULL,NULL,NULL)!=SQLITE_OK)goto rollback;
        sprintf(insert,"INSERT INTO schema_migrations VALUES(1,'%s',strftime('%%s','now'));PRAGMA user_version=1;",actual);
        if(sqlite3_exec(db,insert,NULL,NULL,NULL)!=SQLITE_OK||sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto rollback;
    }else if(version==1){if(sqlite3_prepare_v2(db,"SELECT checksum FROM schema_migrations WHERE version=1",-1,&s,NULL)!=SQLITE_OK)goto fail;if(sqlite3_step(s)!=SQLITE_ROW||sqlite3_column_text(s,0)==NULL||strcmp((const char *)sqlite3_column_text(s,0),actual)!=0){sqlite3_finalize(s);goto fail;}sqlite3_finalize(s);
    }else goto fail;
    if(!foreign_keys_clean(db)){goto fail;}free(sql);*database=db;return 1;
rollback: sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);
fail: free(sql);sqlite3_close(db);return 0;
}
