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
#ifdef WENA_SETTING_PEOPLE
#define WENA_SETTING_FAILURE_MODES 7
#elif defined(WENA_SETTING_COLORS)
#include "../models/color.h"
#define WENA_SETTING_FAILURE_MODES 6
#elif defined(WENA_SETTING_SWIMLANE_ARCHIVE)
#define WENA_SETTING_FAILURE_MODES 6
#elif defined(WENA_SETTING_LIST_ARCHIVE) || defined(WENA_SETTING_WIP)
#define WENA_SETTING_DEFAULT 0
#define WENA_SETTING_FAILURE_MODES 4
#else
#define WENA_SETTING_DEFAULT 1
#define WENA_SETTING_FAILURE_MODES 3
#endif
static int failure;
static int authorize(void *data,int action,const char *first,const char *second,
 const char *database,const char *trigger)
{
#ifdef WENA_SETTING_PEOPLE
 if((failure==4&&action==SQLITE_CREATE_TABLE&&first&&!strcmp(first,"card_people"))||
    (failure==5&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"board_members_actor_idx"))||
    (failure==6&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"card_people_actor_idx"))||
    (failure==7&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"card_people_card_order_idx")))return SQLITE_DENY;
#endif
#ifdef WENA_SETTING_SWIMLANE_ARCHIVE
 if((failure==4&&action==SQLITE_CREATE_TABLE&&first&&!strcmp(first,"card_archive_state"))||
    (failure==5&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"swimlane_archive_board_idx"))||
    (failure==6&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"card_archive_board_idx")))return SQLITE_DENY;
#endif
#ifdef WENA_SETTING_WIP
 if(failure==4&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"list_wip_limits_board_idx"))return SQLITE_DENY;
#endif
#ifdef WENA_SETTING_COLORS
 if((failure==4&&action==SQLITE_CREATE_TABLE&&first&&!strcmp(first,"swimlane_colors"))||
    (failure==5&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"list_colors_board_idx"))||
    (failure==6&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"swimlane_colors_board_idx")))return SQLITE_DENY;
