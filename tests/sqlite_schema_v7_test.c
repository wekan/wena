#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WENA_SETTING_SCHEMA_VERSION
#define WENA_SETTING_SCHEMA_VERSION 7
#define WENA_SETTING_TABLE "board_minicard_settings"
#define WENA_SETTING_COLUMN "show_checklists"
#endif
typedef struct Bundle {unsigned char *bytes;size_t length;char hash[65];} Bundle;
static int failure;
static int authorize(void *data,int action,const char *first,const char *second,
 const char *database,const char *trigger)
{
 (void)data;(void)second;(void)database;(void)trigger;
 if((failure==1&&action==SQLITE_CREATE_TABLE&&first&&!strcmp(first,WENA_SETTING_TABLE))||
    (failure==2&&action==SQLITE_INSERT&&first&&!strcmp(first,"schema_migrations"))||
    (failure==3&&action==SQLITE_TRANSACTION&&first&&!strcmp(first,"COMMIT")))return SQLITE_DENY;
 return SQLITE_OK;
}
static void sql(sqlite3 *db,const char *text){assert(sqlite3_exec(db,text,NULL,NULL,NULL)==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *text)
{
 sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,text,-1,&s,NULL)==SQLITE_OK);
 assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;
}
static void read_bundle(const char *path,Bundle *bundle)
{
 FILE *f;long size;f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));size=ftell(f);assert(size>0);rewind(f);
 bundle->length=(size_t)size;bundle->bytes=(unsigned char*)malloc(bundle->length);assert(bundle->bytes);
 assert(fread(bundle->bytes,1,bundle->length,f)==bundle->length);assert(!fclose(f));
 wena_sha256_hex(bundle->bytes,bundle->length,bundle->hash);
}
int main(int argc,char **argv)
{
 Bundle bundles[WENA_SETTING_SCHEMA_VERSION];sqlite3 *db;char path[1024];int old,mode;size_t i;
 assert(argc==WENA_SETTING_SCHEMA_VERSION+2&&strlen(argv[WENA_SETTING_SCHEMA_VERSION+1])<900);
 for(i=0;i<WENA_SETTING_SCHEMA_VERSION;++i){read_bundle(argv[i+1],&bundles[i]);assert(wena_sqlite_migration_target(bundles[i].hash)==(int)i+1);}
 for(old=1;old<WENA_SETTING_SCHEMA_VERSION;++old){
  assert(!memcmp(bundles[old-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[old-1].length));
  sprintf(path,"%s/from-%d.sqlite",argv[WENA_SETTING_SCHEMA_VERSION+1],old);
  assert(wena_sqlite_open(path,bundles[old-1].bytes,bundles[old-1].length,bundles[old-1].hash,&db));
  sql(db,"INSERT INTO boards VALUES('b','Keep title',42)");
  if(old>=6)sql(db,"INSERT INTO board_settings VALUES('b',1)");
  assert(sqlite3_close(db)==SQLITE_OK);
  assert(wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].length,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash,&db));
  assert(wena_sqlite_schema_version(db)==WENA_SETTING_SCHEMA_VERSION&&number(db,"SELECT version FROM boards WHERE id='b'")==42);
  assert(number(db,"SELECT count(*) FROM " WENA_SETTING_TABLE "")==0);
  if(old>=6)assert(number(db,"SELECT show_checklist_count FROM board_settings WHERE board_id='b'")==1);
  sql(db,"INSERT INTO " WENA_SETTING_TABLE "(board_id) VALUES('b')");
  assert(number(db,"SELECT " WENA_SETTING_COLUMN " FROM " WENA_SETTING_TABLE "")==1);
  assert(sqlite3_exec(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "=2",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "=NULL",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "='bad'",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"INSERT INTO " WENA_SETTING_TABLE " VALUES('missing',0)",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"DELETE FROM boards WHERE id='b'",NULL,NULL,NULL)!=SQLITE_OK);
  sql(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "=0");
  assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
  assert(!wena_sqlite_open(path,bundles[old-1].bytes,bundles[old-1].length,bundles[old-1].hash,&db));
  assert(wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].length,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash,&db));
  assert(number(db,"SELECT " WENA_SETTING_COLUMN " FROM " WENA_SETTING_TABLE "")==0);
  assert(sqlite3_close(db)==SQLITE_OK);
 }
 for(mode=1;mode<=3;++mode){
  sprintf(path,"%s/rollback-%d.sqlite",argv[WENA_SETTING_SCHEMA_VERSION+1],mode);
  assert(wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-2].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-2].length,bundles[WENA_SETTING_SCHEMA_VERSION-2].hash,&db));
  failure=mode;assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);
  assert(!wena_sqlite_upgrade(db,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash));
  assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);failure=0;
  assert(sqlite3_get_autocommit(db)&&wena_sqlite_schema_version(db)==WENA_SETTING_SCHEMA_VERSION-1);
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name='" WENA_SETTING_TABLE "'"));
  assert(wena_sqlite_upgrade(db,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash)&&wena_sqlite_schema_version(db)==WENA_SETTING_SCHEMA_VERSION);
  sql(db,"DROP TABLE " WENA_SETTING_TABLE "");assert(!wena_sqlite_schema_validate(db,NULL));
  assert(sqlite3_close(db)==SQLITE_OK);
  assert(!wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].length,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash,&db));
 }
 for(i=0;i<WENA_SETTING_SCHEMA_VERSION;++i)free(bundles[i].bytes);
 printf("Schema v%d upgrades, immutable history, defaults, constraints, rollback and reopen passed\n",WENA_SETTING_SCHEMA_VERSION);return 0;
}
