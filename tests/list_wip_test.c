#include "../server/mutations/list_wip.h"
#include "../server/sqlite_persistence.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static int change(WenaSqlitePersistence *store,const char *id,unsigned long version,unsigned long request,const char *form)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));
 c.operation=WENA_DOMAIN_EDIT_LIST_WIP;c.request_version=request;
 strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
 sprintf(c.form_body,"listId=%s&expectedVersion=%lu&%s",id,version,form);
 c.form_body_length=strlen(c.form_body);return wena_sqlite_persistence_apply(store,&c,&r);
}
static void read_settings(sqlite3 *db,unsigned long version,size_t value,int enabled,int soft)
{
 WenaWipLimit limit;WenaSqliteBoardSnapshot *snapshot;size_t i;int found;
 assert(wena_sqlite_list_wip_read(db,"b","l",version,&limit));assert(limit.value==value&&limit.enabled==enabled&&limit.soft==soft);
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));assert(snapshot&&wena_sqlite_board_load(db,"b",snapshot));found=0;
 for(i=0;i<snapshot->list_count;++i){
  limit=snapshot->lists[i].wip_limit;
  if(!strcmp(snapshot->lists[i].id,"l")){assert(limit.value==value&&limit.enabled==enabled&&limit.soft==soft);found=1;}
  else assert(limit.value==1&&!limit.enabled&&!limit.soft);
 }
 assert(found);free(snapshot);
}
static void bad_snapshot(sqlite3 *db,const char *query,const char *undo)
{
 WenaSqliteBoardSnapshot *snapshot,*before;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));assert(snapshot&&before);
 assert(wena_sqlite_board_load(db,"b",snapshot));memcpy(before,snapshot,sizeof(*snapshot));
 sql(db,"PRAGMA foreign_keys=OFF;PRAGMA ignore_check_constraints=ON");sql(db,query);
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 sql(db,undo);sql(db,"PRAGMA foreign_keys=ON;PRAGMA ignore_check_constraints=OFF");
 assert(wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));free(snapshot);free(before);
}
typedef struct ConcurrentWrite {sqlite3 *writer;int fired;} ConcurrentWrite;
static int concurrent_write(unsigned int event,void *context,void *statement,void *text)
{
 ConcurrentWrite *write;const char *query;(void)text;
 write=(ConcurrentWrite*)context;query=sqlite3_sql((sqlite3_stmt*)statement);
 if(event==SQLITE_TRACE_STMT&&!write->fired&&query&&strstr(query,"FROM list_wip_limits c")){
  write->fired=1;sql(write->writer,"BEGIN IMMEDIATE;UPDATE boards SET title='After' WHERE id='b';UPDATE list_wip_limits SET value=103;COMMIT");
 }
 return 0;
}
static void concurrent_snapshot(sqlite3 *db,const char *path)
{
 ConcurrentWrite write;WenaSqliteBoardSnapshot *snapshot;size_t i;
 sql(db,"PRAGMA journal_mode=WAL");assert(sqlite3_open(path,&write.writer)==SQLITE_OK);write.fired=0;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));assert(snapshot);
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,concurrent_write,&write)==SQLITE_OK);
 assert(wena_sqlite_board_load(db,"b",snapshot)&&write.fired);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(!strcmp(snapshot->board.title,"Board"));
 for(i=0;i<snapshot->list_count;++i)if(!strcmp(snapshot->lists[i].id,"l"))assert(snapshot->lists[i].wip_limit.value==102);
 assert(wena_sqlite_board_load(db,"b",snapshot)&&!strcmp(snapshot->board.title,"After"));
 for(i=0;i<snapshot->list_count;++i)if(!strcmp(snapshot->lists[i].id,"l"))assert(snapshot->lists[i].wip_limit.value==103);
 sql(write.writer,"BEGIN IMMEDIATE;UPDATE boards SET title='Board' WHERE id='b';UPDATE list_wip_limits SET value=102;COMMIT");
 assert(sqlite3_close(write.writer)==SQLITE_OK);free(snapshot);
}
int main(int argc,char **argv)
{
 FILE *f;unsigned char *migration;long length;char hash[65],path[1024],q[512];
 sqlite3 *db;WenaSqlitePersistence store;WenaWipLimit limit,before;size_t count,i;
 const char *invalid[]={"action=value&value=0","action=value&value=100","action=value&value=-1","action=value&value=1.5","action=value&value=NaN","action=value&value=1%00","action=value&value=2&value=3","action=unknown","action=enabled&action=soft","action=value","action=soft&listId=other"};
 const char *triggers[]={
 "CREATE TRIGGER failure BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END",
 "CREATE TRIGGER failure BEFORE INSERT ON list_wip_limits BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER failure AFTER INSERT ON list_wip_limits BEGIN UPDATE list_wip_limits SET soft=1;END",
 "CREATE TRIGGER failure BEFORE UPDATE ON lists BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER failure AFTER INSERT ON list_wip_limits BEGIN UPDATE cards SET archived=1;END"};
 assert(argc==3);f=fopen(argv[1],"rb");assert(f&&!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);rewind(f);
 migration=(unsigned char*)malloc((size_t)length);assert(migration&&fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
 wena_sha256_hex(migration,(size_t)length,hash);sprintf(path,"%s/wip.sqlite",argv[2]);
 assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1),('other','Other',1);"
 "INSERT INTO lists VALUES('l','b','List',0,1),('empty','b','Empty',1,1),('foreign','other','Foreign',0,1);"
 "INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('t','b','Lane 2',1,1);"
 "INSERT INTO cards VALUES('a','b','s','l','A',0,0,7),('b','b','t','l','B',0,0,8),('c','b','s','l','Archived',1,1,9)");
 wena_sqlite_persistence_init(&store,db);read_settings(db,1,1,0,0);
 count=99;assert(wena_sqlite_list_wip_count(db,"b","l",&count)&&count==2);
 assert(wena_sqlite_list_wip_count(db,"b","empty",&count)&&count==0);
 count=99;assert(!wena_sqlite_list_wip_count(db,"other","l",&count)&&count==99);
 memset(&limit,42,sizeof(limit));memcpy(&before,&limit,sizeof(limit));
 assert(!wena_sqlite_list_wip_read(db,"b","l",2,&limit));
 assert(!wena_sqlite_list_wip_read(db,"other","l",1,&limit));
 assert(!wena_sqlite_list_wip_read(db,"b","missing",1,&limit));
 assert(!memcmp(&limit,&before,sizeof(limit)));
 assert(change(&store,"empty",1,1,"action=value&value=1"));assert(!number(db,"SELECT count(*) FROM idempotency_keys"));
 assert(!change(&store,"l",1,1,"action=value&value=1"));
 for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)assert(!change(&store,"l",1,1,invalid[i]));
 assert(!change(&store,"foreign",1,1,"action=enabled"));assert(!change(&store,"missing",1,1,"action=enabled"));
 assert(!change(&store,"l",2,1,"action=enabled"));assert(!change(&store,"l",0,1,"action=enabled"));
 sql(db,"DELETE FROM actors");assert(!change(&store,"l",1,1,"action=enabled"));sql(db,"INSERT INTO actors VALUES('u','User',1)");
 sql(db,"PRAGMA query_only=ON");assert(!change(&store,"l",1,1,"action=enabled"));sql(db,"PRAGMA query_only=OFF");
 sql(db,"INSERT INTO list_archive_state VALUES('l','b',1,123)");assert(!change(&store,"l",1,1,"action=enabled"));sql(db,"UPDATE list_archive_state SET archived=0");
 for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){sql(db,triggers[i]);assert(!change(&store,"l",1,1,"action=enabled"));sql(db,"DROP TRIGGER failure");
  read_settings(db,1,1,0,0);assert(!number(db,"SELECT count(*) FROM list_wip_limits"));assert(!number(db,"SELECT count(*) FROM idempotency_keys"));
  assert(wena_sqlite_list_wip_count(db,"b","l",&count)&&count==2);
 }
 assert(change(&store,"l",1,1,"action=enabled"));read_settings(db,2,2,1,0);
 assert(!change(&store,"l",2,1,"action=soft"));
 assert(change(&store,"l",2,2,"action=soft"));read_settings(db,3,2,1,1);
 assert(change(&store,"l",3,3,"action=value&value=1"));read_settings(db,4,1,1,1);
 assert(change(&store,"l",4,4,"action=soft"));read_settings(db,5,2,1,0);
 assert(change(&store,"l",5,5,"action=value&value=2"));assert(number(db,"SELECT count(*) FROM idempotency_keys")==4);
 assert(change(&store,"l",5,5,"action=enabled"));read_settings(db,6,2,0,0);
 assert(change(&store,"l",6,6,"action=value&value=99"));read_settings(db,7,99,0,0);
 assert(number(db,"SELECT sum(version) FROM cards")==24&&number(db,"SELECT version FROM boards WHERE id='b'")==1);
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE list_wip_limits SET enabled=4294967296;PRAGMA ignore_check_constraints=OFF");
 assert(!change(&store,"l",7,7,"action=enabled"));assert(!wena_sqlite_list_wip_read(db,"b","l",7,&limit));assert(!memcmp(&limit,&before,sizeof(limit)));
 sql(db,"UPDATE list_wip_limits SET enabled=0;PRAGMA ignore_check_constraints=ON;UPDATE cards SET archived=2 WHERE id='a';PRAGMA ignore_check_constraints=OFF");
 count=99;assert(!wena_sqlite_list_wip_count(db,"b","l",&count)&&count==99);assert(!change(&store,"l",7,7,"action=enabled"));sql(db,"UPDATE cards SET archived=0 WHERE id='a'");
 sql(db,"PRAGMA foreign_keys=OFF;UPDATE list_wip_limits SET board_id='other';PRAGMA foreign_keys=ON");
 assert(!change(&store,"l",7,7,"action=enabled"));sql(db,"UPDATE list_wip_limits SET board_id='b'");
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));read_settings(db,7,99,0,0);
 wena_sqlite_persistence_init(&store,db);
 /* A full list may already contain more than the input control's maximum. */
 sql(db,"BEGIN");for(i=0;i<100;++i){sprintf(q,"INSERT INTO cards VALUES('extra%lu','b','s','l','Card',%lu,0,1)",(unsigned long)i,(unsigned long)i+2);sql(db,q);}sql(db,"COMMIT");
 assert(change(&store,"l",7,7,"action=enabled"));read_settings(db,8,102,1,0);
 bad_snapshot(db,"UPDATE list_wip_limits SET value=0","UPDATE list_wip_limits SET value=102");
 bad_snapshot(db,"UPDATE list_wip_limits SET value=2147483648","UPDATE list_wip_limits SET value=102");
 bad_snapshot(db,"UPDATE list_wip_limits SET value=0.5","UPDATE list_wip_limits SET value=102");
 bad_snapshot(db,"UPDATE list_wip_limits SET value=x'31'","UPDATE list_wip_limits SET value=102");
 bad_snapshot(db,"UPDATE list_wip_limits SET enabled=4294967296","UPDATE list_wip_limits SET enabled=1");
 bad_snapshot(db,"UPDATE list_wip_limits SET soft=-1","UPDATE list_wip_limits SET soft=0");
 bad_snapshot(db,"UPDATE list_wip_limits SET board_id='other'","UPDATE list_wip_limits SET board_id='b'");
 bad_snapshot(db,"UPDATE list_wip_limits SET board_id=x'62'","UPDATE list_wip_limits SET board_id='b'");
 bad_snapshot(db,"INSERT INTO list_wip_limits VALUES('orphan','b',1,0,0)","DELETE FROM list_wip_limits WHERE list_id='orphan'");
 bad_snapshot(db,"INSERT INTO list_wip_limits VALUES('foreign','b',1,0,0)","DELETE FROM list_wip_limits WHERE list_id='foreign'");
 sql(db,"UPDATE list_archive_state SET archived=1");read_settings(db,8,102,1,0);sql(db,"UPDATE list_archive_state SET archived=0");
 bad_snapshot(db,"ALTER TABLE list_wip_limits RENAME TO saved_wip","ALTER TABLE saved_wip RENAME TO list_wip_limits");
 bad_snapshot(db,"ALTER TABLE list_wip_limits RENAME TO saved_wip;CREATE VIEW list_wip_limits AS SELECT * FROM saved_wip","DROP VIEW list_wip_limits;ALTER TABLE saved_wip RENAME TO list_wip_limits");
 concurrent_snapshot(db,path);
 sql(db,"DROP TABLE list_wip_limits");assert(!change(&store,"l",8,8,"action=enabled"));
 sql(db,"CREATE VIEW list_wip_limits AS SELECT 'l' AS list_id,'b' AS board_id,1 AS value,0 AS enabled,0 AS soft");
 assert(!change(&store,"l",8,8,"action=enabled"));assert(!wena_sqlite_list_wip_read(db,"b","l",8,&limit));
 assert(sqlite3_close(db)==SQLITE_OK);free(migration);
 puts("List WIP persistence: shared rules, counts, scope, no-op, replay, corruption, rollback and reopen passed");return 0;
}