#endif
 (void)data;(void)second;(void)database;(void)trigger;
 if((failure==1&&action==SQLITE_CREATE_TABLE&&first&&!strcmp(first,WENA_SETTING_TABLE))||
    (failure==4&&action==SQLITE_CREATE_INDEX&&first&&!strcmp(first,"list_archive_board_idx"))||
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
#ifdef WENA_SETTING_COLORS
static void color_table(sqlite3 *db,const char *table,const char *key,const char *parent,const char *id)
{
 char q[512];const WenaColorContract *colors;size_t count,i;
 const char *bad[]={"NULL","'RED'","'#abc'","'#1234567'","'#12gg34'","'belize'","x'726564'","'red'||char(0)","42"};
 sprintf(q,"INSERT INTO %s(%s,board_id) VALUES('%s','b')",table,key,id);sql(db,q);
 sprintf(q,"SELECT count(*) FROM %s WHERE color=''",table);assert(number(db,q)==1);
 for(i=0;i<sizeof(bad)/sizeof(bad[0]);++i){sprintf(q,"UPDATE %s SET color=%s",table,bad[i]);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);}
 colors=wena_colors(&count);assert(count==25);
 for(i=0;i<count;++i){sprintf(q,"UPDATE %s SET color='%s'",table,colors[i].name);sql(db,q);}
 sprintf(q,"UPDATE %s SET board_id='other'",table);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 sprintf(q,"INSERT INTO %s(%s,board_id) VALUES('missing','b')",table,key);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 sprintf(q,"DELETE FROM %s WHERE id='%s'",parent,id);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 sprintf(q,"UPDATE %s SET color='#aBcD01'",table);sql(db,q);
 sprintf(q,"SELECT count(*) FROM %s WHERE color='#aBcD01'",table);assert(number(db,q)==1);
}
#endif
#ifdef WENA_SETTING_WIP
static void wip_table(sqlite3 *db)
{
 const char *columns[]={"value","enabled","soft"};
 const char *bad[]={"NULL","'bad'","x'31'","0.5","-1","1e100"};
 char q[256];size_t i,j;
 sql(db,"INSERT INTO list_wip_limits(list_id,board_id) VALUES('l','b')");
 assert(number(db,"SELECT count(*) FROM list_wip_limits WHERE value=1 AND enabled=0 AND soft=0")==1);
 for(i=0;i<3;++i)for(j=0;j<sizeof(bad)/sizeof(bad[0]);++j){
  sprintf(q,"UPDATE list_wip_limits SET %s=%s",columns[i],bad[j]);
  assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 }
 assert(sqlite3_exec(db,"UPDATE list_wip_limits SET value=0",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"UPDATE list_wip_limits SET value=2147483648",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"UPDATE list_wip_limits SET enabled=2",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"UPDATE list_wip_limits SET soft=2",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"UPDATE list_wip_limits SET board_id='other'",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"INSERT INTO list_wip_limits(list_id,board_id) VALUES('missing','b')",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"INSERT INTO list_wip_limits(list_id,board_id) VALUES('l','b')",NULL,NULL,NULL)!=SQLITE_OK);
 assert(sqlite3_exec(db,"DELETE FROM lists WHERE id='l'",NULL,NULL,NULL)!=SQLITE_OK);
 /* Defaults and both endpoints; automatic count adjustment can exceed 99. */
 sql(db,"UPDATE list_wip_limits SET value=99,enabled=1,soft=1");
 sql(db,"UPDATE list_wip_limits SET value=100");
 sql(db,"UPDATE list_wip_limits SET value=2147483647");
 assert(number(db,"SELECT value FROM list_wip_limits")==2147483647);
 sql(db,"UPDATE list_wip_limits SET enabled=0,soft=0");
 sql(db,"UPDATE list_wip_limits SET enabled=1,soft=1");
 assert(number(db,"SELECT version FROM lists WHERE id='l'")==7);
 assert(number(db,"SELECT version FROM cards WHERE id='c'")==3);
 assert(number(db,"SELECT archived FROM cards WHERE id='c'")==1);
}
#endif
#ifdef WENA_SETTING_SWIMLANE_ARCHIVE
static void archive_table(sqlite3 *db,const char *table,const char *key,const char *id,const char *parent,int lane)
{
 char q[512];size_t i;const char *bad[]={"NULL","-1","0.5","'bad'","x'31'","1e100"};
 sprintf(q,"INSERT INTO %s(%s,board_id) VALUES('%s','b')",table,key,id);sql(db,q);
 sprintf(q,"SELECT archived_at FROM %s",table);assert(number(db,q)==0);
 for(i=0;i<sizeof(bad)/sizeof(bad[0]);++i){sprintf(q,"UPDATE %s SET archived_at=%s",table,bad[i]);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);}
 sprintf(q,"UPDATE %s SET archived_at=1234567890123",table);sql(db,q);
 sprintf(q,"UPDATE %s SET board_id='other'",table);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 sprintf(q,"INSERT INTO %s(%s,board_id) VALUES('missing','b')",table,key);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 sprintf(q,"INSERT INTO %s(%s,board_id) VALUES('%s','b')",table,key,id);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 sprintf(q,"DELETE FROM %s WHERE id='%s'",parent,id);assert(sqlite3_exec(db,q,NULL,NULL,NULL)!=SQLITE_OK);
 if(lane){
  assert(number(db,"SELECT archived FROM swimlane_archive_state")==0);
  assert(sqlite3_exec(db,"UPDATE swimlane_archive_state SET archived=2",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE swimlane_archive_state SET archived=0.5",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE swimlane_archive_state SET archived=x'31'",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE swimlane_archive_state SET archived=NULL",NULL,NULL,NULL)!=SQLITE_OK);
  sql(db,"UPDATE swimlane_archive_state SET archived=1");
 }
}
#endif
#ifdef WENA_SETTING_PEOPLE
#include "sqlite_schema_people.inc"
#endif
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
#if defined(WENA_SETTING_LIST_ARCHIVE) || defined(WENA_SETTING_COLORS) || defined(WENA_SETTING_WIP) || defined(WENA_SETTING_SWIMLANE_ARCHIVE) || defined(WENA_SETTING_PEOPLE)
  sql(db,"INSERT INTO boards VALUES('other','Other',1);INSERT INTO lists VALUES('l','b','Keep list',0,7);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Keep card',0,1,3)");
#endif
#if defined(WENA_SETTING_COLORS) || defined(WENA_SETTING_WIP) || defined(WENA_SETTING_SWIMLANE_ARCHIVE) || defined(WENA_SETTING_PEOPLE)
  if(old>=10)sql(db,"INSERT INTO list_archive_state VALUES('l','b',1,123456)");
#endif
#if defined(WENA_SETTING_WIP) || defined(WENA_SETTING_SWIMLANE_ARCHIVE) || defined(WENA_SETTING_PEOPLE)
  if(old>=11)sql(db,"INSERT INTO list_colors VALUES('l','b','red');INSERT INTO swimlane_colors VALUES('s','b','#123AbC')");
#endif
#if defined(WENA_SETTING_SWIMLANE_ARCHIVE) || defined(WENA_SETTING_PEOPLE)
  if(old>=12)sql(db,"INSERT INTO list_wip_limits VALUES('l','b',5,1,1)");
#endif
#ifdef WENA_SETTING_PEOPLE
  if(old>=13)sql(db,"INSERT INTO card_archive_state VALUES('c','b',456);INSERT INTO swimlane_archive_state VALUES('s','b',1,789)");
#endif
  if(old>=6)sql(db,"INSERT INTO board_settings VALUES('b',1)");
  assert(sqlite3_close(db)==SQLITE_OK);
  assert(wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].length,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash,&db));
  assert(wena_sqlite_schema_version(db)==WENA_SETTING_SCHEMA_VERSION&&number(db,"SELECT version FROM boards WHERE id='b'")==42);
  assert(number(db,"SELECT count(*) FROM " WENA_SETTING_TABLE "")==0);
  if(old>=6)assert(number(db,"SELECT show_checklist_count FROM board_settings WHERE board_id='b'")==1);
