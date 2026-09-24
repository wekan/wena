#include "../server/sqlite_board.h"
#include "../server/sqlite_persistence.h"
#include "../server/sqlite_storage.h"
#include "../models/color.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);sqlite3_finalize(s);return n;}
static int change(WenaSqlitePersistence *store,int lists,const char *id,unsigned long version,unsigned long request,const char *color,const char *extra)
{
 WenaDomainCommand c;WenaRegionResponse r;memset(&c,0,sizeof(c));
 c.operation=lists?WENA_DOMAIN_SET_LIST_COLOR:WENA_DOMAIN_SET_SWIMLANE_COLOR;c.request_version=request;
 strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
 sprintf(c.form_body,"%s=%s&expectedVersion=%lu&color=%s%s",lists?"listId":"swimlaneId",id,version,color,extra?extra:"");
 c.form_body_length=strlen(c.form_body);return wena_sqlite_persistence_apply(store,&c,&r);
}
static void test_kind(const unsigned char *migration,size_t length,const char *hash,const char *directory,int lists)
{
 char path[1024],q[512],count_query[128],version_query[128];sqlite3 *db;WenaSqlitePersistence store;
 WenaSqliteBoardSnapshot *snapshot,*before;
 const char *table,*parent,*id,*foreign;const WenaColorContract *colors;size_t count,i;unsigned long version,request;
 table=lists?"list_colors":"swimlane_colors";parent=lists?"lists":"swimlanes";id=lists?"l":"s";foreign=lists?"fl":"fs";
 sprintf(path,"%s/%s.sqlite",directory,table);assert(wena_sqlite_open(path,migration,length,hash,&db));
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1),('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1),('fl','other','Foreign',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('fs','other','Foreign',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',4,1,7)");
 snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));assert(snapshot&&before);
 assert(wena_sqlite_board_load(db,"b",snapshot)&&!snapshot->lists[0].color[0]&&!snapshot->swimlanes[0].color[0]);
 wena_sqlite_persistence_init(&store,db);sprintf(count_query,"SELECT count(*) FROM %s",table);sprintf(version_query,"SELECT version FROM %s WHERE id='%s'",parent,id);
 assert(change(&store,lists,id,1,1,"",NULL));assert(!number(db,count_query)&&!number(db,"SELECT count(*) FROM idempotency_keys"));
 assert(!change(&store,lists,id,0,1,"red",NULL));assert(!change(&store,lists,id,2,1,"red",NULL));assert(!change(&store,lists,foreign,1,1,"red",NULL));
 assert(!change(&store,lists,"missing",1,1,"red",NULL));assert(!change(&store,lists,id,1,1,"red","&color=blue"));
 assert(!change(&store,lists,id,1,1,"White",NULL));assert(!change(&store,lists,id,1,1,"belize",NULL));
 assert(!change(&store,lists,id,1,1,"%23123",NULL));assert(!change(&store,lists,id,1,1,"%231234567",NULL));assert(!change(&store,lists,id,1,1,"red%00",NULL));
 sql(db,"DELETE FROM actors");assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"INSERT INTO actors VALUES('u','User',1)");
 sql(db,"PRAGMA query_only=ON");assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"PRAGMA query_only=OFF");
 if(lists){sql(db,"INSERT INTO list_archive_state VALUES('l','b',1,123)");assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"UPDATE list_archive_state SET archived=0");}
 sql(db,"CREATE TRIGGER late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"DROP TRIGGER late");
 sprintf(q,"CREATE TRIGGER ignored BEFORE INSERT ON %s BEGIN SELECT RAISE(IGNORE);END",table);sql(db,q);
 assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"DROP TRIGGER ignored");
 sprintf(q,"CREATE TRIGGER altered AFTER INSERT ON %s BEGIN UPDATE %s SET color='blue';END",table,table);sql(db,q);
 assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"DROP TRIGGER altered");
 sprintf(q,"CREATE TRIGGER ignored BEFORE UPDATE ON %s BEGIN SELECT RAISE(IGNORE);END",parent);sql(db,q);
 assert(!change(&store,lists,id,1,1,"red",NULL));sql(db,"DROP TRIGGER ignored");
 assert(!number(db,count_query)&&number(db,version_query)==1&&!number(db,"SELECT count(*) FROM idempotency_keys"));
 version=1;request=1;colors=wena_colors(&count);assert(count==25);
 for(i=0;i<count;++i){assert(change(&store,lists,id,version,request,colors[i].name,NULL));++version;++request;assert(number(db,version_query)==(sqlite3_int64)version);
 assert(wena_sqlite_board_load(db,"b",snapshot)&&!strcmp(lists?snapshot->lists[0].color:snapshot->swimlanes[0].color,colors[i].name));}
 assert(!change(&store,lists,id,version,request-1,"red",NULL));
 assert(change(&store,lists,id,version,request,"%23aBcD01",NULL));++version;++request;
 sprintf(q,"SELECT count(*) FROM %s WHERE color='#aBcD01'",table);assert(number(db,q)==1);
 assert(change(&store,lists,id,version,request,"%23aBcD01",NULL));assert(number(db,version_query)==(sqlite3_int64)version);
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==26);
 assert(change(&store,lists,id,version,request,"",NULL));++version;++request;
 sprintf(q,"SELECT count(*) FROM %s WHERE color=''",table);assert(number(db,q)==1);
 assert(wena_sqlite_board_load(db,"b",snapshot));memcpy(before,snapshot,sizeof(*before));
 sql(db,"PRAGMA ignore_check_constraints=ON");sprintf(q,"UPDATE %s SET color=x'726564'",table);sql(db,q);sql(db,"PRAGMA ignore_check_constraints=OFF");
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 assert(!change(&store,lists,id,version,request,"blue",NULL));sprintf(q,"UPDATE %s SET color='red'",table);sql(db,q);
 sql(db,"PRAGMA foreign_keys=OFF");sprintf(q,"UPDATE %s SET board_id='other'",table);sql(db,q);
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 assert(!change(&store,lists,id,version,request,"blue",NULL));sprintf(q,"UPDATE %s SET board_id='b'",table);sql(db,q);sql(db,"PRAGMA foreign_keys=ON");
 if(lists){sql(db,"UPDATE list_archive_state SET archived=1");assert(wena_sqlite_board_load(db,"b",snapshot)&&snapshot->lists[0].archived&&!strcmp(snapshot->lists[0].color,"red"));sql(db,"UPDATE list_archive_state SET archived=0");}
 sql(db,"PRAGMA foreign_keys=OFF");sprintf(q,"UPDATE %s SET %s='missing'",table,lists?"list_id":"swimlane_id");sql(db,q);
 memcpy(before,snapshot,sizeof(*before));assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 sprintf(q,"UPDATE %s SET %s='%s'",table,lists?"list_id":"swimlane_id",id);sql(db,q);sql(db,"PRAGMA foreign_keys=ON;PRAGMA ignore_check_constraints=ON");
 sprintf(q,"UPDATE %s SET color='#invalid'",table);sql(db,q);sql(db,"PRAGMA ignore_check_constraints=OFF");
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 sprintf(q,"UPDATE %s SET color='red'",table);sql(db,q);
 assert(number(db,"SELECT count(*) FROM cards WHERE id='c' AND archived=1 AND position=4 AND version=7")==1);
 assert(number(db,"SELECT version FROM boards WHERE id='b'")==1);
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(path,migration,length,hash,&db));wena_sqlite_persistence_init(&store,db);
 assert(change(&store,lists,id,version,request,"blue",NULL));++version;++request;
 assert(wena_sqlite_board_load(db,"b",snapshot)&&!strcmp(lists?snapshot->lists[0].color:snapshot->swimlanes[0].color,"blue"));memcpy(before,snapshot,sizeof(*before));
 sprintf(q,"DROP TABLE %s",table);sql(db,q);assert(!change(&store,lists,id,version,request,"red",NULL));
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 sprintf(q,"CREATE VIEW %s AS SELECT '%s' AS %s,'b' AS board_id,'red' AS color",table,id,lists?"list_id":"swimlane_id");sql(db,q);
 assert(!wena_sqlite_board_load(db,"b",snapshot)&&!memcmp(snapshot,before,sizeof(*snapshot)));
 assert(sqlite3_close(db)==SQLITE_OK);free(snapshot);free(before);
}
int main(int argc,char **argv)
{
 FILE *f;unsigned char *migration;long length;char hash[65];
 assert(argc==3);f=fopen(argv[1],"rb");assert(f&&!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);rewind(f);
 migration=(unsigned char*)malloc((size_t)length);assert(migration&&fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
 wena_sha256_hex(migration,(size_t)length,hash);test_kind(migration,(size_t)length,hash,argv[2],1);test_kind(migration,(size_t)length,hash,argv[2],0);
 free(migration);puts("Shared hierarchy colors: palette, hex, no-op, scope, replay, rollback, corruption and reopen passed");return 0;
}
