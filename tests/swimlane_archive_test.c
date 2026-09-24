#include "../server/list_state.h"
#include "../server/sqlite_persistence.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static int change(WenaSqlitePersistence *store,const char *lane,unsigned long version,unsigned long request,int archived)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));c.operation=archived?WENA_DOMAIN_ARCHIVE_SWIMLANE:WENA_DOMAIN_RESTORE_SWIMLANE;
 c.request_version=request;strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
 sprintf(c.form_body,"swimlaneId=%s&expectedVersion=%lu",lane,version);c.form_body_length=strlen(c.form_body);return wena_sqlite_persistence_apply(store,&c,&r);
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
 assert(change(&store,"s",1,1,1));assert(wena_sqlite_swimlane_state_read(db,"b","s",2,&archived,&at)&&archived);prior=at;
 assert(at>number(db,"SELECT archived_at FROM card_archive_state WHERE card_id='pre'"));
 assert(number(db,"SELECT count(*) FROM card_archive_state WHERE card_id IN ('a','z') AND archived_at>(SELECT archived_at FROM swimlane_archive_state WHERE swimlane_id='s')")==2);
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=1")==3);
 assert(number(db,"SELECT version FROM cards WHERE id='pre'")==7&&number(db,"SELECT version FROM cards WHERE id='other'")==1);
 assert(!change(&store,"s",2,1,1));assert(change(&store,"s",2,2,1));assert(number(db,"SELECT count(*) FROM idempotency_keys")==1);
 sql(db,"INSERT INTO list_wip_limits VALUES('l','b',2,1,0)");assert(!change(&store,"s",2,1,0));
 assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='s' AND archived=1")==3&&number(db,"SELECT version FROM swimlanes WHERE id='s'")==2);
 sql(db,"UPDATE list_wip_limits SET soft=1");assert(change(&store,"s",2,1,0));
 assert(wena_sqlite_swimlane_state_read(db,"b","s",3,&archived,&at)&&!archived&&at==prior);
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
 sql(db,"WITH RECURSIVE n(x) AS (SELECT 0 UNION ALL SELECT x+1 FROM n WHERE x<2048) INSERT INTO cards SELECT 'bulk'||x,'b','empty','l','Bulk',x,0,1 FROM n");
 assert(!change(&store,"empty",3,4,1));assert(number(db,"SELECT count(*) FROM cards WHERE swimlane_id='empty' AND archived=0 AND version=1")==2049);
 assert(number(db,"SELECT version FROM swimlanes WHERE id='empty'")==3);sql(db,"DELETE FROM cards WHERE swimlane_id='empty'");
 sql(db,"DROP TABLE swimlane_archive_state");assert(!change(&store,"s",5,4,1));
 sql(db,"CREATE VIEW swimlane_archive_state AS SELECT 's' AS swimlane_id,'b' AS board_id,0 AS archived,0 AS archived_at");assert(!change(&store,"s",5,4,1));
 assert(sqlite3_close(db)==SQLITE_OK);free(migration);
 puts("Swimlane cascades: timestamp separation, prior archives, WIP rollback, no-op, replay, corruption and reopen passed");return 0;
}
