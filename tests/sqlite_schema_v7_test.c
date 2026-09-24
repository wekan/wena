#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Bundle {unsigned char *bytes;size_t length;char hash[65];} Bundle;
static int failure;
static int authorize(void *data,int action,const char *first,const char *second,
 const char *database,const char *trigger)
{
 (void)data;(void)second;(void)database;(void)trigger;
 if((failure==1&&action==SQLITE_CREATE_TABLE&&first&&!strcmp(first,"board_minicard_settings"))||
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
 Bundle bundles[7];sqlite3 *db;char path[1024];int old,mode;size_t i;
 assert(argc==9&&strlen(argv[8])<900);
 for(i=0;i<7;++i){read_bundle(argv[i+1],&bundles[i]);assert(wena_sqlite_migration_target(bundles[i].hash)==(int)i+1);}
 for(old=1;old<=6;++old){
  assert(!memcmp(bundles[old-1].bytes,bundles[6].bytes,bundles[old-1].length));
  sprintf(path,"%s/from-%d.sqlite",argv[8],old);
  assert(wena_sqlite_open(path,bundles[old-1].bytes,bundles[old-1].length,bundles[old-1].hash,&db));
  sql(db,"INSERT INTO boards VALUES('b','Keep title',42)");
  if(old==6)sql(db,"INSERT INTO board_settings VALUES('b',1)");
  assert(sqlite3_close(db)==SQLITE_OK);
  assert(wena_sqlite_open(path,bundles[6].bytes,bundles[6].length,bundles[6].hash,&db));
  assert(wena_sqlite_schema_version(db)==7&&number(db,"SELECT version FROM boards WHERE id='b'")==42);
  assert(number(db,"SELECT count(*) FROM board_minicard_settings")==0);
  if(old==6)assert(number(db,"SELECT show_checklist_count FROM board_settings WHERE board_id='b'")==1);
  sql(db,"INSERT INTO board_minicard_settings(board_id) VALUES('b')");
  assert(number(db,"SELECT show_checklists FROM board_minicard_settings")==1);
  assert(sqlite3_exec(db,"UPDATE board_minicard_settings SET show_checklists=2",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE board_minicard_settings SET show_checklists=NULL",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE board_minicard_settings SET show_checklists='bad'",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"INSERT INTO board_minicard_settings VALUES('missing',0)",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"DELETE FROM boards WHERE id='b'",NULL,NULL,NULL)!=SQLITE_OK);
  sql(db,"UPDATE board_minicard_settings SET show_checklists=0");
  assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
  assert(!wena_sqlite_open(path,bundles[old-1].bytes,bundles[old-1].length,bundles[old-1].hash,&db));
  assert(wena_sqlite_open(path,bundles[6].bytes,bundles[6].length,bundles[6].hash,&db));
  assert(number(db,"SELECT show_checklists FROM board_minicard_settings")==0);
  assert(sqlite3_close(db)==SQLITE_OK);
 }
 for(mode=1;mode<=3;++mode){
  sprintf(path,"%s/rollback-%d.sqlite",argv[8],mode);
  assert(wena_sqlite_open(path,bundles[5].bytes,bundles[5].length,bundles[5].hash,&db));
  failure=mode;assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);
  assert(!wena_sqlite_upgrade(db,bundles[6].hash));
  assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);failure=0;
  assert(sqlite3_get_autocommit(db)&&wena_sqlite_schema_version(db)==6);
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name='board_minicard_settings'"));
  assert(wena_sqlite_upgrade(db,bundles[6].hash)&&wena_sqlite_schema_version(db)==7);
  sql(db,"DROP TABLE board_minicard_settings");assert(!wena_sqlite_schema_validate(db,NULL));
  assert(sqlite3_close(db)==SQLITE_OK);
  assert(!wena_sqlite_open(path,bundles[6].bytes,bundles[6].length,bundles[6].hash,&db));
 }
 for(i=0;i<7;++i)free(bundles[i].bytes);
 puts("Schema v7 upgrades from v1-v6, immutable history, defaults, constraints, rollback and reopen passed");return 0;
}
