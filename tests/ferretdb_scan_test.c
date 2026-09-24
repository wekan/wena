#include "../server/ferretdb_scan.h"
#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *d,const char *s){assert(sqlite3_exec(d,s,NULL,NULL,NULL)==SQLITE_OK);}
static unsigned char *bytes(const char *path,size_t *length)
{
    FILE *f;long size;unsigned char *result;f=fopen(path,"rb");assert(f);
    assert(fseek(f,0,SEEK_END)==0);size=ftell(f);assert(size>=0);rewind(f);
    result=(unsigned char*)malloc((size_t)size+1);assert(result);
    assert(fread(result,1,(size_t)size,f)==(size_t)size);fclose(f);*length=(size_t)size;return result;
}
static void check(const char *path,size_t limit,int expected,size_t count)
{
    size_t got,n,m;unsigned char *before,*after;before=bytes(path,&n);got=999;
    assert(wena_ferretdb_scan_readonly(path,limit,&got)==expected);
    assert(got==(expected?count:999));after=bytes(path,&m);
    assert(n==m&&!memcmp(before,after,n));free(before);free(after);
}
int main(int ac,char **av)
{
    char path[1024];sqlite3 *d;size_t count;assert(ac==2);
    sprintf(path,"%s/scan.sqlite",av[1]);assert(sqlite3_open(path,&d)==SQLITE_OK);
    sql(d,"CREATE TABLE _ferretdb_collections(name TEXT NOT NULL UNIQUE,table_name TEXT NOT NULL UNIQUE,settings TEXT NOT NULL) STRICT;"
        "CREATE TABLE b(_ferretdb_sjson TEXT NOT NULL) STRICT;CREATE TABLE l(_ferretdb_sjson TEXT NOT NULL) STRICT;"
        "CREATE TABLE s(_ferretdb_sjson TEXT NOT NULL) STRICT;CREATE TABLE c(_ferretdb_sjson TEXT NOT NULL) STRICT;");
    sql(d,"INSERT INTO _ferretdb_collections VALUES('boards','b','{\"indexFormat\":2}'),('lists','l','{\"indexFormat\":2}'),"
        "('swimlanes','s','{\"indexFormat\":2}'),('cards','c','{\"indexFormat\":2}')");
    check(path,0,1,0);
    sql(d,"INSERT INTO b VALUES('{\"$s\":{}}');INSERT INTO c VALUES('{\"$s\":{}}')");
    check(path,2,1,2);check(path,1,0,0);check(path,0,0,0);
    sql(d,"INSERT INTO c VALUES('{\"$s\":{\"p\":{\"v\":{\"t\":\"int\"}},\"$k\":[\"v\"]},\"v\":2147483648}')");
    check(path,10,0,0);sql(d,"DELETE FROM c WHERE rowid=2");
    sql(d,"INSERT INTO c VALUES(CAST(x'7b222473223a7b7d7d0061' AS TEXT))");check(path,10,0,0);
    sql(d,"DELETE FROM c WHERE rowid=2;CREATE TABLE \"quoted\"\"table\"(_ferretdb_sjson TEXT NOT NULL) STRICT;");
    sql(d,"INSERT INTO _ferretdb_collections VALUES('other','quoted\"table','{\"indexFormat\":2}');"
        "INSERT INTO \"quoted\"\"table\" VALUES('{\"$s\":{}}')");check(path,3,1,3);
    sql(d,"UPDATE _ferretdb_collections SET settings='{\"indexFormat\":3}' WHERE name='other'");check(path,10,0,0);
    sql(d,"UPDATE _ferretdb_collections SET settings='bad json' WHERE name='other'");check(path,10,0,0);
    sql(d,"UPDATE _ferretdb_collections SET settings='{\"indexFormat\":2}',table_name='missing' WHERE name='other'");check(path,10,0,0);
    sql(d,"CREATE TABLE extra(_ferretdb_sjson TEXT NOT NULL,hidden_value TEXT) STRICT;"
        "UPDATE _ferretdb_collections SET table_name='extra' WHERE name='other'");check(path,10,0,0);
    sql(d,"DROP TABLE extra;CREATE TABLE extra(_ferretdb_sjson TEXT NOT NULL)");check(path,10,0,0);
    sql(d,"DELETE FROM _ferretdb_collections WHERE name='other'");check(path,2,1,2);
    sql(d,"PRAGMA application_id=1");check(path,10,0,0);sql(d,"PRAGMA application_id=0");
    sql(d,"BEGIN EXCLUSIVE");check(path,10,0,0);sql(d,"ROLLBACK");check(path,2,1,2);
    sqlite3_close(d);count=999;
    assert(!wena_ferretdb_scan_readonly("relative.sqlite",10,&count)&&count==999);
    assert(!wena_ferretdb_scan_readonly(path,10,NULL));
    puts("FerretDB read-only scan: passed");return 0;
}
