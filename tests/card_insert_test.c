#include "../server/sqlite_persistence.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q){assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static long number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;long result;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);result=(long)sqlite3_column_int64(s,0);sqlite3_finalize(s);return result;}
static void order(sqlite3 *db,const char *list,const char *lane,char *result)
{
    sqlite3_stmt *s;WenaSha256 hash;int rc;const char *id;
    assert(sqlite3_prepare_v2(db,"SELECT id,position FROM cards WHERE list_id=?1 AND swimlane_id=?2 ORDER BY position",-1,&s,NULL)==SQLITE_OK);
    sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,lane,-1,SQLITE_TRANSIENT);wena_sha256_init(&hash);
    while((rc=sqlite3_step(s))==SQLITE_ROW){id=(const char*)sqlite3_column_text(s,0);assert(wena_sqlite_card_order_add(&hash,id,strlen(id),(unsigned long)sqlite3_column_int64(s,1)));}
    assert(rc==SQLITE_DONE);sqlite3_finalize(s);wena_sha256_final_hex(&hash,result);
}
static void command(sqlite3 *db,WenaDomainCommand *c,int lane,unsigned long target)
{
    char source[65],destination[65];const char *list,*swimlane;
    list=lane?"l1":"l2";swimlane=lane?"s2":"s1";
    order(db,"l1","s1",source);order(db,list,swimlane,destination);
    memset(c,0,sizeof(*c));c->operation=WENA_DOMAIN_MOVE_CARD;c->request_version=1;
    strcpy(c->route,"/b/b/native");strcpy(c->user_id,"u");
    sprintf(c->form_body,"cardId=c&expectedVersion=1&targetListId=%s&targetSwimlaneId=%s&sourceListId=l1&sourceSwimlaneId=s1&insertPosition=%lu&expectedSourceOrder=%s&expectedOrder=%s",list,swimlane,target,source,destination);
    c->form_body_length=strlen(c->form_body);
}
static sqlite3 *fixture(const char *migration,int lane,int empty)
{
    sqlite3 *db;char q[512];assert(sqlite3_open(":memory:",&db)==SQLITE_OK);sql(db,migration);
    sql(db,"PRAGMA foreign_keys=ON;INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l1','b','One',0,1),('l2','b','Two',1,1);INSERT INTO swimlanes VALUES('s1','b','One',0,1),('s2','b','Two',1,1);");
    sql(db,"INSERT INTO cards VALUES('c','b','s1','l1','Card',2,0,1),('a','b','s1','l1','Archived',8,1,1)");
    if(!empty){sprintf(q,"INSERT INTO cards VALUES('d0','b','%s','%s','Duplicate',3,1,1),('d1','b','%s','%s','Duplicate',7,0,1),('d2','b','%s','%s','Duplicate',11,0,1)",lane?"s2":"s1",lane?"l1":"l2",lane?"s2":"s1",lane?"l1":"l2",lane?"s2":"s1",lane?"l1":"l2");sql(db,q);}
    return db;
}
static void reject(sqlite3 *db,WenaSqlitePersistence *store,WenaDomainCommand *c)
{
    WenaRegionResponse r;assert(!wena_sqlite_persistence_apply(store,c,&r));
    assert(!number(db,"SELECT count(*) FROM idempotency_keys"));
    assert(number(db,"SELECT count(*) FROM cards WHERE id='c' AND list_id='l1' AND swimlane_id='s1' AND position=2 AND version=1")==1);
    assert(number(db,"SELECT position FROM cards WHERE id='a'")==8);
}
int main(int argc,char **argv)
{
    FILE *f;long length;char *migration,*field,q[256];sqlite3 *db;WenaSqlitePersistence store;
    WenaDomainCommand c;WenaRegionResponse r;int lane,target,i,empty;
    assert(argc==3);(void)argv[2];f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);rewind(f);
    migration=(char*)malloc((size_t)length+1);assert(migration);assert(fread(migration,1,(size_t)length,f)==(size_t)length);migration[length]=0;fclose(f);
    for(lane=0;lane<2;++lane)for(empty=0;empty<2;++empty)for(target=0;target<(empty?1:4);++target){
        db=fixture(migration,lane,empty);wena_sqlite_persistence_init(&store,db);command(db,&c,lane,(unsigned long)target);
        assert(wena_sqlite_persistence_apply(&store,&c,&r));assert(store.moved_card_position==target);
        assert(number(db,"SELECT version FROM cards WHERE id='c'")==2);
        assert(number(db,"SELECT position FROM cards WHERE id='a'")==8);
        if(!empty)for(i=0;i<3;++i){sprintf(q,"SELECT position FROM cards WHERE id='d%d'",i);assert(number(db,q)==(i>=target?i+1:i));sprintf(q,"SELECT version FROM cards WHERE id='d%d'",i);assert(number(db,q)==1);}
        assert(number(db,"SELECT count(*) FROM idempotency_keys")==1);
        assert(!wena_sqlite_persistence_apply(&store,&c,&r));sqlite3_close(db);
    }
    db=fixture(migration,0,0);wena_sqlite_persistence_init(&store,db);command(db,&c,0,4);reject(db,&store,&c);
    command(db,&c,0,1);strcat(c.form_body,"&insertPosition=2");c.form_body_length=strlen(c.form_body);reject(db,&store,&c);
    command(db,&c,0,1);strcat(c.form_body,"&targetPosition=2");c.form_body_length=strlen(c.form_body);reject(db,&store,&c);
    command(db,&c,0,1);sql(db,"UPDATE cards SET position=9 WHERE id='a'");
    assert(!wena_sqlite_persistence_apply(&store,&c,&r));sql(db,"UPDATE cards SET position=8 WHERE id='a'");
    command(db,&c,0,1);sql(db,"UPDATE cards SET position=12 WHERE id='d2'");reject(db,&store,&c);sql(db,"UPDATE cards SET position=11 WHERE id='d2'");
    command(db,&c,0,1);sql(db,"UPDATE cards SET archived=1 WHERE id='c'");reject(db,&store,&c);sql(db,"UPDATE cards SET archived=0 WHERE id='c'");
    command(db,&c,0,1);sql(db,"CREATE TRIGGER reject_insert BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");reject(db,&store,&c);sql(db,"DROP TRIGGER reject_insert");
    assert(number(db,"SELECT position FROM cards WHERE id='d2'")==11);
    command(db,&c,0,1);sql(db,"CREATE TRIGGER reject_position BEFORE UPDATE ON cards WHEN NEW.id='d1' AND NEW.position=2 BEGIN SELECT RAISE(ABORT,'position');END");reject(db,&store,&c);sql(db,"DROP TRIGGER reject_position");
    assert(number(db,"SELECT position FROM cards WHERE id='d0'")==3);
    command(db,&c,0,1);sql(db,"CREATE TRIGGER alter_revision AFTER UPDATE ON cards WHEN NEW.id='d1' AND NEW.position=2 BEGIN UPDATE cards SET version=version+1 WHERE id='d1';END");reject(db,&store,&c);sql(db,"DROP TRIGGER alter_revision");
    assert(number(db,"SELECT version FROM cards WHERE id='d1'")==1);
    command(db,&c,0,1);sql(db,"CREATE TRIGGER alter_source AFTER UPDATE ON cards WHEN NEW.id='c' AND NEW.list_id='l2' BEGIN UPDATE cards SET position=9 WHERE id='a';END");reject(db,&store,&c);sql(db,"DROP TRIGGER alter_source");
    command(db,&c,0,1);strcpy(c.user_id,"unknown");reject(db,&store,&c);
    command(db,&c,0,1);field=strstr(c.form_body,"targetListId=l2");assert(field);field[14]='1';reject(db,&store,&c);
    command(db,&c,0,1);sql(db,"UPDATE cards SET version=2 WHERE id='c'");
    assert(!wena_sqlite_persistence_apply(&store,&c,&r));sql(db,"UPDATE cards SET version=1 WHERE id='c'");
    command(db,&c,0,1);assert(wena_sqlite_persistence_apply(&store,&c,&r));sqlite3_close(db);
    db=fixture(migration,0,1);wena_sqlite_persistence_init(&store,db);
    sql(db,"WITH RECURSIVE n(i) AS (VALUES(0) UNION ALL SELECT i+1 FROM n WHERE i<2047) INSERT INTO cards SELECT 'd'||i,'b','s1','l2','Capacity',i,0,1 FROM n");
    command(db,&c,0,0);reject(db,&store,&c);sqlite3_close(db);free(migration);
    puts("Cross-column card insertion: passed");return 0;
}
