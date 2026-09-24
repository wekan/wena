#include "../client/features/card_mutation.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3*d,const char*q){assert(sqlite3_exec(d,q,NULL,NULL,NULL)==SQLITE_OK);}
static int number(sqlite3*d,const char*q){sqlite3_stmt*s;int n;assert(sqlite3_prepare_v2(d,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
static void unchanged(sqlite3*d,WenaSqliteBoardSnapshot*s,WenaSqliteBoardSnapshot*b,int keys){assert(!memcmp(s,b,sizeof(*s)));assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);}
static void same_column_order(WenaSqliteBoardSnapshot*a,WenaSqliteBoardSnapshot*b){size_t i,j;j=0;for(i=0;i<a->card_count;++i){if(strcmp(a->cards[i].list_id,"l1"))continue;while(j<b->card_count&&strcmp(b->cards[j].list_id,"l1"))++j;assert(j<b->card_count);assert(!memcmp(&a->cards[i],&b->cards[j],sizeof(WenaCard)));++j;}}
static void board_fingerprint(sqlite3 *db,const char *board,char output[65])
{sql(db,"BEGIN");assert(wena_sqlite_card_board_order(db,board,output));sql(db,"COMMIT");}
static void batch_command(sqlite3 *db,WenaDomainCommand *command,const char *board,const char *list,const char *lane,
    WenaCardRevision *selected,size_t count,unsigned long position,unsigned long request)
{
    char fingerprint[65];board_fingerprint(db,board,fingerprint);memset(command,0,sizeof(*command));
    command->operation=WENA_DOMAIN_MOVE_SELECTED_CARDS;command->request_version=request;
    strcpy(command->user_id,"u");sprintf(command->route,"/b/%s/native",board);
    sprintf(command->form_body,"targetListId=%s&targetSwimlaneId=%s&insertPosition=%lu&expectedBoardOrder=%s",list,lane,position,fingerprint);
    command->form_body_length=strlen(command->form_body);command->selected_cards=selected;command->selected_card_count=count;
}
static void batch_rejected(sqlite3 *db,WenaSqlitePersistence *store,WenaDomainCommand *command)
{
    char before[65],after[65];int keys;WenaRegionResponse response;
    board_fingerprint(db,"bm",before);keys=number(db,"SELECT count(*) FROM idempotency_keys");
    assert(!wena_sqlite_persistence_apply(store,command,&response));
    board_fingerprint(db,"bm",after);assert(!strcmp(before,after));
    assert(keys==number(db,"SELECT count(*) FROM idempotency_keys"));
}
static int batch_commit_fail(void *context){(void)context;return 1;}
static void selected_moves(sqlite3 *db)
{
    WenaSqlitePersistence store;WenaDomainCommand command;WenaRegionResponse response;WenaCardRevision rows[3],*all;
    char fingerprint[65],before[65],query[256];size_t i;int keys;
    sql(db,"INSERT INTO boards VALUES('bm','Batch move',1);INSERT INTO lists VALUES('src','bm','Source',0,1),('dst','bm','Destination',1,1);"
        "INSERT INTO swimlanes VALUES('ms','bm','Lane',0,1),('mt','bm','Other',1,1);"
        "INSERT INTO cards VALUES('ma','bm','ms','src','Repeated',10,0,1),('mb','bm','ms','src','Untouched',20,0,1),"
        "('mx','bm','mt','src','Repeated',1,0,1),('mu','bm','mt','src','Untouched',2,0,1),"
        "('md','bm','ms','dst','Repeated',5,0,1),('mh','bm','ms','dst','Archived',9,1,1)");
    wena_sqlite_persistence_init(&store,db);strcpy(rows[0].id,"mx");strcpy(rows[1].id,"md");strcpy(rows[2].id,"ma");
    for(i=0;i<3;++i)rows[i].version=1;
    strcpy(fingerprint,"preserved");assert(!wena_sqlite_card_board_order(db,"bm",fingerprint)&&!strcmp(fingerprint,"preserved"));
    batch_command(db,&command,"bm","dst","ms",rows,3,1,1000);
    command.selected_card_count=0;batch_rejected(db,&store,&command);command.selected_card_count=2049;batch_rejected(db,&store,&command);command.selected_card_count=3;
    command.selected_cards=NULL;batch_rejected(db,&store,&command);command.selected_cards=rows;
    strcpy(rows[2].id,"mx");batch_rejected(db,&store,&command);strcpy(rows[2].id,"mh");batch_rejected(db,&store,&command);
    strcpy(rows[2].id,"missing");batch_rejected(db,&store,&command);strcpy(rows[2].id,"ma");rows[2].version=2;batch_rejected(db,&store,&command);rows[2].version=1;
    strcpy(command.user_id,"unknown");batch_rejected(db,&store,&command);strcpy(command.user_id,"u");
    sql(db,"UPDATE cards SET version=version+1 WHERE id='mb'");batch_rejected(db,&store,&command);sql(db,"UPDATE cards SET version=version-1 WHERE id='mb'");
    batch_command(db,&command,"bm","dst","ms",rows,3,3,1000);batch_rejected(db,&store,&command);
    batch_command(db,&command,"bm","dst","ms",rows,3,1,1000);
    sql(db,"CREATE TRIGGER fail_selected_move BEFORE UPDATE ON cards WHEN OLD.id='ma' BEGIN SELECT RAISE(ABORT,'last');END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER fail_selected_move");
    sql(db,"CREATE TRIGGER alter_selected_move AFTER UPDATE ON cards WHEN NEW.id='ma' BEGIN UPDATE cards SET position=123 WHERE id='mb';END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER alter_selected_move");
    sql(db,"CREATE TRIGGER late_selected_move BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER late_selected_move");
    sqlite3_commit_hook(db,batch_commit_fail,NULL);batch_rejected(db,&store,&command);sqlite3_commit_hook(db,NULL,NULL);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));batch_rejected(db,&store,&command);
    assert(number(db,"SELECT position FROM cards WHERE id='mx'")==0&&number(db,"SELECT position FROM cards WHERE id='md'")==1);
    assert(number(db,"SELECT position FROM cards WHERE id='ma'")==2&&number(db,"SELECT position FROM cards WHERE id='mh'")==3);
    assert(number(db,"SELECT version FROM cards WHERE id='mh'")==1&&number(db,"SELECT position FROM cards WHERE id='mb'")==20);
    assert(number(db,"SELECT count(*) FROM cards WHERE id IN('ma','md','mx') AND list_id='dst' AND swimlane_id='ms' AND version=2")==3);
    for(i=0;i<3;++i){assert(rows[i].version==1);rows[i].version=2;}
    batch_command(db,&command,"bm","dst","ms",rows,3,0,1001);board_fingerprint(db,"bm",before);
    keys=number(db,"SELECT count(*) FROM idempotency_keys");assert(wena_sqlite_persistence_apply(&store,&command,&response));
    board_fingerprint(db,"bm",fingerprint);assert(!strcmp(before,fingerprint)&&keys==number(db,"SELECT count(*) FROM idempotency_keys"));
    batch_command(db,&command,"bm","dst","ms",rows,3,4,1002);assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT position FROM cards WHERE id='mh'")==0&&number(db,"SELECT position FROM cards WHERE id='mx'")==1);
    for(i=0;i<3;++i)rows[i].version=3;
    batch_command(db,&command,"bm","dst","mt",rows,3,0,1003);assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT count(*) FROM cards WHERE list_id='dst' AND swimlane_id='mt' AND version=4")==3);
    /* All 2048 active cards can move together into an empty destination. */
    sql(db,"BEGIN;INSERT INTO boards VALUES('cap','Capacity',1);INSERT INTO lists VALUES('cs','cap','Source',0,1),('cd','cap','Destination',1,1);INSERT INTO swimlanes VALUES('cl','cap','Lane',0,1)");
    all=(WenaCardRevision*)calloc(2048,sizeof(*all));assert(all);
    for(i=0;i<2048;++i){sprintf(all[i].id,"bulk%lu",(unsigned long)i);all[i].version=1;
        sprintf(query,"INSERT INTO cards VALUES('%s','cap','cl','cs','Bulk',%lu,0,1)",all[i].id,(unsigned long)i);sql(db,query);}
    sql(db,"COMMIT");batch_command(db,&command,"cap","cd","cl",all,2048,0,1004);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT count(*) FROM cards WHERE board_id='cap' AND list_id='cd' AND version=2")==2048);
    assert(number(db,"SELECT position FROM cards WHERE id='bulk2047'")==2047);free(all);
}

