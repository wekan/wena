#include "../server/mutations/card_archive.h"
#include "../server/sqlite_persistence.h"
#include "../server/sqlite_storage.h"
#include "../client/features/card_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static int change(WenaSqlitePersistence *store,const char *card,unsigned long version,unsigned long request,int archived)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));c.operation=archived?WENA_DOMAIN_ARCHIVE_CARD:WENA_DOMAIN_RESTORE_CARD;
 c.request_version=request;strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
 sprintf(c.form_body,"cardId=%s&expectedVersion=%lu",card,version);c.form_body_length=strlen(c.form_body);return wena_sqlite_persistence_apply(store,&c,&r);
}
int main(int argc,char **argv)
{
 FILE *f;unsigned char *migration;long length;char hash[65],path[1024];sqlite3 *db;WenaSqlitePersistence store;
 sqlite3_int64 at,prior,maximum;int archived;size_t i;WenaCard card;WenaCardMutation adapter;
 const char *triggers[]={
 "CREATE TRIGGER failure BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END",
 "CREATE TRIGGER failure BEFORE INSERT ON card_archive_state BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER failure AFTER INSERT ON card_archive_state BEGIN UPDATE card_archive_state SET archived_at=0;END",
 "CREATE TRIGGER failure BEFORE UPDATE ON cards BEGIN SELECT RAISE(IGNORE);END",
 "CREATE TRIGGER failure AFTER UPDATE ON cards BEGIN UPDATE cards SET version=version+1;END"};
 assert(argc==3);f=fopen(argv[1],"rb");assert(f&&!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);rewind(f);
 migration=(unsigned char*)malloc((size_t)length);assert(migration&&fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
 wena_sha256_hex(migration,(size_t)length,hash);sprintf(path,"%s/card.sqlite",argv[2]);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1),('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1);"
 "INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1),('old','b','s','l','Old archive',1,1,1)");
 wena_sqlite_persistence_init(&store,db);
 assert(!wena_sqlite_card_archive_change(db,"b","c",1,1,0));
 sql(db,"BEGIN IMMEDIATE");prior=number(db,"SELECT 9000000000000");
 assert(wena_sqlite_card_archive_change(db,"b","c",1,1,prior));
 assert(wena_sqlite_card_archive_read(db,"b","c",2,&archived,&at)&&at>prior);sql(db,"ROLLBACK");
 assert(wena_sqlite_card_archive_read(db,"b","c",1,&archived,&at)&&!archived&&!at);
 assert(change(&store,"old",1,1,0));assert(!number(db,"SELECT count(*) FROM card_archive_state"));
 for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){sql(db,triggers[i]);assert(!change(&store,"c",1,1,1));sql(db,"DROP TRIGGER failure");
  assert(number(db,"SELECT version FROM cards WHERE id='c'")==1&&!number(db,"SELECT archived FROM cards WHERE id='c'"));assert(!number(db,"SELECT count(*) FROM card_archive_state"));
 }
 assert(change(&store,"c",1,1,1));assert(wena_sqlite_card_archive_read(db,"b","c",2,&archived,&at)&&archived&&at>0);prior=at;
 assert(!change(&store,"c",2,1,1));assert(!change(&store,"c",1,2,0));assert(!change(&store,"c",2,1,0));
 assert(change(&store,"c",2,2,0));assert(wena_sqlite_card_archive_read(db,"b","c",3,&archived,&at)&&!archived&&at==prior);
 assert(change(&store,"c",3,2,1));assert(wena_sqlite_card_archive_read(db,"b","c",4,&archived,&at)&&archived&&at>prior);prior=at;
 assert(wena_card_init(&card,"c","b","s","l","Card",0,1));assert(wena_card_mutation_init(&adapter,db,"u","b",&card,1));
 sql(db,"CREATE TRIGGER failure AFTER UPDATE ON cards BEGIN UPDATE card_archive_state SET archived_at=0;END");
 assert(!wena_card_mutation_restore(&adapter,"b","c",4)&&card.archived);sql(db,"DROP TRIGGER failure");
 assert(wena_sqlite_card_archive_read(db,"b","c",4,&archived,&at)&&at==prior);
 assert(wena_card_mutation_restore(&adapter,"b","c",4)&&!card.archived);
 at=77;archived=9;assert(!wena_sqlite_card_archive_read(db,"other","c",0,&archived,&at)&&at==77&&archived==9);
 assert(!wena_sqlite_card_archive_read(db,"b","c",4,&archived,&at)&&at==77&&archived==9);
 sql(db,"PRAGMA foreign_keys=OFF;UPDATE card_archive_state SET board_id='other';PRAGMA foreign_keys=ON");
 assert(!change(&store,"c",5,3,1));assert(!wena_sqlite_card_archive_read(db,"b","c",5,&archived,&at)&&at==77&&archived==9);sql(db,"UPDATE card_archive_state SET board_id='b'");
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE card_archive_state SET archived_at=x'31';PRAGMA ignore_check_constraints=OFF");
 assert(!change(&store,"c",5,3,1));assert(!wena_sqlite_card_archive_read(db,"b","c",5,&archived,&at)&&at==77&&archived==9);
 sql(db,"UPDATE card_archive_state SET archived_at=9223372036854775807");maximum=number(db,"SELECT archived_at FROM card_archive_state");
 assert(!wena_sqlite_archive_time_after(db,maximum,&at)&&at==77);assert(!change(&store,"c",5,3,1));
 sql(db,"UPDATE card_archive_state SET archived_at=9000000000000");assert(change(&store,"c",5,3,1));
 assert(wena_sqlite_card_archive_read(db,"b","c",6,&archived,&at)&&archived&&at>number(db,"SELECT 9000000000000"));prior=at;
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&db));wena_sqlite_persistence_init(&store,db);
 assert(wena_sqlite_card_archive_read(db,"b","c",6,&archived,&at)&&archived&&at==prior);
 sql(db,"DROP TABLE card_archive_state");assert(!change(&store,"c",6,4,0));
 sql(db,"CREATE VIEW card_archive_state AS SELECT 'c' AS card_id,'b' AS board_id,0 AS archived_at");assert(!change(&store,"c",6,4,0));
 assert(sqlite3_close(db)==SQLITE_OK);free(migration);
 puts("Card archive metadata: guarded timestamps, retained restores, monotonicity, corruption, rollback and reopen passed");return 0;
}
