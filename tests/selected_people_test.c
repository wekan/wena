#include "../server/sqlite_persistence.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q)
{int rc;rc=sqlite3_exec(db,q,NULL,NULL,NULL);if(rc!=SQLITE_OK)fprintf(stderr,"%s: %s\n",q,sqlite3_errmsg(db));assert(rc==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static int apply(WenaSqlitePersistence *store,const WenaCardRevision *cards,size_t count,const char *board,
 const char *actor,const char *form,unsigned long request)
{
 WenaDomainCommand command;WenaRegionResponse response;int ok;
 memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_SET_SELECTED_PERSON;command.request_version=request;
 strcpy(command.user_id,actor);sprintf(command.route,"/b/%s/native",board);strcpy(command.form_body,form);command.form_body_length=strlen(form);
 command.selected_cards=cards;command.selected_card_count=count;ok=wena_sqlite_persistence_apply(store,&command,&response);
 if(!ok){WenaRegionResponse empty;memset(&empty,0,sizeof(empty));assert(!memcmp(&response,&empty,sizeof(empty)));}
 else assert(response.region_count==1&&!strcmp(response.regions[0].name,"board"));
 return ok;
}
#define ADD "personId=a&field=members&enabled=1&expectedBoardVersion=1"
static void original(sqlite3 *db)
{
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==0);
 assert(number(db,"SELECT sum(version) FROM cards WHERE board_id='b'")==3);
 assert(number(db,"SELECT version FROM boards WHERE id='b'")==1);
 assert(number(db,"SELECT count(*) FROM card_people")==2);
 assert(number(db,"SELECT position FROM card_people WHERE card_id='c0' AND field='members'")==5);
 assert(number(db,"SELECT count(*) FROM board_members WHERE board_id='b' AND active=1")==2);
 assert(number(db,"SELECT count(*) FROM actors WHERE display_name='Alice'")==1);
}
static void triggered_as(sqlite3 *db,WenaSqlitePersistence *store,const WenaCardRevision *cards,const char *trigger,const char *form)
{sql(db,trigger);assert(!apply(store,cards,2,"b","u",form,1));original(db);sql(db,"DROP TRIGGER fail_people");}
static void triggered(sqlite3 *db,WenaSqlitePersistence *store,const WenaCardRevision *cards,const char *trigger)
{triggered_as(db,store,cards,trigger,ADD);}
static int no_commit(void *unused){(void)unused;return 1;}
static int no_publish(void *unused,sqlite3 *db){(void)unused;assert(!sqlite3_get_autocommit(db));return 0;}
int main(int argc,char **argv)
{
 sqlite3 *db;WenaSqlitePersistence store;WenaCardRevision cards[2],*many;unsigned char *bundle;
 FILE *file;long length;char hash[65],path[1024],query[512];size_t i;int before;
 assert(argc==3&&strlen(argv[2])<900);sprintf(path,"%s/selected-people.sqlite",argv[2]);
 file=fopen(argv[1],"rb");assert(file&&!fseek(file,0,SEEK_END));length=ftell(file);assert(length>0);rewind(file);
 bundle=(unsigned char*)malloc((size_t)length);assert(bundle&&fread(bundle,1,(size_t)length,file)==(size_t)length&&!fclose(file));
 wena_sha256_hex(bundle,(size_t)length,hash);assert(wena_sqlite_open(path,bundle,(size_t)length,hash,&db));wena_sqlite_persistence_init(&store,db);
 sql(db,"INSERT INTO boards VALUES('b','Board',1),('other','Other',1);INSERT INTO actors VALUES('u','User',1),('a','Alice',1),('z','Zed',1),('i','Inactive',1);"
  "INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1)");
 sql(db,"INSERT INTO cards VALUES('c0','b','s','l','First',0,0,1),('c1','b','s','l','Second',1,0,1),('c2','b','s','l','Untouched',2,0,1);"
  "INSERT INTO board_members(board_id,actor_id,active) VALUES('b','a',1),('b','z',1),('b','i',0);"
  "INSERT INTO card_people VALUES('b','c0','members','a',5),('b','c0','assignees','z',9)");
 strcpy(cards[0].id,"c0");cards[0].version=1;strcpy(cards[1].id,"c1");cards[1].version=1;
 assert(!apply(&store,cards,0,"b","u",ADD,1));assert(!apply(&store,cards,2049,"b","u",ADD,1));assert(!apply(&store,NULL,2,"b","u",ADD,1));
 assert(!apply(&store,cards,2,"b","missing",ADD,1));assert(!apply(&store,cards,2,"other","u",ADD,1));
 assert(!apply(&store,cards,2,"b","u","personId=a&field=watchers&enabled=1&expectedBoardVersion=1",1));
 assert(!apply(&store,cards,2,"b","u","personId=a&field=members&enabled=true&expectedBoardVersion=1",1));
 assert(!apply(&store,cards,2,"b","u","personId=i&field=members&enabled=1&expectedBoardVersion=1",1));
 assert(!apply(&store,cards,2,"b","u","personId=missing&field=members&enabled=1&expectedBoardVersion=1",1));
 assert(!apply(&store,cards,2,"b","u","personId=a&field=members&enabled=1&expectedBoardVersion=2",1));
 cards[1].version=2;before=sqlite3_total_changes(db);assert(!apply(&store,cards,2,"b","u",ADD,1));assert(sqlite3_total_changes(db)==before);cards[1].version=1;
 strcpy(cards[1].id,"c0");assert(!apply(&store,cards,2,"b","u",ADD,1));strcpy(cards[1].id,"c1");
 sql(db,"UPDATE cards SET archived=1 WHERE id='c1'");before=sqlite3_total_changes(db);assert(!apply(&store,cards,2,"b","u",ADD,1));
 assert(sqlite3_total_changes(db)==before);sql(db,"UPDATE cards SET archived=0 WHERE id='c1'");
 sql(db,"INSERT INTO swimlane_archive_state VALUES('s','b',1,10)");assert(!apply(&store,cards,2,"b","u",ADD,1));sql(db,"DELETE FROM swimlane_archive_state");original(db);
 triggered(db,&store,cards,"CREATE TRIGGER fail_people BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people BEFORE INSERT ON card_people BEGIN SELECT RAISE(IGNORE);END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people AFTER INSERT ON card_people BEGIN DELETE FROM card_people WHERE card_id='c0' AND field='assignees';END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people AFTER UPDATE ON boards BEGIN UPDATE cards SET title='Changed' WHERE id='c0';END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people AFTER UPDATE ON boards BEGIN UPDATE card_people SET position=7 WHERE card_id='c0' AND field='members';END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people AFTER UPDATE ON boards BEGIN UPDATE board_members SET active=0 WHERE actor_id='a';END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people AFTER UPDATE ON boards BEGIN UPDATE actors SET display_name='Changed' WHERE id='a';END");
 triggered(db,&store,cards,"CREATE TRIGGER fail_people BEFORE UPDATE ON boards BEGIN SELECT RAISE(IGNORE);END");
 triggered_as(db,&store,cards,"CREATE TRIGGER fail_people AFTER UPDATE ON cards WHEN NEW.id='c0' BEGIN INSERT INTO card_people VALUES('b','c1','assignees','a',0);END",
  "personId=a&field=assignees&enabled=1&expectedBoardVersion=1");
 triggered_as(db,&store,cards,"CREATE TRIGGER fail_people AFTER INSERT ON card_people WHEN NEW.card_id='c1' BEGIN DELETE FROM card_people WHERE card_id='c0' AND actor_id='a' AND field='assignees';END",
  "personId=a&field=assignees&enabled=1&expectedBoardVersion=1");
 store.prepare_publish=no_publish;assert(!apply(&store,cards,2,"b","u",ADD,1));store.prepare_publish=NULL;original(db);
 sqlite3_commit_hook(db,no_commit,NULL);assert(!apply(&store,cards,2,"b","u",ADD,1));sqlite3_commit_hook(db,NULL,NULL);original(db);
 assert(apply(&store,cards,2,"b","u",ADD,1));assert(cards[0].version==1&&cards[1].version==1&&!strcmp(cards[1].id,"c1"));
 assert(number(db,"SELECT version FROM cards WHERE id='c0'")==1&&number(db,"SELECT version FROM cards WHERE id='c1'")==2);
 assert(number(db,"SELECT version FROM cards WHERE id='c2'")==1&&number(db,"SELECT version FROM boards WHERE id='b'")==2);
 assert(number(db,"SELECT count(*) FROM card_people WHERE field='members'")==2&&number(db,"SELECT position FROM card_people WHERE card_id='c0' AND field='members'")==5);
 assert(!apply(&store,cards,2,"b","u",ADD,1));cards[1].version=2;
 before=sqlite3_total_changes(db);
 assert(apply(&store,cards,2,"b","u","personId=a&field=members&enabled=1&expectedBoardVersion=2",2));
 assert(sqlite3_total_changes(db)==before&&number(db,"SELECT count(*) FROM idempotency_keys")==1);
 /* One-card span uses the identical operation for the other field. */
 assert(apply(&store,cards,1,"b","u","personId=a&field=assignees&enabled=1&expectedBoardVersion=2",2));
 cards[0].version=2;assert(number(db,"SELECT position FROM card_people WHERE card_id='c0' AND field='assignees' AND actor_id='a'")==10);
 sql(db,"UPDATE board_members SET active=0 WHERE actor_id='a'");
 assert(!apply(&store,cards,2,"b","u","personId=a&field=members&enabled=1&expectedBoardVersion=3",3));
 assert(apply(&store,cards,2,"b","u","personId=a&field=members&enabled=0&expectedBoardVersion=3",3));
 assert(number(db,"SELECT count(*) FROM card_people WHERE field='members'")==0&&number(db,"SELECT version FROM boards WHERE id='b'")==4);
 /* Full native selection remains bounded without retaining full snapshots. */
 many=(WenaCardRevision*)calloc(2048,sizeof(*many));assert(many);
 sql(db,"BEGIN;INSERT INTO boards VALUES('big','Big',1);INSERT INTO lists VALUES('bl','big','List',0,1);INSERT INTO swimlanes VALUES('bs','big','Lane',0,1);INSERT INTO board_members(board_id,actor_id) VALUES('big','a')");
 for(i=0;i<2048;++i){sprintf(many[i].id,"p%04lu",(unsigned long)i);many[i].version=1;
  sprintf(query,"INSERT INTO cards VALUES('%s','big','bs','bl','Card',%lu,0,1)",many[i].id,(unsigned long)i);sql(db,query);}
 sql(db,"COMMIT");assert(apply(&store,many,2048,"big","u","personId=a&field=assignees&enabled=1&expectedBoardVersion=1",100));
 assert(number(db,"SELECT count(*) FROM card_people WHERE board_id='big' AND field='assignees'")==2048);
 assert(number(db,"SELECT count(*) FROM cards WHERE board_id='big' AND version=2")==2048&&number(db,"SELECT version FROM boards WHERE id='big'")==2);
 assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
 assert(wena_sqlite_open(path,bundle,(size_t)length,hash,&db));assert(number(db,"SELECT count(*) FROM card_people WHERE board_id='big'")==2048);
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==4&&number(db,"SELECT version FROM boards WHERE id='b'")==4);
 assert(sqlite3_close(db)==SQLITE_OK);free(bundle);free(many);puts("Atomic native person selections, whole-batch verification, replay and capacity passed");return 0;
}