int main(int argc,char**argv)
{
    const char*hash="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE*f;unsigned char*migration;long length;sqlite3*d,*writer;
    WenaSqliteBoardSnapshot*s,*before,*reopened;WenaCardMutation a,other;
    WenaSqlitePersistence store;WenaDomainCommand command;WenaRegionResponse response;
    char path[512];size_t count,i;int keys;
    assert(argc==3);f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);rewind(f);
    migration=(unsigned char*)malloc((size_t)length);assert(migration);assert(fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
    s=(WenaSqliteBoardSnapshot*)malloc(sizeof(*s));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));reopened=(WenaSqliteBoardSnapshot*)malloc(sizeof(*reopened));assert(s&&before&&reopened);
    sprintf(path,"%s/card-reorder.sqlite",argv[2]);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&d));
    sql(d,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('other','Other',1);INSERT INTO lists VALUES('l1','b','One',0,1);INSERT INTO lists VALUES('l2','b','Two',1,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);");
    sql(d,"INSERT INTO cards VALUES('c0','b','s','l1','Zero',0,0,1);INSERT INTO cards VALUES('c1','b','s','l1','Archived',3,1,1);INSERT INTO cards VALUES('c2','b','s','l1','Two',8,0,1);INSERT INTO cards VALUES('o0','b','s','l2','Other column',1,0,1);");
    assert(wena_sqlite_board_load(d,"b",s));count=s->card_count;assert(count==4);
    assert(wena_card_mutation_init(&a,d,"u","b",s->cards,count));
    memcpy(before,s,sizeof(*s));keys=0;
    /* A no-op does not compact normal gaps or reserve a replay identity. */
    assert(wena_card_mutation_reorder_request(&a,"b","c0",1,40,0));unchanged(d,s,before,keys);
    assert(number(d,"SELECT position FROM cards WHERE id='c2'")==8);
    assert(!wena_card_mutation_reorder(&a,"other","c0",1,2));assert(!wena_card_mutation_reorder(&a,"b","missing",1,2));
    assert(!wena_card_mutation_reorder(&a,"b","c1",1,2));assert(!wena_card_mutation_reorder(&a,"b","c0",1,3));
    assert(!wena_card_mutation_reorder(&a,"b","c0",0,2));assert(!wena_card_mutation_reorder_request(&a,"b","c0",1,0,2));
    assert(!wena_card_mutation_reorder(&a,"b","c0",99,2));
    strcpy(s->cards[0].list_id,"l2");
    assert(!wena_card_mutation_reorder(&a,"b","c0",1,1));
    strcpy(s->cards[0].list_id,"l1");
    assert(wena_card_mutation_init(&other,d,"unknown","b",s->cards,count));assert(!wena_card_mutation_reorder(&other,"b","c0",1,2));
    assert(wena_card_mutation_init(&other,d,"u","b",s->cards,count-1));assert(!wena_card_mutation_reorder(&other,"b","c0",1,1));unchanged(d,s,before,keys);
    /* A valid sorted snapshot must not conceal a malformed traversal cache. */
    {WenaCard swap;swap=s->cards[0];s->cards[0]=s->cards[3];s->cards[3]=swap;}
    assert(!wena_card_mutation_reorder(&a,"b","c0",1,2));
    assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);
    memcpy(s,before,sizeof(*s));
    /* Abort after other rows have already been moved into temporary slots. */
    sql(d,"CREATE TRIGGER reject_stage BEFORE UPDATE ON cards WHEN OLD.id='c0' AND NEW.position>8 BEGIN SELECT RAISE(ABORT,'stage'); END;");
    assert(!wena_card_mutation_reorder(&a,"b","c0",1,2));unchanged(d,s,before,keys);sql(d,"DROP TRIGGER reject_stage");
    assert(number(d,"SELECT position FROM cards WHERE id='c1'")==3);
    sql(d,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END;");
    assert(!wena_card_mutation_reorder(&a,"b","c0",1,2));unchanged(d,s,before,keys);sql(d,"DROP TRIGGER reject_metadata");
    assert(number(d,"SELECT position FROM cards WHERE id='c2'")==8);
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_MOVE_CARD;command.request_version=90;
    strcpy(command.user_id,"u");strcpy(command.route,"/b/b/native");
    strcpy(command.form_body,"cardId=c0&expectedVersion=1&targetListId=l1&targetSwimlaneId=s&targetPosition=2");command.form_body_length=strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&a.persistence,&command,&response));
    strcat(command.form_body,"&expectedOrder=bad");command.form_body_length=strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&a.persistence,&command,&response));unchanged(d,s,before,keys);
    assert(wena_card_mutation_reorder_request(&a,"b","c0",1,40,2));++keys;
    assert(!strcmp(s->cards[0].id,"c1")&&s->cards[0].archived&&s->cards[0].sort==0);
    assert(!memcmp(&s->cards[1],&before->cards[1],sizeof(WenaCard)));
    assert(!strcmp(s->cards[2].id,"c2")&&s->cards[2].sort==1);
    assert(!strcmp(s->cards[3].id,"c0")&&s->cards[3].sort==2);
    assert(number(d,"SELECT archived FROM cards WHERE id='c1'")==1);
    assert(number(d,"SELECT version FROM cards WHERE id='c1'")==1);
    assert(number(d,"SELECT position FROM cards WHERE id='o0'")==1);
    memcpy(before,s,sizeof(*s));assert(!wena_card_mutation_reorder_request(&a,"b","c0",2,40,0));unchanged(d,s,before,keys);
    assert(wena_card_mutation_reorder_request(&a,"b","c0",2,41,2));unchanged(d,s,before,keys);
    assert(wena_card_mutation_reorder_request(&a,"b","c0",2,41,0));++keys;
    memcpy(before,s,sizeof(*s));
    /* A second client appends the last card to the same column, changing only
     * its numeric position. Ordered-ID-only guards would miss this conflict. */
    assert(wena_sqlite_open(path,migration,(size_t)length,hash,&writer));wena_sqlite_persistence_init(&store,writer);
    command.request_version=88;strcpy(command.route,"/b/b/other-client");
    strcpy(command.form_body,"cardId=c2&expectedVersion=1&targetListId=l1&targetSwimlaneId=s");command.form_body_length=strlen(command.form_body);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));assert(sqlite3_close(writer)==SQLITE_OK);++keys;
    assert(!wena_card_mutation_reorder(&a,"b","c0",3,2));assert(!wena_card_mutation_reorder(&a,"b","c0",3,0));unchanged(d,s,before,keys);
    assert(wena_sqlite_board_load(d,"b",s));assert(wena_card_mutation_reorder(&a,"b","c0",3,2));++keys;
    memcpy(before,s,sizeof(*s));
    sql(d,"UPDATE cards SET version=0.5 WHERE id='c1'");assert(!wena_card_mutation_reorder(&a,"b","c0",4,0));unchanged(d,s,before,keys);sql(d,"UPDATE cards SET version=1 WHERE id='c1'");
    sql(d,"UPDATE cards SET position=0.5 WHERE id='c1'");assert(!wena_card_mutation_reorder(&a,"b","c0",4,0));unchanged(d,s,before,keys);sql(d,"UPDATE cards SET position=0 WHERE id='c1'");
    sql(d,"UPDATE cards SET position=9223372036854775807 WHERE id='c1'");assert(!wena_card_mutation_reorder(&a,"b","c0",4,0));unchanged(d,s,before,keys);sql(d,"UPDATE cards SET position=0 WHERE id='c1'");
    for(i=0;i<s->card_count;++i)if(!strcmp(s->cards[i].id,"c1"))s->cards[i].sort=0.5;
    assert(!wena_card_mutation_reorder(&a,"b","c0",4,0));memcpy(s,before,sizeof(*s));
    assert(sqlite3_close(d)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&d));
    assert(wena_sqlite_board_load(d,"b",reopened));same_column_order(s,reopened);
    assert(wena_card_mutation_init(&a,d,"u","b",s->cards,count));
    assert(!wena_card_mutation_reorder_request(&a,"b","c0",4,42,0));assert(wena_card_mutation_reorder(&a,"b","c0",4,0));
    assert(number(d,"SELECT max(request_version) FROM idempotency_keys WHERE route='/b/b/native'")==43);
    selected_moves(d);
    assert(sqlite3_close(d)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&d));
    assert(number(d,"SELECT count(*) FROM cards WHERE board_id='cap' AND list_id='cd' AND version=2")==2048);
    assert(sqlite3_close(d)==SQLITE_OK);free(migration);free(s);free(before);free(reopened);
    puts("indexed card reorder tests passed");return 0;
}
