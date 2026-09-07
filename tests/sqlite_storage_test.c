#include "../server/sqlite_storage.h"
#include "../server/sha256.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    assert(!wena_sqlite_open(corrupt_path,sql,length,expected,&db));free(sql);return 0;
}
