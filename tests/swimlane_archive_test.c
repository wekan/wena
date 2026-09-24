#include "../server/list_state.h"
#include "../client/features/hierarchy_mutation.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_persistence.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){int rc;rc=sqlite3_exec(db,q,NULL,NULL,NULL);if(rc!=SQLITE_OK)fprintf(stderr,"%s: %s\n",q,sqlite3_errmsg(db));assert(rc==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static int change(WenaSqlitePersistence *store,const char *lane,unsigned long version,unsigned long request,int archived)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));c.operation=archived?WENA_DOMAIN_ARCHIVE_SWIMLANE:WENA_DOMAIN_RESTORE_SWIMLANE;
 c.request_version=request;strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
 sprintf(c.form_body,"swimlaneId=%s&expectedVersion=%lu",lane,version);c.form_body_length=strlen(c.form_body);return wena_sqlite_persistence_apply(store,&c,&r);
}
static int guarded(WenaSqlitePersistence *store,WenaDomainOperation operation,const char *form)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));c.operation=operation;c.request_version=100;
 strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");strcpy(c.form_body,form);c.form_body_length=strlen(form);
 return wena_sqlite_persistence_apply(store,&c,&r);
}
static void destination_guards(sqlite3 *db,WenaSqlitePersistence *store)
{
 sqlite3_int64 keys,versions;char q[256];keys=number(db,"SELECT count(*) FROM idempotency_keys");versions=number(db,"SELECT sum(version) FROM cards");
 assert(!guarded(store,WENA_DOMAIN_CREATE_CARD,"title=Hidden&targetListId=l&targetSwimlaneId=s"));
 assert(!guarded(store,WENA_DOMAIN_RESTORE_CARD,"cardId=a&expectedVersion=2"));
 assert(!guarded(store,WENA_DOMAIN_MOVE_CARD,"cardId=other&expectedVersion=1&targetListId=l&targetSwimlaneId=s"));
 assert(!guarded(store,WENA_DOMAIN_MOVE_SWIMLANE,"swimlaneId=s&expectedVersion=2&targetPosition=1"));
 assert(!guarded(store,WENA_DOMAIN_SET_SWIMLANE_COLOR,"swimlaneId=s&expectedVersion=2&color=red"));
 /* Even a legacy active card under a hidden lane cannot be moved out. */
 sql(db,"UPDATE cards SET archived=0 WHERE id='a'");
 assert(!guarded(store,WENA_DOMAIN_MOVE_CARD,"cardId=a&expectedVersion=2&targetListId=l&targetSwimlaneId=t"));sql(db,"UPDATE cards SET archived=1 WHERE id='a'");
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys&&number(db,"SELECT sum(version) FROM cards")==versions);
 assert(number(db,"SELECT count(*) FROM cards")==4&&number(db,"SELECT version FROM swimlanes WHERE id='s'")==2);
 assert(guarded(store,WENA_DOMAIN_CREATE_CARD,"title=Automatic"));
 sprintf(q,"SELECT count(*) FROM cards WHERE id='%s' AND swimlane_id='t'",store->created_card_id);assert(number(db,q)==1);
 sprintf(q,"DELETE FROM cards WHERE id='%s'",store->created_card_id);sql(db,q);
 sql(db,"INSERT INTO swimlane_archive_state VALUES('t','b',1,1),('empty','b',1,1)");
 /* Use a fresh route identity for the all-hidden rejection. */
 sql(db,"DELETE FROM idempotency_keys WHERE operation='create-card' AND request_version=100");
 assert(!guarded(store,WENA_DOMAIN_CREATE_CARD,"title=No active lane"));
 sql(db,"DELETE FROM swimlane_archive_state WHERE swimlane_id IN ('t','empty')");
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys&&number(db,"SELECT sum(version) FROM cards")==versions);
}
static void original(sqlite3 *db)
{
 assert(number(db,"SELECT version FROM swimlanes WHERE id='s'")==1);
 assert(number(db,"SELECT count(*) FROM swimlane_archive_state")==0);
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=0 AND version=1")==2);
 assert(number(db,"SELECT version FROM cards WHERE id='pre'")==7);
 assert(number(db,"SELECT count(*) FROM card_archive_state")==1);
 assert(number(db,"SELECT archived_at FROM card_archive_state WHERE card_id='pre'")==number(db,"SELECT 9000000000000"));
 assert(!number(db,"SELECT count(*) FROM idempotency_keys"));
}
static void snapshot_state(sqlite3 *db,int archived,size_t active)
{
 WenaSqliteBoardSnapshot *snapshot;size_t i,count;int found;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));assert(snapshot&&wena_sqlite_board_load(db,"b",snapshot));found=0;count=0;
 for(i=0;i<snapshot->swimlane_count;++i)if(!strcmp(snapshot->swimlanes[i].id,"s")){assert(snapshot->swimlanes[i].archived==archived);found=1;}
 for(i=0;i<snapshot->card_count;++i)if(!strcmp(snapshot->cards[i].swimlane_id,"s")&&!snapshot->cards[i].archived)++count;
 assert(found&&count==active);free(snapshot);
}
static void rejected_snapshot(sqlite3 *db,const char *query,const char *undo)
{
 WenaSqliteBoardSnapshot *snapshot,*before;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));assert(snapshot&&before);
 assert(wena_sqlite_board_load(db,"b",snapshot));memcpy(before,snapshot,sizeof(*before));
 sql(db,"PRAGMA foreign_keys=OFF;PRAGMA ignore_check_constraints=ON");sql(db,query);
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 sql(db,undo);sql(db,"PRAGMA foreign_keys=ON;PRAGMA ignore_check_constraints=OFF");
 assert(wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));free(snapshot);free(before);
}
typedef struct Race {sqlite3 *writer;int fired;} Race;
static int concurrent_write(unsigned int event,void *context,void *statement,void *text)
{
 Race *race;const char *query;(void)text;race=(Race*)context;query=sqlite3_sql((sqlite3_stmt*)statement);
 if(event==SQLITE_TRACE_STMT&&!race->fired&&query&&strstr(query,"FROM swimlane_archive_state c")){
  race->fired=1;sql(race->writer,"BEGIN IMMEDIATE;UPDATE swimlane_archive_state SET archived=1 WHERE swimlane_id='s';UPDATE cards SET archived=1 WHERE swimlane_id='s';COMMIT");
 }
 return 0;
}
static void snapshot_tests(sqlite3 *db,const char *path)
{
 Race race;
 rejected_snapshot(db,"UPDATE swimlane_archive_state SET archived=2 WHERE swimlane_id='s'","UPDATE swimlane_archive_state SET archived=0 WHERE swimlane_id='s'");
 rejected_snapshot(db,"UPDATE swimlane_archive_state SET archived=x'31' WHERE swimlane_id='s'","UPDATE swimlane_archive_state SET archived=0 WHERE swimlane_id='s'");
 rejected_snapshot(db,"UPDATE swimlane_archive_state SET archived_at=-1 WHERE swimlane_id='s'","UPDATE swimlane_archive_state SET archived_at=0 WHERE swimlane_id='s'");
 rejected_snapshot(db,"UPDATE swimlane_archive_state SET archived_at=0.5 WHERE swimlane_id='s'","UPDATE swimlane_archive_state SET archived_at=0 WHERE swimlane_id='s'");
 rejected_snapshot(db,"UPDATE swimlane_archive_state SET board_id='other' WHERE swimlane_id='s'","UPDATE swimlane_archive_state SET board_id='b' WHERE swimlane_id='s'");
 rejected_snapshot(db,"UPDATE swimlane_archive_state SET board_id=x'62' WHERE swimlane_id='s'","UPDATE swimlane_archive_state SET board_id='b' WHERE swimlane_id='s'");
 rejected_snapshot(db,"INSERT INTO swimlane_archive_state VALUES('missing','b',1,123)","DELETE FROM swimlane_archive_state WHERE swimlane_id='missing'");
 rejected_snapshot(db,"INSERT INTO swimlane_archive_state VALUES('foreign','b',1,123)","DELETE FROM swimlane_archive_state WHERE swimlane_id='foreign'");
 rejected_snapshot(db,"INSERT INTO list_archive_state VALUES('missing','b',1,123)","DELETE FROM list_archive_state WHERE list_id='missing'");
 rejected_snapshot(db,"ALTER TABLE swimlane_archive_state RENAME TO saved_lanes","ALTER TABLE saved_lanes RENAME TO swimlane_archive_state");
 rejected_snapshot(db,"ALTER TABLE swimlane_archive_state RENAME TO saved_lanes;CREATE VIEW swimlane_archive_state AS SELECT * FROM saved_lanes","DROP VIEW swimlane_archive_state;ALTER TABLE saved_lanes RENAME TO swimlane_archive_state");
 sql(db,"PRAGMA journal_mode=WAL");assert(sqlite3_open(path,&race.writer)==SQLITE_OK);race.fired=0;
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,concurrent_write,&race)==SQLITE_OK);
 snapshot_state(db,0,2);assert(race.fired);assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 snapshot_state(db,1,0);
 sql(race.writer,"BEGIN IMMEDIATE;UPDATE swimlane_archive_state SET archived=0 WHERE swimlane_id='s';UPDATE cards SET archived=0 WHERE id IN ('a','z');COMMIT");
 assert(sqlite3_close(race.writer)==SQLITE_OK);snapshot_state(db,0,2);
}
static int reject_commit(void *context)
{int *calls;calls=(int*)context;++*calls;return 1;}
static void native_archive(sqlite3 *db)
{
 WenaSqliteBoardSnapshot *snapshot,*before,*fresh;WenaHierarchyMutation adapter;
 unsigned long version;sqlite3_int64 keys;int calls;size_t i,peer_count;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));
 before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));
 fresh=(WenaSqliteBoardSnapshot*)malloc(sizeof(*fresh));assert(snapshot&&before&&fresh);
 assert(wena_sqlite_board_load(db,"b",snapshot));
 assert(wena_hierarchy_mutation_init(&adapter,db,"u","b",snapshot));
 peer_count=snapshot->card_count;adapter.published_card_count=&peer_count;
 version=77;assert(!wena_hierarchy_mutation_swimlane_archive_load(&adapter,"other","s",&version)&&version==77);
 assert(!wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","missing",&version)&&version==77);
 strcpy(adapter.actor_id,"missing");assert(!wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","s",&version)&&version==77);
 strcpy(adapter.actor_id,"u");
 assert(wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","s",&version)&&version==5);
 memcpy(before,snapshot,sizeof(*before));keys=number(db,"SELECT count(*) FROM idempotency_keys");
 snapshot->swimlanes[0].archived=2;version=77;
 assert(!wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","s",&version)&&version==77);
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",5,200,1));
 memcpy(snapshot,before,sizeof(*before));
 strcpy(snapshot->swimlanes[1].id,snapshot->swimlanes[0].id);
 assert(!wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","s",&version)&&version==77);
 memcpy(snapshot,before,sizeof(*before));
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",4,200,1));
 /* The cascade succeeds, but unrelated corrupt metadata prevents staging.
  * Its cards, lane revision and request identity must all roll back. */
 sql(db,"PRAGMA ignore_check_constraints=ON;INSERT INTO swimlane_colors VALUES('t','b','invalid');PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",5,200,1));
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",5,200,0));
 assert(!memcmp(before,snapshot,sizeof(*before)));
 sql(db,"DELETE FROM swimlane_colors WHERE swimlane_id='t'");
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",5,200,1)&&calls==1);
 sqlite3_commit_hook(db,NULL,NULL);
 assert(!adapter.persistence.prepare_publish&&!adapter.persistence.publish_context);
 assert(peer_count==snapshot->card_count);
 assert(!memcmp(before,snapshot,sizeof(*before)));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(before,fresh,sizeof(*fresh)));
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
 sql(db,"CREATE TRIGGER native_failure BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",5,200,1));
 sql(db,"DROP TRIGGER native_failure");
 assert(!memcmp(before,snapshot,sizeof(*before)));
 /* Publish every child, including cards absent from a stale display cache. */
 snapshot->card_count=0;peer_count=0;
 assert(wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",5,200,1));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(peer_count==snapshot->card_count&&peer_count>0);
 assert(wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","s",&version)&&version==6);
 for(i=0;i<snapshot->card_count;++i)if(!strcmp(snapshot->cards[i].swimlane_id,"s"))assert(snapshot->cards[i].archived);
 memcpy(before,snapshot,sizeof(*before));
 assert(!wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",6,200,1));
 assert(!memcmp(before,snapshot,sizeof(*before)));
 assert(wena_hierarchy_mutation_swimlane_archive_request(&adapter,"b","s",6,201,1));
 assert(!memcmp(before,snapshot,sizeof(*before))&&number(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
 assert(wena_hierarchy_mutation_swimlane_restore(&adapter,"b","s",6));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(wena_hierarchy_mutation_swimlane_archive_load(&adapter,"b","s",&version)&&version==7);
 assert(number(db,"SELECT archived FROM cards WHERE id='pre'")==1);
 assert(wena_hierarchy_mutation_swimlane_archive(&adapter,"b","s",7));
 assert(wena_hierarchy_mutation_swimlane_restore(&adapter,"b","s",8));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(!adapter.persistence.prepare_publish&&!adapter.persistence.publish_context);
 assert(peer_count==snapshot->card_count);
 free(snapshot);free(before);free(fresh);
}
static int list_cards(WenaSqlitePersistence *store,const char *form,unsigned long request)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));
 c.operation=WENA_DOMAIN_ARCHIVE_LIST_CARDS;c.request_version=request;
 strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");strcpy(c.form_body,form);c.form_body_length=strlen(form);
 return wena_sqlite_persistence_apply(store,&c,&r);
}
static void native_list_cards(sqlite3 *db)
{
 WenaSqliteBoardSnapshot *snapshot,*before,*fresh;WenaHierarchyMutation adapter;
 unsigned long list_version,lane_version;size_t peer;sqlite3_int64 keys;int calls;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));
 fresh=(WenaSqliteBoardSnapshot*)malloc(sizeof(*fresh));assert(snapshot&&before&&fresh);
 sql(db,"UPDATE cards SET archived=0,version=version+1 WHERE id IN ('batch-a','batch-z','batch-other')");
 assert(wena_sqlite_board_load(db,"b",snapshot));
 assert(wena_hierarchy_mutation_init(&adapter,db,"u","b",snapshot));
 peer=snapshot->card_count;adapter.published_card_count=&peer;
 list_version=77;lane_version=88;
 assert(!wena_hierarchy_mutation_list_cards_load(&adapter,"b","batch","foreign",&list_version,&lane_version));
 assert(list_version==77&&lane_version==88);
 assert(wena_hierarchy_mutation_list_cards_load(&adapter,"b","batch","t",&list_version,&lane_version));
 assert(list_version==1&&lane_version==1);
 memcpy(before,snapshot,sizeof(*before));keys=number(db,"SELECT count(*) FROM idempotency_keys");
 assert(!wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,2,500));
 assert(!wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch",NULL,1,1,500));
 sql(db,"CREATE TRIGGER native_batch_fail BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,1,500));
 sql(db,"DROP TRIGGER native_batch_fail");
 sql(db,"PRAGMA ignore_check_constraints=ON;INSERT INTO swimlane_colors VALUES('t','b','invalid');PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,1,500));
 sql(db,"DELETE FROM swimlane_colors WHERE swimlane_id='t'");
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);
 assert(!wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,1,500)&&calls==1);
 sqlite3_commit_hook(db,NULL,NULL);
 assert(!memcmp(before,snapshot,sizeof(*before))&&peer==snapshot->card_count);
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(before,fresh,sizeof(*fresh)));
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
 snapshot->card_count=0;peer=0;
 assert(wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,1,500));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(peer==snapshot->card_count&&peer>0);
 assert(number(db,"SELECT archived FROM cards WHERE id='batch-other'")==0);
 assert(number(db,"SELECT version FROM cards WHERE id='batch-pre'")==4);
 memcpy(before,snapshot,sizeof(*before));
 assert(!wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,1,500));
 assert(!memcmp(before,snapshot,sizeof(*before)));
 assert(wena_hierarchy_mutation_list_cards_archive_request(&adapter,"b","batch","t",1,1,501));
 assert(!memcmp(before,snapshot,sizeof(*before))&&number(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
 assert(wena_hierarchy_mutation_list_cards_load(&adapter,"b","batch",NULL,&list_version,&lane_version));
 assert(list_version==1&&lane_version==0);
 assert(wena_hierarchy_mutation_list_cards_archive(&adapter,"b","batch","",list_version,lane_version));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(number(db,"SELECT count(*) FROM cards WHERE list_id='batch' AND archived=1")==4);
 assert(!adapter.persistence.prepare_publish&&!adapter.persistence.publish_context);
 free(snapshot);free(before);free(fresh);
}
static void list_cards_tests(sqlite3 *db,WenaSqlitePersistence *store)
{
 const char *scoped="listId=batch&expectedVersion=1&swimlaneId=t&expectedSwimlaneVersion=1";
 const char *all="listId=batch&expectedVersion=1";sqlite3_int64 keys;size_t i;
 const char *failures[]={
 "CREATE TRIGGER batch_fail BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END",
 "CREATE TRIGGER batch_fail BEFORE UPDATE ON cards WHEN OLD.id='batch-z' BEGIN SELECT RAISE(ABORT,'second');END",
 "CREATE TRIGGER batch_fail AFTER UPDATE ON cards WHEN NEW.id='batch-z' BEGIN UPDATE cards SET archived=0 WHERE id='batch-a';END",
 "CREATE TRIGGER batch_fail AFTER UPDATE ON cards WHEN NEW.id='batch-z' BEGIN UPDATE cards SET swimlane_id='empty' WHERE id='batch-a';END",
 "CREATE TRIGGER batch_fail AFTER UPDATE ON cards WHEN NEW.id='batch-z' BEGIN UPDATE lists SET version=version+1 WHERE id='batch';END"};
 sql(db,"INSERT INTO lists VALUES('batch','b','Batch',3,1);"
 "INSERT INTO cards VALUES('batch-a','b','t','batch','A',0,0,1),('batch-z','b','t','batch','Z',1,0,1),"
 "('batch-pre','b','t','batch','Prior',2,1,4),('batch-other','b','empty','batch','Other',0,0,1);"
 "INSERT INTO card_archive_state VALUES('batch-pre','b',123)");
 keys=number(db,"SELECT count(*) FROM idempotency_keys");
 assert(!list_cards(store,"listId=batch&expectedVersion=2",300));
 assert(!list_cards(store,"listId=batch&expectedVersion=1&swimlaneId=foreign&expectedSwimlaneVersion=1",300));
 assert(!list_cards(store,"listId=batch&expectedVersion=1&swimlaneId=t&expectedSwimlaneVersion=2",300));
 assert(!list_cards(store,"listId=batch&expectedVersion=1&swimlaneId=t",300));
 assert(!list_cards(store,"listId=batch&expectedVersion=1&expectedSwimlaneVersion=1",300));
 sql(db,"INSERT INTO list_archive_state VALUES('batch','b',1,1)");assert(!list_cards(store,all,300));
 sql(db,"DELETE FROM list_archive_state WHERE list_id='batch'");
 sql(db,"INSERT INTO swimlane_archive_state VALUES('t','b',1,1)");assert(!list_cards(store,scoped,300));
 sql(db,"DELETE FROM swimlane_archive_state WHERE swimlane_id='t'");
 sql(db,"PRAGMA query_only=ON");assert(!list_cards(store,all,300));sql(db,"PRAGMA query_only=OFF");
 for(i=0;i<sizeof(failures)/sizeof(failures[0]);++i){
  sql(db,failures[i]);assert(!list_cards(store,scoped,300));sql(db,"DROP TRIGGER batch_fail");
  assert(number(db,"SELECT count(*) FROM cards WHERE list_id='batch' AND archived=0 AND version=1")==3);
  assert(number(db,"SELECT count(*) FROM cards WHERE list_id='batch' AND swimlane_id='t'")==3);
  assert(number(db,"SELECT version FROM lists WHERE id='batch'")==1);
  assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
 }
 assert(list_cards(store,scoped,300));
 assert(number(db,"SELECT count(*) FROM cards WHERE list_id='batch' AND swimlane_id='t' AND archived=1")==3);
 assert(number(db,"SELECT version FROM cards WHERE id='batch-pre'")==4);
 assert(number(db,"SELECT archived_at FROM card_archive_state WHERE card_id='batch-pre'")==123);
 assert(number(db,"SELECT archived FROM cards WHERE id='batch-other'")==0);
 assert(!list_cards(store,scoped,300));assert(list_cards(store,scoped,301));
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
 assert(list_cards(store,all,301));
 assert(number(db,"SELECT count(*) FROM cards WHERE list_id='batch' AND archived=1")==4);
 assert(number(db,"SELECT version FROM lists WHERE id='batch'")==1);
 assert(number(db,"SELECT version FROM swimlanes WHERE id='t'")==1);
 assert(list_cards(store,all,302));assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys+2);
}
static int selected_archive(WenaSqlitePersistence *store,const WenaDomainCardRevision *cards,size_t count,unsigned long request)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));
 c.operation=WENA_DOMAIN_ARCHIVE_SELECTED_CARDS;c.request_version=request;
 strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");c.selected_cards=cards;c.selected_card_count=count;
 return wena_sqlite_persistence_apply(store,&c,&r);
}
static void selected_archive_tests(sqlite3 *db,WenaSqlitePersistence *store)
{
 WenaDomainCardRevision cards[2];sqlite3_int64 keys;size_t i;int calls;
 const char *triggers[]={
 "CREATE TRIGGER selected_fail BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END",
 "CREATE TRIGGER selected_fail BEFORE UPDATE ON cards WHEN OLD.id='sel-z' BEGIN SELECT RAISE(ABORT,'second');END",
 "CREATE TRIGGER selected_fail AFTER UPDATE ON cards WHEN NEW.id='sel-z' BEGIN UPDATE cards SET archived=0 WHERE id='sel-a';END",
 "CREATE TRIGGER selected_fail AFTER UPDATE ON cards WHEN NEW.id='sel-z' BEGIN UPDATE cards SET swimlane_id='empty' WHERE id='sel-a';END"};
 sql(db,"INSERT INTO cards VALUES('sel-a','b','t','batch','Same',7,0,1),('sel-z','b','empty','l','Same',3000,0,2),('sel-other','b','t','l','Same',10,0,1)");
 sql(db,"INSERT INTO lists VALUES('foreign-list','other','Foreign',0,1);INSERT INTO cards VALUES('sel-foreign','other','foreign','foreign-list','Foreign',0,0,1)");
 memset(cards,0,sizeof(cards));strcpy(cards[0].id,"sel-a");cards[0].version=1;strcpy(cards[1].id,"sel-z");cards[1].version=2;
 keys=number(db,"SELECT count(*) FROM idempotency_keys");
 assert(!selected_archive(store,NULL,2,600));assert(!selected_archive(store,cards,0,600));
 assert(!selected_archive(store,cards,WENA_DOMAIN_CARD_BATCH_CAPACITY+1,600));
 cards[1].version=1;assert(!selected_archive(store,cards,2,600));cards[1].version=2;
 strcpy(cards[1].id,"sel-a");cards[1].version=1;assert(!selected_archive(store,cards,2,600));
 strcpy(cards[1].id,"missing");assert(!selected_archive(store,cards,2,600));
 strcpy(cards[1].id,"sel-foreign");assert(!selected_archive(store,cards,2,600));
 strcpy(cards[1].id,"bad/id");assert(!selected_archive(store,cards,2,600));
 strcpy(cards[1].id,"batch-pre");cards[1].version=4;assert(!selected_archive(store,cards,2,600));
 strcpy(cards[1].id,"sel-z");cards[1].version=2;
 sql(db,"PRAGMA query_only=ON");assert(!selected_archive(store,cards,2,600));sql(db,"PRAGMA query_only=OFF");
 for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){
  sql(db,triggers[i]);assert(!selected_archive(store,cards,2,600));sql(db,"DROP TRIGGER selected_fail");
  assert(number(db,"SELECT count(*) FROM cards WHERE id IN ('sel-a','sel-z') AND archived=0")==2);
  assert(number(db,"SELECT version FROM cards WHERE id='sel-a'")==1&&number(db,"SELECT version FROM cards WHERE id='sel-z'")==2);
  assert(number(db,"SELECT count(*) FROM card_archive_state WHERE card_id IN ('sel-a','sel-z')")==0);
  assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
 }
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);
 assert(!selected_archive(store,cards,2,600)&&calls==1);sqlite3_commit_hook(db,NULL,NULL);
 assert(number(db,"SELECT count(*) FROM cards WHERE id IN ('sel-a','sel-z') AND archived=0")==2);
 assert(selected_archive(store,cards,2,600));
 assert(number(db,"SELECT count(*) FROM cards WHERE id IN ('sel-a','sel-z') AND archived=1")==2);
 assert(number(db,"SELECT archived FROM cards WHERE id='sel-other'")==0);
 assert(number(db,"SELECT position FROM cards WHERE id='sel-a'")==7&&number(db,"SELECT position FROM cards WHERE id='sel-z'")==3000);
 assert(!selected_archive(store,cards,2,600));++cards[0].version;++cards[1].version;
 assert(!selected_archive(store,cards,2,601));assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
}
static int selection_race(unsigned int event,void *context,void *statement,void *text)
{
 Race *race;const char *query;(void)text;race=(Race*)context;query=sqlite3_sql((sqlite3_stmt*)statement);
 if(event==SQLITE_TRACE_STMT&&!race->fired&&query&&strstr(query,"SELECT id,board_id,list_id,version,swimlane_id FROM cards WHERE id=")){
  race->fired=1;sql(race->writer,"BEGIN IMMEDIATE;UPDATE cards SET version=version+1 WHERE id IN ('sel-a','sel-z');COMMIT");
 }
 return 0;
}
static void native_selected_archive(sqlite3 *db,const char *path)
{
 WenaSqliteBoardSnapshot *snapshot,*before,*fresh;WenaHierarchyMutation adapter;
 WenaDomainCardRevision *rows,*saved,original_rows[2];WenaId ids[2];size_t peer;
 sqlite3_int64 keys;int calls;Race race;
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));
 fresh=(WenaSqliteBoardSnapshot*)malloc(sizeof(*fresh));assert(snapshot&&before&&fresh);
 sql(db,"UPDATE cards SET archived=0,version=version+1 WHERE id IN ('sel-a','sel-z')");
 assert(wena_sqlite_board_load(db,"b",snapshot));assert(wena_hierarchy_mutation_init(&adapter,db,"u","b",snapshot));
 peer=snapshot->card_count;adapter.published_card_count=&peer;rows=NULL;
 strcpy(ids[0],"sel-a");strcpy(ids[1],"sel-z");
 assert(sqlite3_open(path,&race.writer)==SQLITE_OK);race.fired=0;
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,selection_race,&race)==SQLITE_OK);
 assert(wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,2,&rows));
 assert(rows&&rows[0].version==3&&rows[1].version==4&&!strcmp(rows[0].id,"sel-a")&&!strcmp(rows[1].id,"sel-z"));
 assert(race.fired);assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(number(db,"SELECT version FROM cards WHERE id='sel-a'")==4&&number(db,"SELECT version FROM cards WHERE id='sel-z'")==5);
 assert(!wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800));
 sql(race.writer,"UPDATE cards SET version=version-1 WHERE id IN ('sel-a','sel-z')");
 assert(sqlite3_close(race.writer)==SQLITE_OK);
 saved=rows;memcpy(original_rows,rows,sizeof(original_rows));
 strcpy(adapter.actor_id,"missing");
 assert(!wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,2,&rows));
 strcpy(adapter.actor_id,"u");strcpy(ids[1],"sel-a");
 assert(!wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,2,&rows));
 strcpy(ids[1],"sel-foreign");assert(!wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,2,&rows));
 strcpy(ids[1],"batch-pre");assert(!wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,2,&rows));
 strcpy(ids[1],"sel-z");
 assert(!wena_hierarchy_mutation_selected_cards_load(&adapter,"other",(const WenaId*)ids,2,&rows));
 assert(rows==saved&&!memcmp(rows,original_rows,sizeof(original_rows)));
 memcpy(before,snapshot,sizeof(*before));keys=number(db,"SELECT count(*) FROM idempotency_keys");
 ++rows[1].version;assert(!wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800));--rows[1].version;
 sql(db,"CREATE TRIGGER native_selected_fail BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800));
 sql(db,"DROP TRIGGER native_selected_fail");
 sql(db,"PRAGMA ignore_check_constraints=ON;INSERT INTO swimlane_colors VALUES('t','b','invalid');PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800));
 sql(db,"DELETE FROM swimlane_colors WHERE swimlane_id='t'");
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);
 assert(!wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800)&&calls==1);
 sqlite3_commit_hook(db,NULL,NULL);
 assert(!memcmp(before,snapshot,sizeof(*before))&&peer==snapshot->card_count&&!memcmp(rows,original_rows,sizeof(original_rows)));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(before,fresh,sizeof(*fresh)));
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
 snapshot->card_count=0;peer=0;
 assert(wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800));
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(peer==snapshot->card_count&&peer>0&&!memcmp(rows,original_rows,sizeof(original_rows)));
 memcpy(before,snapshot,sizeof(*before));
 assert(!wena_hierarchy_mutation_selected_cards_archive_request(&adapter,"b",rows,2,800)&&!memcmp(before,snapshot,sizeof(*before)));
 assert(!wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,2,&rows)&&rows==saved);
 strcpy(ids[0],"sel-other");
 assert(wena_hierarchy_mutation_selected_cards_load(&adapter,"b",(const WenaId*)ids,1,&rows));
 assert(wena_hierarchy_mutation_selected_cards_archive(&adapter,"b",rows,1));
 assert(number(db,"SELECT archived FROM cards WHERE id='sel-other'")==1);
 assert(!adapter.persistence.prepare_publish&&!adapter.persistence.publish_context);
 free(rows);free(snapshot);free(before);free(fresh);
}
static void full_selection(sqlite3 *db,WenaSqlitePersistence *store)
{
 WenaDomainCardRevision *cards;size_t i;
 cards=(WenaDomainCardRevision*)calloc(WENA_DOMAIN_CARD_BATCH_CAPACITY,sizeof(*cards));assert(cards);
 for(i=0;i<WENA_DOMAIN_CARD_BATCH_CAPACITY;++i){sprintf(cards[i].id,"bulk%lu",(unsigned long)i);cards[i].version=1;}
 assert(selected_archive(store,cards,WENA_DOMAIN_CARD_BATCH_CAPACITY,700));
 assert(number(db,"SELECT count(*) FROM cards WHERE id GLOB 'bulk*' AND archived=1")==2048);
 assert(number(db,"SELECT count(*) FROM cards WHERE id GLOB 'bulk*' AND archived=0")==1);
 free(cards);sql(db,"DELETE FROM card_archive_state WHERE card_id GLOB 'bulk*'");
}
static int move_capture_race(unsigned int event,void *context,void *statement,void *text)
{
 Race *race;const char *query;(void)text;race=(Race*)context;query=sqlite3_sql((sqlite3_stmt*)statement);
 if(event==SQLITE_TRACE_STMT&&!race->fired&&query&&strstr(query,"SELECT id,board_id,list_id,version,swimlane_id FROM cards WHERE id=")){
  race->fired=1;sql(race->writer,"UPDATE cards SET position=6 WHERE id='nc'");
 }
 return 0;
}
static int move_capture_deny_commit(void *context,int action,const char *first,
 const char *second,const char *database,const char *trigger)
{
 int *calls;(void)second;(void)database;(void)trigger;calls=(int*)context;
 if(action==SQLITE_TRANSACTION&&first&&!strcmp(first,"COMMIT")){++*calls;return SQLITE_DENY;}
 return SQLITE_OK;
}
static void native_selected_move(sqlite3 *db,const char *path)
{
 WenaSqliteBoardSnapshot *snapshot,*before,*fresh;WenaHierarchyMutation adapter;
 WenaCardMoveSelection *capture,*saved,*original;WenaId ids[2];size_t peer;int calls;sqlite3_int64 keys;Race race;char fingerprint[65];
 sql(db,"INSERT INTO boards VALUES('nm','Native move',1);INSERT INTO lists VALUES('ns','nm','Source',0,1),('nd','nm','Destination',1,1);"
  "INSERT INTO swimlanes VALUES('nl','nm','Lane',0,1);INSERT INTO cards VALUES('na','nm','nl','ns','Repeated',0,0,1),('nb','nm','nl','ns','Repeated',1,0,1),('nc','nm','nl','nd','Other',5,0,1)");
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));fresh=(WenaSqliteBoardSnapshot*)malloc(sizeof(*fresh));
 original=(WenaCardMoveSelection*)malloc(sizeof(*original));assert(snapshot&&before&&fresh&&original);
 assert(wena_sqlite_board_load(db,"nm",snapshot));assert(wena_hierarchy_mutation_init(&adapter,db,"u","nm",snapshot));
 peer=snapshot->card_count;adapter.published_card_count=&peer;*before=*snapshot;
 strcpy(ids[0],"nb");strcpy(ids[1],"na");capture=NULL;
 sql(db,"BEGIN");assert(wena_sqlite_card_board_order(db,"nm",fingerprint));sql(db,"COMMIT");
 assert(sqlite3_open(path,&race.writer)==SQLITE_OK);race.fired=0;
 snapshot->card_count=0;peer=0;strcpy(snapshot->lists[0].title,"Stale display");
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,move_capture_race,&race)==SQLITE_OK);
 assert(wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK&&race.fired);
 assert(!strcmp(capture->fingerprint,fingerprint)&&capture->count==2&&!strcmp(capture->cards[0].id,"nb"));
 assert(peer==3&&!memcmp(snapshot,before,sizeof(*before)));
 assert(wena_sqlite_board_load(db,"nm",fresh)&&memcmp(snapshot,fresh,sizeof(*fresh)));
 assert(!wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000));
 sql(race.writer,"UPDATE cards SET position=5 WHERE id='nc'");assert(sqlite3_close(race.writer)==SQLITE_OK);
 *original=*capture;saved=capture;
 strcpy(ids[1],"nb");assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));
 strcpy(ids[1],"sel-a");assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));
 strcpy(ids[1],"na");strcpy(adapter.actor_id,"missing");
 assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));strcpy(adapter.actor_id,"u");
 sql(db,"INSERT INTO list_archive_state VALUES('ns','nm',1,1)");
 assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));
 sql(db,"UPDATE list_archive_state SET archived=0 WHERE list_id='ns';UPDATE cards SET archived=1 WHERE id='na'");
 assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));sql(db,"UPDATE cards SET archived=0 WHERE id='na'");
 sql(db,"PRAGMA ignore_check_constraints=ON;INSERT INTO swimlane_colors VALUES('nl','nm','invalid');PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));
 sql(db,"DELETE FROM swimlane_colors WHERE swimlane_id='nl'");
 calls=0;assert(sqlite3_set_authorizer(db,move_capture_deny_commit,&calls)==SQLITE_OK);
 assert(!wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture)&&calls==1);
 assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK&&sqlite3_get_autocommit(db));
 assert(saved==capture&&!memcmp(capture,original,sizeof(*original))&&!memcmp(snapshot,before,sizeof(*before))&&peer==3);
 keys=number(db,"SELECT count(*) FROM idempotency_keys");
 capture->fingerprint[64]='x';assert(!wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000));*capture=*original;
 sql(db,"CREATE TRIGGER native_move_late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000));sql(db,"DROP TRIGGER native_move_late");
 sql(db,"PRAGMA ignore_check_constraints=ON;INSERT INTO swimlane_colors VALUES('nl','nm','invalid');PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000));sql(db,"DELETE FROM swimlane_colors WHERE swimlane_id='nl'");
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);
 assert(!wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000)&&calls==1);sqlite3_commit_hook(db,NULL,NULL);
 assert(!memcmp(snapshot,before,sizeof(*before))&&!memcmp(capture,original,sizeof(*original))&&peer==3);
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys&&number(db,"SELECT count(*) FROM cards WHERE list_id='ns' AND version=1")==2);
 assert(!adapter.persistence.prepare_publish&&!adapter.persistence.publish_context);
 snapshot->card_count=0;peer=0;
 assert(wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000));
 assert(peer==3&&wena_sqlite_board_load(db,"nm",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh))&&!memcmp(capture,original,sizeof(*original)));
 *before=*snapshot;assert(!wena_hierarchy_mutation_selected_move_request(&adapter,capture,"nd","nl",0,8000));assert(!memcmp(snapshot,before,sizeof(*before)));
 assert(wena_hierarchy_mutation_selected_move_load(&adapter,"nm",(const WenaId*)ids,2,&capture));
 keys=number(db,"SELECT count(*) FROM idempotency_keys");snapshot->card_count=0;peer=0;
 assert(wena_hierarchy_mutation_selected_move(&adapter,capture,"nd","nl",0));
 assert(peer==3&&!memcmp(snapshot,fresh,sizeof(*fresh))&&keys==number(db,"SELECT count(*) FROM idempotency_keys"));
 assert(wena_hierarchy_mutation_selected_move(&adapter,capture,"ns","nl",0));
 assert(wena_sqlite_board_load(db,"nm",fresh)&&!memcmp(snapshot,fresh,sizeof(*fresh))&&peer==3);
 free(capture);free(original);free(snapshot);free(before);free(fresh);
}

