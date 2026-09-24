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
 memcpy(before,after,sizeof(*before));sql(db,"DROP TABLE list_archive_state");
 assert(!wena_sqlite_board_load(db,"b",after)&&!memcmp(before,after,sizeof(*before)));
 assert(!change(&store,0,4,4,NULL));sqlite3_close(db);free(before);free(after);free(migration);puts("List archive transactions and snapshots: passed");return 0;
}
