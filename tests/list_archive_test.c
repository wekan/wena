#include "../client/features/hierarchy_mutation.h"
#include "../server/sqlite_persistence.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);sqlite3_finalize(s);return n;}
static int change(WenaSqlitePersistence *store,int archived,unsigned long version,unsigned long request,const char *extra)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));c.operation=archived?WENA_DOMAIN_ARCHIVE_LIST:WENA_DOMAIN_RESTORE_LIST;
 c.request_version=request;strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
 sprintf(c.form_body,"listId=l&expectedVersion=%lu%s",version,extra?extra:"");c.form_body_length=strlen(c.form_body);
 return wena_sqlite_persistence_apply(store,&c,&r);
}
static int operation(WenaSqlitePersistence *store,WenaDomainOperation kind,const char *body)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));c.operation=kind;c.request_version=100;
 strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");strcpy(c.form_body,body);c.form_body_length=strlen(body);
 return wena_sqlite_persistence_apply(store,&c,&r);
}
static void blocked_operations(WenaSqlitePersistence *store)
{
 sqlite3_int64 keys;keys=number(store->database,"SELECT count(*) FROM idempotency_keys");
 assert(!operation(store,WENA_DOMAIN_CREATE_CARD,"title=Blocked&targetListId=l&targetSwimlaneId=s"));
 assert(!operation(store,WENA_DOMAIN_MOVE_CARD,"cardId=live&expectedVersion=1&targetListId=l&targetSwimlaneId=s"));
 assert(!operation(store,WENA_DOMAIN_MOVE_CARD,"cardId=live&expectedVersion=1&targetListId=l&targetSwimlaneId=s&insertPosition=0"));
 assert(!operation(store,WENA_DOMAIN_MOVE_CARD,"cardId=active&expectedVersion=1&targetListId=open&targetSwimlaneId=s"));
 assert(!operation(store,WENA_DOMAIN_MOVE_CARD,"cardId=active&expectedVersion=1&targetListId=l&targetSwimlaneId=s&targetPosition=0"));
 assert(!operation(store,WENA_DOMAIN_MOVE_LIST,"listId=l&expectedVersion=4&targetPosition=1"));
 assert(number(store->database,"SELECT count(*) FROM idempotency_keys")==keys);
 assert(number(store->database,"SELECT version FROM cards WHERE id='live'")==1);
 assert(number(store->database,"SELECT version FROM cards WHERE id='active'")==1);
}
static void native_adapter(const unsigned char *migration,size_t length,const char *hash,const char *directory)
{
 char path[1024];sqlite3 *db;WenaHierarchyMutation adapter;
 WenaSqliteBoardSnapshot *snapshot,*before,*fresh;unsigned long version;sqlite3_int64 at,keys;
 sprintf(path,"%s/native-archive.sqlite",directory);assert(wena_sqlite_open(path,migration,length,hash,&db));
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Active',0,0,1),('a','b','s','l','Archived',2,1,7)");
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));fresh=(WenaSqliteBoardSnapshot*)malloc(sizeof(*fresh));assert(snapshot&&before&&fresh);
 assert(wena_sqlite_board_load(db,"b",snapshot));assert(wena_hierarchy_mutation_init(&adapter,db,"u","b",snapshot));
 assert(wena_hierarchy_mutation_archive_load(&adapter,"b","l",&version)&&version==1);
 memcpy(before,snapshot,sizeof(*before));
 assert(!wena_hierarchy_mutation_archive(&adapter,"other","l",1));
 assert(!wena_hierarchy_mutation_archive(&adapter,"b","missing",1));
 assert(!wena_hierarchy_mutation_archive(&adapter,"b","l",0));
 assert(!wena_hierarchy_mutation_archive_request(&adapter,"b","l",1,0,1));
 assert(!wena_hierarchy_mutation_archive_request(&adapter,"b","l",1,1,2));
 strcpy(adapter.actor_id,"missing");version=88;
 assert(!wena_hierarchy_mutation_archive_load(&adapter,"b","l",&version)&&version==88);
 assert(!wena_hierarchy_mutation_archive(&adapter,"b","l",1));strcpy(adapter.actor_id,"u");
 sql(db,"CREATE TRIGGER late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!wena_hierarchy_mutation_archive(&adapter,"b","l",1));sql(db,"DROP TRIGGER late");
 assert(!memcmp(snapshot,before,sizeof(*before))&&!number(db,"SELECT count(*) FROM list_archive_state"));
 assert(wena_hierarchy_mutation_archive_request(&adapter,"b","l",1,50,1));before->lists[0].archived=1;
 assert(!memcmp(snapshot,before,sizeof(*before)));at=number(db,"SELECT archived_at FROM list_archive_state");
 assert(wena_hierarchy_mutation_archive_load(&adapter,"b","l",&version)&&version==2);
 keys=number(db,"SELECT count(*) FROM idempotency_keys");
 assert(!wena_hierarchy_mutation_archive_request(&adapter,"b","l",2,50,1));
 assert(wena_hierarchy_mutation_archive(&adapter,"b","l",2));assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
 assert(!wena_hierarchy_mutation_restore(&adapter,"b","l",1));assert(!memcmp(snapshot,before,sizeof(*before)));
 assert(wena_hierarchy_mutation_restore(&adapter,"b","l",2));before->lists[0].archived=0;
 assert(!memcmp(snapshot,before,sizeof(*before))&&number(db,"SELECT archived_at FROM list_archive_state")==at);
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*snapshot)));
 /* Model corruption and stale archive flags cannot supply a selection. */
 snapshot->lists[1]=snapshot->lists[0];snapshot->list_count=2;
 assert(!wena_hierarchy_mutation_archive(&adapter,"b","l",3));memcpy(snapshot,before,sizeof(*snapshot));
 snapshot->lists[0].archived=2;assert(!wena_hierarchy_mutation_archive(&adapter,"b","l",3));memcpy(snapshot,before,sizeof(*snapshot));
 snapshot->lists[0].archived=1;version=88;
 assert(!wena_hierarchy_mutation_archive_load(&adapter,"b","l",&version)&&version==88);memcpy(snapshot,before,sizeof(*snapshot));
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE list_archive_state SET archived=2;PRAGMA ignore_check_constraints=OFF");
 assert(!wena_hierarchy_mutation_archive_load(&adapter,"b","l",&version)&&version==88);
 assert(!wena_hierarchy_mutation_archive(&adapter,"b","l",3)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 sql(db,"UPDATE list_archive_state SET archived=0");
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,length,hash,&db));
 assert(wena_hierarchy_mutation_init(&adapter,db,"u","b",snapshot));
 assert(wena_hierarchy_mutation_archive(&adapter,"b","l",3));
 assert(wena_hierarchy_mutation_archive_load(&adapter,"b","l",&version)&&version==4);
 assert(wena_sqlite_board_load(db,"b",fresh)&&!memcmp(snapshot,fresh,sizeof(*snapshot)));
 sqlite3_close(db);free(snapshot);free(before);free(fresh);
}
int main(int argc,char **argv)
{
 FILE *f;unsigned char *migration;long length;char hash[65],path[1024];sqlite3 *db;
 WenaSqlitePersistence store;WenaDomainCommand forged;WenaRegionResponse response;WenaSqliteBoardSnapshot *before,*after;sqlite3_int64 at;
 assert(argc==3);f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);rewind(f);
 migration=(unsigned char*)malloc((size_t)length);assert(migration&&fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
 wena_sha256_hex(migration,(size_t)length,hash);sprintf(path,"%s/archive.sqlite",argv[2]);
 assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1),('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1),('x','other','Foreign',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1)");
 sql(db,"INSERT INTO cards VALUES('active','b','s','l','Active',0,0,1),('archived','b','s','l','Archived',5,1,8)");
 before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));after=(WenaSqliteBoardSnapshot*)malloc(sizeof(*after));assert(before&&after);
 assert(wena_sqlite_board_load(db,"b",before)&&!before->lists[0].archived);
 wena_sqlite_persistence_init(&store,db);
 assert(change(&store,0,1,1,NULL));assert(!number(db,"SELECT count(*) FROM idempotency_keys"));assert(!number(db,"SELECT count(*) FROM list_archive_state"));
 memset(&forged,0,sizeof(forged));forged.operation=WENA_DOMAIN_ARCHIVE_LIST;forged.request_version=1;
 strcpy(forged.user_id,"u");strcpy(forged.route,"/b/b/native");strcpy(forged.form_body,"listId=x&expectedVersion=1");forged.form_body_length=strlen(forged.form_body);
 assert(!wena_sqlite_persistence_apply(&store,&forged,&response));
 sql(db,"DELETE FROM actors WHERE id='u'");assert(!change(&store,1,1,1,NULL));sql(db,"INSERT INTO actors VALUES('u','User',1)");
 sql(db,"PRAGMA query_only=ON");assert(!change(&store,1,1,1,NULL));sql(db,"PRAGMA query_only=OFF");
 assert(!change(&store,1,0,1,NULL));assert(!change(&store,1,99,1,NULL));assert(!change(&store,1,1,1,"&listId=x"));
 sql(db,"CREATE TRIGGER reject_archive BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!change(&store,1,1,1,NULL));assert(!number(db,"SELECT count(*) FROM list_archive_state"));assert(number(db,"SELECT version FROM lists WHERE id='l'")==1);
 sql(db,"DROP TRIGGER reject_archive;CREATE TRIGGER alter_archive AFTER INSERT ON list_archive_state BEGIN UPDATE list_archive_state SET archived=0;END");
 assert(!change(&store,1,1,1,NULL));assert(!number(db,"SELECT count(*) FROM list_archive_state"));sql(db,"DROP TRIGGER alter_archive");
 sql(db,"CREATE TRIGGER ignore_version BEFORE UPDATE ON lists BEGIN SELECT RAISE(IGNORE);END");
 assert(!change(&store,1,1,1,NULL));assert(!number(db,"SELECT count(*) FROM list_archive_state"));sql(db,"DROP TRIGGER ignore_version");
 assert(change(&store,1,1,1,NULL));at=number(db,"SELECT archived_at FROM list_archive_state");assert(at>0);
 assert(number(db,"SELECT version FROM lists WHERE id='l'")==2);assert(wena_sqlite_board_load(db,"b",after)&&after->lists[0].archived);
 assert(before->card_count==after->card_count&&!memcmp(before->cards,after->cards,before->card_count*sizeof(WenaCard)));
 assert(!change(&store,1,2,1,NULL));assert(change(&store,1,2,2,NULL));assert(number(db,"SELECT count(*) FROM idempotency_keys")==1);
 assert(!change(&store,0,1,2,NULL));assert(change(&store,0,2,2,NULL));assert(number(db,"SELECT archived_at FROM list_archive_state")==at);
 assert(number(db,"SELECT version FROM lists WHERE id='l'")==3);assert(wena_sqlite_board_load(db,"b",after)&&!after->lists[0].archived);
 assert(!memcmp(before->cards,after->cards,before->card_count*sizeof(WenaCard)));
 memcpy(before,after,sizeof(*before));
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE list_archive_state SET archived=2;PRAGMA ignore_check_constraints=OFF");
 assert(!wena_sqlite_board_load(db,"b",after)&&!memcmp(before,after,sizeof(*before)));assert(!change(&store,1,3,3,NULL));
 sql(db,"UPDATE list_archive_state SET archived=0;PRAGMA foreign_keys=OFF;UPDATE list_archive_state SET board_id='other'");
 assert(!wena_sqlite_board_load(db,"b",after)&&!memcmp(before,after,sizeof(*before)));assert(!change(&store,1,3,3,NULL));
 sql(db,"UPDATE list_archive_state SET board_id='b';PRAGMA foreign_keys=ON");
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));
 wena_sqlite_persistence_init(&store,db);assert(change(&store,1,3,3,NULL));
 assert(wena_sqlite_board_load(db,"b",after)&&after->lists[0].archived);
 assert(!memcmp(before->cards,after->cards,before->card_count*sizeof(WenaCard)));
 sql(db,"INSERT INTO lists VALUES('open','b','Open',1,1);INSERT INTO cards VALUES('live','b','s','open','Live',0,0,1)");
 blocked_operations(&store);
 assert(operation(&store,WENA_DOMAIN_CREATE_CARD,"title=Legacy"));
 assert(number(db,"SELECT count(*) FROM cards WHERE title='Legacy' AND list_id='open'")==1);
 assert(number(db,"SELECT count(*) FROM cards WHERE list_id='l'")==2);
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE list_archive_state SET archived=2;PRAGMA ignore_check_constraints=OFF");blocked_operations(&store);
 sql(db,"UPDATE list_archive_state SET archived=1;PRAGMA foreign_keys=OFF;UPDATE list_archive_state SET board_id='other'");blocked_operations(&store);
 sql(db,"UPDATE list_archive_state SET board_id='b';PRAGMA foreign_keys=ON");
 assert(wena_sqlite_board_load(db,"b",after));
 memcpy(before,after,sizeof(*before));sql(db,"DROP TABLE list_archive_state");
 assert(!wena_sqlite_board_load(db,"b",after)&&!memcmp(before,after,sizeof(*before)));
 blocked_operations(&store);
 assert(!change(&store,0,4,4,NULL));sqlite3_close(db);free(before);free(after);native_adapter(migration,(size_t)length,hash,argv[2]);free(migration);puts("List archive transactions and snapshots: passed");return 0;
}