static void transfer_command(sqlite3 *db,WenaDomainCommand *c,WenaCardRevision *cards,size_t count,
 const char *source,const char *target,const char *list,const char *lane,size_t before,unsigned long request)
{
 char a[65],b[65];unsigned long av,bv;char query[128];
 sql(db,"BEGIN");assert(wena_sqlite_card_board_order(db,source,a));assert(wena_sqlite_card_board_order(db,target,b));sql(db,"COMMIT");
 sprintf(query,"SELECT version FROM boards WHERE id='%s'",source);av=(unsigned long)number(db,query);
 sprintf(query,"SELECT version FROM boards WHERE id='%s'",target);bv=(unsigned long)number(db,query);
 memset(c,0,sizeof(*c));c->operation=WENA_DOMAIN_TRANSFER_SELECTED_CARDS;c->request_version=request;
 strcpy(c->user_id,"u");sprintf(c->route,"/b/%s/native",source);c->selected_cards=cards;c->selected_card_count=count;
 sprintf(c->form_body,"targetBoardId=%s&targetListId=%s&targetSwimlaneId=%s&insertPosition=%lu&expectedBoardVersion=%lu&expectedTargetBoardVersion=%lu&expectedBoardOrder=%s&expectedTargetBoardOrder=%s",target,list,lane,(unsigned long)before,av,bv,a,b);
 c->form_body_length=strlen(c->form_body);
}
static void transfer_original(sqlite3 *db)
{
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='xs' AND version=1")==3);
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='xt' AND version=1")==2);
 assert(number(db,"SELECT position FROM cards WHERE id='xd0'")==5&&number(db,"SELECT position FROM cards WHERE id='xd1'")==8);
 assert(number(db,"SELECT count(*) FROM boards WHERE id IN('xs','xt') AND version=1")==2);
 assert(number(db,"SELECT count(*) FROM card_descriptions WHERE board_id='xs' AND description IN('Description','Other description')")==2);
 assert(number(db,"SELECT count(*) FROM card_archive_state WHERE board_id='xs' AND archived_at=123")==1);
 assert(number(db,"SELECT count(*) FROM checklists WHERE board_id='xs' AND version=1 AND hide_all_items=1")==1);
 assert(number(db,"SELECT count(*) FROM checklist_items WHERE board_id='xs' AND version=1 AND title='Finished' AND is_finished=1")==1);
 assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='xs'")==3&&number(db,"SELECT count(*) FROM card_labels WHERE board_id='xt'")==0);
 assert(number(db,"SELECT count(*) FROM actor_card_sections WHERE card_id='xa' AND collapsed=1 AND version=1")==1);
 assert(number(db,"SELECT count(*) FROM idempotency_keys WHERE operation='transfer-selected-cards'")==0);
 assert(sqlite3_get_autocommit(db)&&number(db,"PRAGMA defer_foreign_keys")==0);
}
static int transfer_reject_publish(void *context,sqlite3 *db){(void)context;(void)db;return 0;}
static void cross_board_capacity(sqlite3 *db)
{
 WenaSqlitePersistence store;WenaDomainCommand c;WenaRegionResponse response;WenaCardRevision *rows;size_t i;
 sql(db,"INSERT INTO boards VALUES('fs','Full source',1),('ft','Full target',1);"
 "INSERT INTO lists VALUES('fsl','fs','Source',0,1),('ftl','ft','Target',0,1);"
 "INSERT INTO swimlanes VALUES('fss','fs','Lane',0,1),('fts','ft','Lane',0,1);"
 "INSERT INTO cards VALUES('occupied','ft','fts','ftl','Existing',0,0,1)");
 sql(db,"WITH RECURSIVE n(x) AS (SELECT 0 UNION ALL SELECT x+1 FROM n WHERE x<2047) INSERT INTO cards SELECT 'f'||x,'fs','fss','fsl','Full',x,0,1 FROM n");
 rows=(WenaCardRevision*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*rows));assert(rows);
 for(i=0;i<WENA_CARD_ORDER_CAPACITY;++i){sprintf(rows[i].id,"f%lu",(unsigned long)(WENA_CARD_ORDER_CAPACITY-1-i));rows[i].version=1;}
 wena_sqlite_persistence_init(&store,db);transfer_command(db,&c,rows,WENA_CARD_ORDER_CAPACITY,"fs","ft","ftl","fts",0,9001);
 assert(!wena_sqlite_persistence_apply(&store,&c,&response));
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='fs' AND version=1")==2048);
 sql(db,"DELETE FROM cards WHERE id='occupied'");transfer_command(db,&c,rows,WENA_CARD_ORDER_CAPACITY,"fs","ft","ftl","fts",0,9001);
 assert(wena_sqlite_persistence_apply(&store,&c,&response));
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='fs'")==0&&number(db,"SELECT count(*) FROM cards WHERE board_id='ft' AND version=2")==2048);
 assert(number(db,"SELECT position FROM cards WHERE id='f2047'")==0&&number(db,"SELECT position FROM cards WHERE id='f0'")==2047);
 free(rows);
}
static void cross_board_selected(sqlite3 *db)
{
 WenaSqlitePersistence store;WenaDomainCommand command;WenaRegionResponse response;WenaCardRevision rows[2];size_t i;int calls;
 const char *triggers[]={
 "CREATE TRIGGER cross_fail BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END",
 "CREATE TRIGGER cross_fail BEFORE UPDATE OF board_id ON cards WHEN OLD.id='xa' BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER cross_fail AFTER UPDATE OF board_id ON cards WHEN NEW.id='xa' BEGIN UPDATE checklist_items SET title='Changed' WHERE id='xi';END",
 "CREATE TRIGGER cross_fail AFTER UPDATE OF board_id ON cards WHEN NEW.id='xa' BEGIN UPDATE card_descriptions SET description='Changed' WHERE card_id='xb';END",
 "CREATE TRIGGER cross_fail AFTER UPDATE OF board_id ON cards WHEN NEW.id='xa' BEGIN UPDATE labels SET name='Changed' WHERE board_id='xt' AND id='d0';END",
 "CREATE TRIGGER cross_fail AFTER UPDATE OF version ON boards WHEN NEW.id='xt' BEGIN UPDATE cards SET position=99 WHERE id='xu';END",
 "CREATE TRIGGER cross_fail AFTER UPDATE OF board_id ON cards WHEN NEW.id='xa' BEGIN INSERT INTO list_archive_state VALUES('xsl','xs',1,1);END",
 "CREATE TRIGGER cross_fail AFTER UPDATE OF board_id ON cards WHEN NEW.id='xa' BEGIN UPDATE actor_card_sections SET collapsed=0 WHERE card_id='xa';END"
 };
 sql(db,"INSERT INTO boards VALUES('xs','Source',1),('xt','Target',1);"
 "INSERT INTO lists VALUES('xsl','xs','Source',0,1),('xtl','xt','Target',0,1);"
 "INSERT INTO swimlanes VALUES('xss','xs','Lane',0,1),('xts','xt','Lane',0,1)");
 sql(db,"INSERT INTO cards VALUES('xa','xs','xss','xsl','Repeated',0,0,1),('xb','xs','xss','xsl','Repeated',1,0,1),"
 "('xu','xs','xss','xsl','Untouched',2,0,1),('xd0','xt','xts','xtl','Destination',5,0,1),('xd1','xt','xts','xtl','Archived',8,1,1)");
 sql(db,"INSERT INTO card_descriptions VALUES('xa','xs','Description'),('xb','xs','Other description');INSERT INTO card_archive_state VALUES('xa','xs',123);"
 "INSERT INTO actor_card_sections VALUES('u','xa','labels',1,1)");
 sql(db,"INSERT INTO checklists(id,board_id,card_id,title,position,hide_all_items) VALUES('xc','xs','xa','Hidden',7,1);"
 "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('xi','xs','xa','xc','Finished',9,1)");
 sql(db,"INSERT INTO labels VALUES('xs','s0','Match','red',0,1,0,0),('xs','blank','','blue',1,1,0,0),('xt','d0','Match','blue',0,1,0,0),('xt','d1','Match','green',1,1,0,0);"
 "INSERT INTO card_labels VALUES('xs','xa','s0'),('xs','xa','blank'),('xs','xb','s0')");
 strcpy(rows[0].id,"xb");rows[0].version=1;strcpy(rows[1].id,"xa");rows[1].version=1;
 wena_sqlite_persistence_init(&store,db);transfer_command(db,&command,rows,2,"xs","xt","xtl","xts",1,9000);
 command.selected_card_count=0;assert(!wena_sqlite_persistence_apply(&store,&command,&response));command.selected_card_count=2;
 strcpy(command.user_id,"missing");assert(!wena_sqlite_persistence_apply(&store,&command,&response));strcpy(command.user_id,"u");
 strcpy(rows[1].id,"xb");assert(!wena_sqlite_persistence_apply(&store,&command,&response));strcpy(rows[1].id,"xa");
 rows[1].version=2;assert(!wena_sqlite_persistence_apply(&store,&command,&response));rows[1].version=1;
 sql(db,"UPDATE cards SET position=6 WHERE id='xd0'");assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"UPDATE cards SET position=5 WHERE id='xd0'");
 sql(db,"UPDATE boards SET version=2 WHERE id='xt'");assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"UPDATE boards SET version=1 WHERE id='xt'");
 sql(db,"INSERT INTO list_wip_limits VALUES('xtl','xt',2,1,0)");assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"UPDATE list_wip_limits SET value=3 WHERE list_id='xtl'");
 sql(db,"INSERT INTO list_archive_state VALUES('xtl','xt',1,1)");assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"DELETE FROM list_archive_state WHERE list_id='xtl'");
 transfer_original(db);
 transfer_command(db,&command,rows,2,"xs","xt","xtl","xts",3,9000);assert(!wena_sqlite_persistence_apply(&store,&command,&response));
 transfer_command(db,&command,rows,2,"xs","xt","xsl","xts",1,9000);assert(!wena_sqlite_persistence_apply(&store,&command,&response));
 transfer_command(db,&command,rows,2,"xs","xs","xsl","xss",1,9000);assert(!wena_sqlite_persistence_apply(&store,&command,&response));
 sql(db,"UPDATE cards SET archived=1 WHERE id='xa'");transfer_command(db,&command,rows,2,"xs","xt","xtl","xts",1,9000);
 assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"UPDATE cards SET archived=0 WHERE id='xa'");
 transfer_command(db,&command,rows,2,"xs","xt","xtl","xts",1,9000);
 sql(db,"PRAGMA foreign_keys=OFF;UPDATE card_descriptions SET board_id='xt' WHERE card_id='xa';PRAGMA foreign_keys=ON");
 assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"UPDATE card_descriptions SET board_id='xs' WHERE card_id='xa'");
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE card_descriptions SET description=CAST(description AS BLOB) WHERE card_id='xa';PRAGMA ignore_check_constraints=OFF");
 assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"UPDATE card_descriptions SET description='Description' WHERE card_id='xa'");
 transfer_original(db);
 for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){
  sql(db,triggers[i]);assert(!wena_sqlite_persistence_apply(&store,&command,&response));sql(db,"DROP TRIGGER cross_fail");transfer_original(db);
 }
 store.prepare_publish=transfer_reject_publish;assert(!wena_sqlite_persistence_apply(&store,&command,&response));store.prepare_publish=NULL;transfer_original(db);
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);assert(!wena_sqlite_persistence_apply(&store,&command,&response)&&calls==1);sqlite3_commit_hook(db,NULL,NULL);transfer_original(db);
 assert(wena_sqlite_persistence_apply(&store,&command,&response));
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='xt' AND version=2")==2&&number(db,"SELECT position FROM cards WHERE id='xb'")==1&&number(db,"SELECT position FROM cards WHERE id='xa'")==2);
 assert(number(db,"SELECT position FROM cards WHERE id='xd0'")==0&&number(db,"SELECT position FROM cards WHERE id='xd1'")==3&&number(db,"SELECT position FROM cards WHERE id='xu'")==2);
 assert(number(db,"SELECT count(*) FROM card_descriptions WHERE board_id='xt'")==2&&number(db,"SELECT archived_at FROM card_archive_state WHERE card_id='xa'")==123);
 assert(number(db,"SELECT count(*) FROM checklists WHERE board_id='xt' AND version=2 AND hide_all_items=1 AND position=7")==1);
 assert(number(db,"SELECT count(*) FROM checklist_items WHERE board_id='xt' AND version=2 AND is_finished=1 AND position=9")==1);
 assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='xt' AND label_id IN('d0','d1')")==4);
 assert(number(db,"SELECT count(*) FROM actor_card_sections WHERE card_id='xa' AND collapsed=1 AND version=1")==1);
 assert(number(db,"SELECT count(*) FROM boards WHERE id IN('xs','xt') AND version=2")==2);
 assert(!wena_sqlite_persistence_apply(&store,&command,&response));
 assert(number(db,"SELECT count(*) FROM idempotency_keys WHERE operation='transfer-selected-cards'")==1);
 assert(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0);
}

