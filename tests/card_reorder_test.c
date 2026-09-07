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
    assert(sqlite3_close(d)==SQLITE_OK);free(migration);free(s);free(before);free(reopened);
    puts("indexed card reorder tests passed");return 0;
}