#ifdef WENA_SETTING_PEOPLE
  if(old>=10)assert(number(db,"SELECT archived_at FROM list_archive_state")==123456);
  if(old>=11){assert(number(db,"SELECT count(*) FROM list_colors WHERE color='red'")==1);assert(number(db,"SELECT count(*) FROM swimlane_colors WHERE color='#123AbC'")==1);}
  if(old>=12)assert(number(db,"SELECT count(*) FROM list_wip_limits WHERE value=5 AND enabled=1 AND soft=1")==1);
  if(old>=13){assert(number(db,"SELECT archived_at FROM card_archive_state")==456);assert(number(db,"SELECT archived_at FROM swimlane_archive_state")==789);}
  people_tables(db);
#elif defined(WENA_SETTING_SWIMLANE_ARCHIVE)
  if(old>=10)assert(number(db,"SELECT archived_at FROM list_archive_state WHERE archived=1")==123456);
  if(old>=11)assert(number(db,"SELECT count(*) FROM list_colors WHERE color='red'")==1);
  if(old>=12)assert(number(db,"SELECT count(*) FROM list_wip_limits WHERE value=5 AND enabled=1 AND soft=1")==1);
  archive_table(db,"swimlane_archive_state","swimlane_id","s","swimlanes",1);
  archive_table(db,"card_archive_state","card_id","c","cards",0);
  assert(number(db,"SELECT version FROM lists WHERE id='l'")==7);
  assert(number(db,"SELECT version FROM swimlanes WHERE id='s'")==1);
  assert(number(db,"SELECT version FROM cards WHERE id='c'")==3);
  assert(number(db,"SELECT archived FROM cards WHERE id='c'")==1);
#elif defined(WENA_SETTING_WIP)
  if(old>=10)assert(number(db,"SELECT archived_at FROM list_archive_state WHERE archived=1")==123456);
  if(old>=11){
   assert(number(db,"SELECT count(*) FROM list_colors WHERE color='red'")==1);
   assert(number(db,"SELECT count(*) FROM swimlane_colors WHERE color='#123AbC'")==1);
  }
  wip_table(db);
#elif defined(WENA_SETTING_COLORS)
  if(old>=10)assert(number(db,"SELECT archived_at FROM list_archive_state WHERE archived=1")==123456);
  color_table(db,"list_colors","list_id","lists","l");
  color_table(db,"swimlane_colors","swimlane_id","swimlanes","s");
  assert(number(db,"SELECT version FROM lists WHERE id='l'")==7);
  assert(number(db,"SELECT version FROM swimlanes WHERE id='s'")==1);
  assert(number(db,"SELECT archived FROM cards WHERE id='c'")==1);
#else
#ifdef WENA_SETTING_LIST_ARCHIVE
  sql(db,"INSERT INTO list_archive_state(list_id,board_id) VALUES('l','b')");
  assert(number(db,"SELECT archived_at FROM list_archive_state")==0);
  assert(number(db,"SELECT version FROM lists WHERE id='l'")==7);
  assert(number(db,"SELECT archived FROM cards WHERE id='c'")==1);
  assert(sqlite3_exec(db,"UPDATE list_archive_state SET archived_at=-1",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE list_archive_state SET archived_at=0.5",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE list_archive_state SET archived_at='bad'",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE list_archive_state SET board_id='other'",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"DELETE FROM lists WHERE id='l'",NULL,NULL,NULL)!=SQLITE_OK);
  sql(db,"UPDATE list_archive_state SET archived_at=123");
#else
  sql(db,"INSERT INTO " WENA_SETTING_TABLE "(board_id) VALUES('b')");
#endif
  assert(number(db,"SELECT " WENA_SETTING_COLUMN " FROM " WENA_SETTING_TABLE "")==WENA_SETTING_DEFAULT);
  assert(sqlite3_exec(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "=2",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "=NULL",NULL,NULL,NULL)!=SQLITE_OK);
  assert(sqlite3_exec(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "='bad'",NULL,NULL,NULL)!=SQLITE_OK);
#ifdef WENA_SETTING_LIST_ARCHIVE
  assert(sqlite3_exec(db,"INSERT INTO list_archive_state(list_id,board_id) VALUES('missing','b')",NULL,NULL,NULL)!=SQLITE_OK);
#else
  assert(sqlite3_exec(db,"INSERT INTO " WENA_SETTING_TABLE " VALUES('missing',0)",NULL,NULL,NULL)!=SQLITE_OK);
#endif
  assert(sqlite3_exec(db,"DELETE FROM boards WHERE id='b'",NULL,NULL,NULL)!=SQLITE_OK);
  sql(db,"UPDATE " WENA_SETTING_TABLE " SET " WENA_SETTING_COLUMN "=0");
#endif
  assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
  assert(!wena_sqlite_open(path,bundles[old-1].bytes,bundles[old-1].length,bundles[old-1].hash,&db));
  assert(wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].length,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash,&db));