static int transfer_capture_race(unsigned int event,void *context,void *statement,void *text)
{
 Race *race;const char *query;(void)text;race=(Race*)context;query=sqlite3_sql((sqlite3_stmt*)statement);
 if(event==SQLITE_TRACE_STMT&&!race->fired&&query&&strstr(query,"SELECT id,board_id,list_id,version,swimlane_id FROM cards WHERE id=")){
  race->fired=1;sql(race->writer,"UPDATE cards SET position=3 WHERE id='xu';UPDATE boards SET version=3 WHERE id='xs'");
 }
 return 0;
}
static void native_cross_board(sqlite3 *db,const char *path)
{
 WenaHierarchyMutation adapter;WenaHierarchyTransfer transfer;WenaCardTransferSelection *capture,*saved,*original;
 WenaSqliteBoardSnapshot *source,*destination,*before_source,*before_destination,*fresh;WenaId ids[2];
 Race race;size_t source_count,target_count;int calls;
 source=(WenaSqliteBoardSnapshot*)malloc(sizeof(*source));destination=(WenaSqliteBoardSnapshot*)malloc(sizeof(*destination));
 before_source=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before_source));before_destination=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before_destination));
 fresh=(WenaSqliteBoardSnapshot*)malloc(sizeof(*fresh));original=(WenaCardTransferSelection*)malloc(sizeof(*original));
 assert(source&&destination&&before_source&&before_destination&&fresh&&original);
 assert(wena_sqlite_board_load(db,"xt",source)&&wena_sqlite_board_load(db,"xs",destination));
 assert(wena_hierarchy_mutation_init(&adapter,db,"u","xt",source));
 assert(!wena_hierarchy_transfer_init(&transfer,&adapter,source));assert(wena_hierarchy_transfer_init(&transfer,&adapter,destination));
 source_count=source->card_count;target_count=destination->card_count;adapter.published_card_count=&source_count;transfer.published_card_count=&target_count;
 *before_source=*source;*before_destination=*destination;capture=NULL;strcpy(ids[0],"xb");strcpy(ids[1],"xa");
 source->card_count=destination->card_count=0;source_count=target_count=0;strcpy(source->lists[0].title,"Stale");
 assert(sqlite3_open(path,&race.writer)==SQLITE_OK);race.fired=0;
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,transfer_capture_race,&race)==SQLITE_OK);
 assert(wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xs",&capture));
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK&&race.fired);
 assert(capture->source_board_version==2&&capture->target_board_version==2&&capture->source.count==2&&!strcmp(capture->source.cards[0].id,"xb"));
 assert(!memcmp(source,before_source,sizeof(*source))&&!memcmp(destination,before_destination,sizeof(*destination))&&source_count==4&&target_count==1);
 assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));
 sql(race.writer,"UPDATE cards SET position=2 WHERE id='xu';UPDATE boards SET version=2 WHERE id='xs'");assert(sqlite3_close(race.writer)==SQLITE_OK);
 *original=*capture;saved=capture;
 assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"missing",&capture));
 assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xt",&capture));
 strcpy(adapter.actor_id,"missing");assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xs",&capture));strcpy(adapter.actor_id,"u");
 transfer.published_card_count=&source_count;assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xs",&capture));transfer.published_card_count=&target_count;
 calls=0;assert(sqlite3_set_authorizer(db,move_capture_deny_commit,&calls)==SQLITE_OK);
 assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xs",&capture)&&calls==1);
 assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK&&sqlite3_get_autocommit(db));
 sql(db,"BEGIN");assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xs",&capture)&&!sqlite3_get_autocommit(db));sql(db,"ROLLBACK");
 assert(saved==capture&&!memcmp(capture,original,sizeof(*capture))&&!memcmp(source,before_source,sizeof(*source))&&
  !memcmp(destination,before_destination,sizeof(*destination))&&source_count==4&&target_count==1);
 capture->target_fingerprint[64]='x';assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));*capture=*original;
 strcpy(destination->board.id,"other");assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));*destination=*before_destination;
 adapter.persistence.prepare_publish=transfer_reject_publish;adapter.persistence.publish_context=&calls;
 assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500)&&adapter.persistence.prepare_publish==transfer_reject_publish&&adapter.persistence.publish_context==&calls);
 adapter.persistence.prepare_publish=NULL;adapter.persistence.publish_context=NULL;
 sql(db,"PRAGMA ignore_check_constraints=ON;INSERT INTO swimlane_colors VALUES('xss','xs','invalid');PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,2,"xs",&capture));
 assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));sql(db,"DELETE FROM swimlane_colors WHERE swimlane_id='xss'");
 sql(db,"CREATE TRIGGER transfer_native_late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));sql(db,"DROP TRIGGER transfer_native_late");
 calls=0;sqlite3_commit_hook(db,reject_commit,&calls);assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500)&&calls==1);sqlite3_commit_hook(db,NULL,NULL);
 assert(saved==capture&&!memcmp(capture,original,sizeof(*capture))&&!memcmp(source,before_source,sizeof(*source))&&
  !memcmp(destination,before_destination,sizeof(*destination))&&source_count==4&&target_count==1);
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='xt' AND version=2")==2);
 source->card_count=destination->card_count=0;source_count=target_count=0;
 assert(wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));
 assert(source_count==2&&target_count==3&&!memcmp(capture,original,sizeof(*capture)));
 assert(wena_sqlite_board_load(db,"xt",fresh)&&!memcmp(source,fresh,sizeof(*source)));
 assert(wena_sqlite_board_load(db,"xs",fresh)&&!memcmp(destination,fresh,sizeof(*destination)));
 *before_source=*source;*before_destination=*destination;
 assert(!wena_hierarchy_transfer_request(&transfer,capture,"xsl","xss",1,9500));
 assert(!memcmp(source,before_source,sizeof(*source))&&!memcmp(destination,before_destination,sizeof(*destination)));
 strcpy(ids[0],"xd0");assert(wena_hierarchy_transfer_load(&transfer,"xt",(const WenaId*)ids,1,"xs",&capture));
 assert(wena_hierarchy_transfer_save(&transfer,capture,"xsl","xss",3)&&source_count==1&&target_count==4);
 assert(wena_sqlite_board_load(db,"xs",fresh)&&!memcmp(destination,fresh,sizeof(*destination)));
 free(capture);free(original);free(source);free(destination);free(before_source);free(before_destination);free(fresh);
}

int main(int argc,char **argv)
{
 FILE *f;unsigned char *migration;long length;char hash[65],path[1024];sqlite3 *db;WenaSqlitePersistence store;
 sqlite3_int64 at,prior;int archived;size_t i;
 const char *triggers[]={
 "CREATE TRIGGER failure BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END",
 "CREATE TRIGGER failure BEFORE INSERT ON swimlane_archive_state BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER failure AFTER INSERT ON swimlane_archive_state BEGIN UPDATE swimlane_archive_state SET archived=0;END",
 "CREATE TRIGGER failure BEFORE UPDATE ON swimlanes BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER failure BEFORE UPDATE ON cards WHEN OLD.id='z' BEGIN SELECT RAISE(ABORT,'second card');END",
 "CREATE TRIGGER failure AFTER INSERT ON swimlane_archive_state BEGIN UPDATE cards SET archived=0 WHERE id='a';END",
 "CREATE TRIGGER failure AFTER INSERT ON swimlane_archive_state BEGIN UPDATE cards SET swimlane_id='t' WHERE id='a';END"};
 assert(argc==3);f=fopen(argv[1],"rb");assert(f&&!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);rewind(f);
 migration=(unsigned char*)malloc((size_t)length);assert(migration&&fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
 wena_sha256_hex(migration,(size_t)length,hash);sprintf(path,"%s/lane.sqlite",argv[2]);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1),('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1);"
 "INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('t','b','Other lane',1,1),('empty','b','Empty',2,1),('foreign','other','Foreign',0,1);"
 "INSERT INTO cards VALUES('a','b','s','l','A',0,0,1),('pre','b','s','l','Pre-archived',1,1,7),('z','b','s','l','Z',2,0,1),('other','b','t','l','Other',9,0,1);"
 "INSERT INTO card_archive_state VALUES('pre','b',9000000000000)");
 wena_sqlite_persistence_init(&store,db);
 assert(change(&store,"s",1,1,0));original(db);
 assert(!change(&store,"s",2,1,1));assert(!change(&store,"s",0,1,1));assert(!change(&store,"foreign",1,1,1));assert(!change(&store,"missing",1,1,1));
 sql(db,"DELETE FROM actors");assert(!change(&store,"s",1,1,1));sql(db,"INSERT INTO actors VALUES('u','User',1)");
 sql(db,"PRAGMA query_only=ON");assert(!change(&store,"s",1,1,1));sql(db,"PRAGMA query_only=OFF");
 for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){sql(db,triggers[i]);assert(!change(&store,"s",1,1,1));sql(db,"DROP TRIGGER failure");original(db);}
 assert(change(&store,"s",1,1,1));assert(wena_sqlite_swimlane_state_read(db,"b","s",2,&archived,&at)&&archived);prior=at;snapshot_state(db,1,0);destination_guards(db,&store);
 assert(at>number(db,"SELECT archived_at FROM card_archive_state WHERE card_id='pre'"));
 assert(number(db,"SELECT count(*) FROM card_archive_state WHERE card_id IN ('a','z') AND archived_at>(SELECT archived_at FROM swimlane_archive_state WHERE swimlane_id='s')")==2);
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=1")==3);
 assert(number(db,"SELECT version FROM cards WHERE id='pre'")==7&&number(db,"SELECT version FROM cards WHERE id='other'")==1);
 assert(!change(&store,"s",2,1,1));assert(change(&store,"s",2,2,1));assert(number(db,"SELECT count(*) FROM idempotency_keys")==1);
 sql(db,"INSERT INTO list_wip_limits VALUES('l','b',2,1,0)");assert(!change(&store,"s",2,1,0));
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=1")==3&&number(db,"SELECT version FROM swimlanes WHERE id='s'")==2);
 sql(db,"UPDATE list_wip_limits SET soft=1");assert(change(&store,"s",2,1,0));
 assert(wena_sqlite_swimlane_state_read(db,"b","s",3,&archived,&at)&&!archived&&at==prior);snapshot_state(db,0,2);
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=0 AND version=3")==2);
 assert(number(db,"SELECT archived FROM cards WHERE id='pre'")==1&&number(db,"SELECT version FROM cards WHERE id='pre'")==7);
 assert(number(db,"SELECT version FROM lists WHERE id='l'")==1);
 assert(change(&store,"s",3,2,1));assert(wena_sqlite_swimlane_state_read(db,"b","s",4,&archived,&at)&&at>prior);
 assert(change(&store,"s",4,2,0));assert(number(db,"SELECT version FROM cards WHERE id='pre'")==7);
 assert(change(&store,"empty",1,3,1));assert(change(&store,"empty",2,3,0));
 /* Unknown legacy archive timestamps must not restore a previously hidden card. */
 sql(db,"INSERT INTO swimlanes VALUES('old','b','Old',3,1);INSERT INTO swimlane_archive_state VALUES('old','b',1,0);INSERT INTO cards VALUES('legacy','b','old','l','Legacy',0,1,1)");
 assert(change(&store,"old",1,4,0));assert(number(db,"SELECT archived FROM cards WHERE id='legacy'")==1);
 at=77;archived=9;assert(!wena_sqlite_swimlane_state_read(db,"other","s",0,&archived,&at)&&at==77&&archived==9);
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE swimlane_archive_state SET archived_at=x'31' WHERE swimlane_id='s';PRAGMA ignore_check_constraints=OFF");
 assert(!change(&store,"s",5,4,1));assert(!wena_sqlite_swimlane_state_read(db,"b","s",5,&archived,&at)&&at==77&&archived==9);
 sql(db,"UPDATE swimlane_archive_state SET archived_at=0 WHERE swimlane_id='s'");
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));wena_sqlite_persistence_init(&store,db);
 assert(wena_sqlite_swimlane_state_read(db,"b","s",5,&archived,&at)&&!archived);
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=0 AND version=5")==2);
 native_archive(db);
 list_cards_tests(db,&store);
 native_list_cards(db);
 selected_archive_tests(db,&store);
 native_selected_archive(db,path);
 native_selected_move(db,path);
 cross_board_selected(db);
 cross_board_capacity(db);
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));wena_sqlite_persistence_init(&store,db);
 assert(number(db,"SELECT count(*) FROM cards WHERE list_id='batch' AND archived=1")==5);
 assert(number(db,"SELECT count(*) FROM cards WHERE list_id='ns' AND version=3")==2);
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='xt' AND version=2")==2);
 assert(number(db,"SELECT count(*) FROM checklist_items WHERE board_id='xt' AND version=2")==1);
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='ft' AND version=2")==2048);
 assert(number(db,"SELECT version FROM swimlanes WHERE id='s'")==9);snapshot_state(db,0,2);
 native_cross_board(db,path);
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));wena_sqlite_persistence_init(&store,db);
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='xs'")==4&&number(db,"SELECT count(*) FROM cards WHERE board_id='xt'")==1);
 assert(number(db,"SELECT version FROM cards WHERE id='xa'")==3);
 snapshot_tests(db,path);
 sql(db,"WITH RECURSIVE n(x) AS (SELECT 0 UNION ALL SELECT x+1 FROM n WHERE x<2048) INSERT INTO cards SELECT 'bulk'||x,'b','empty','l','Bulk',x,0,1 FROM n");
 assert(!list_cards(&store,"listId=l&expectedVersion=1",400));
 assert(!change(&store,"empty",3,4,1));assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='empty' AND archived=0 AND version=1")==2049);
 assert(number(db,"SELECT version FROM swimlanes WHERE id='empty'")==3);full_selection(db,&store);sql(db,"DELETE FROM cards WHERE swimlane_id='empty' AND id GLOB 'bulk*'");
 sql(db,"DROP TABLE swimlane_archive_state");assert(!change(&store,"s",9,4,1));
 sql(db,"CREATE VIEW swimlane_archive_state AS SELECT 's' AS swimlane_id,'b' AS board_id,0 AS archived,0 AS archived_at");assert(!change(&store,"s",9,4,1));
 assert(sqlite3_close(db)==SQLITE_OK);free(migration);
 puts("Swimlane cascades: timestamp separation, prior archives, WIP rollback, no-op, replay, corruption and reopen passed");return 0;
}