#ifdef WENA_SETTING_PEOPLE
  assert(number(db,"SELECT count(*) FROM card_people WHERE card_id='c' AND actor_id='a'")==2);
  assert(number(db,"SELECT active FROM board_members WHERE actor_id='a'")==0);
#elif defined(WENA_SETTING_SWIMLANE_ARCHIVE)
  assert(number(db,"SELECT count(*) FROM swimlane_archive_state WHERE archived=1 AND archived_at=1234567890123")==1);
  assert(number(db,"SELECT count(*) FROM card_archive_state WHERE archived_at=1234567890123")==1);
#elif defined(WENA_SETTING_WIP)
  assert(number(db,"SELECT count(*) FROM list_wip_limits WHERE value=2147483647 AND enabled=1 AND soft=1")==1);
#elif defined(WENA_SETTING_COLORS)
  assert(number(db,"SELECT count(*) FROM list_colors WHERE color='#aBcD01'")==1);
  assert(number(db,"SELECT count(*) FROM swimlane_colors WHERE color='#aBcD01'")==1);
#else
  assert(number(db,"SELECT " WENA_SETTING_COLUMN " FROM " WENA_SETTING_TABLE "")==0);
#endif
  assert(sqlite3_close(db)==SQLITE_OK);
 }
 for(mode=1;mode<=WENA_SETTING_FAILURE_MODES;++mode){
  sprintf(path,"%s/rollback-%d.sqlite",argv[WENA_SETTING_SCHEMA_VERSION+1],mode);
  assert(wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-2].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-2].length,bundles[WENA_SETTING_SCHEMA_VERSION-2].hash,&db));
  failure=mode;assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);
  assert(!wena_sqlite_upgrade(db,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash));
  assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);failure=0;
  assert(sqlite3_get_autocommit(db)&&wena_sqlite_schema_version(db)==WENA_SETTING_SCHEMA_VERSION-1);
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name='" WENA_SETTING_TABLE "'"));
#ifdef WENA_SETTING_PEOPLE
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name IN ('card_people','board_members_actor_idx','card_people_actor_idx','card_people_card_order_idx')"));
#endif
#ifdef WENA_SETTING_SWIMLANE_ARCHIVE
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name IN ('card_archive_state','swimlane_archive_board_idx','card_archive_board_idx')"));
#endif
#ifdef WENA_SETTING_WIP
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name='list_wip_limits_board_idx'"));
#endif
#ifdef WENA_SETTING_COLORS
  assert(!number(db,"SELECT count(*) FROM sqlite_master WHERE name IN ('swimlane_colors','list_colors_board_idx','swimlane_colors_board_idx')"));
#endif
  assert(wena_sqlite_upgrade(db,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash)&&wena_sqlite_schema_version(db)==WENA_SETTING_SCHEMA_VERSION);
  sql(db,"DROP TABLE " WENA_SETTING_TABLE "");assert(!wena_sqlite_schema_validate(db,NULL));
  assert(sqlite3_close(db)==SQLITE_OK);
  assert(!wena_sqlite_open(path,bundles[WENA_SETTING_SCHEMA_VERSION-1].bytes,bundles[WENA_SETTING_SCHEMA_VERSION-1].length,bundles[WENA_SETTING_SCHEMA_VERSION-1].hash,&db));
 }
 for(i=0;i<WENA_SETTING_SCHEMA_VERSION;++i)free(bundles[i].bytes);
 printf("Schema v%d upgrades, immutable history, defaults, constraints, rollback and reopen passed\n",WENA_SETTING_SCHEMA_VERSION);return 0;
}
