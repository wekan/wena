#include "../client/features/checklist_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *q)
{assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static int number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;int n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static void schema(sqlite3 *db,const char *path)
{
 FILE *f;long n;char *s;f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0);rewind(f);
 s=(char*)malloc((size_t)n+1);assert(s&&fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);sql(db,s);free(s);
}
static void unchanged(sqlite3 *db,WenaChecklistMutation *adapter,WenaChecklistEdit *edit)
{
 char query[160];
 assert(!wena_checklist_mutation_save_request(adapter,"b","src",edit,1));
 sprintf(query,"SELECT position FROM %s WHERE id='z'",edit->action==WENA_CHECKLIST_MOVE_ITEM?"checklist_items":"checklists");
 assert(number(db,query)==2147483647);
 sprintf(query,"SELECT position FROM %s WHERE id='a'",edit->action==WENA_CHECKLIST_MOVE_ITEM?"checklist_items":"checklists");
 assert(number(db,query)==0);
 assert(number(db,"SELECT sum(version) FROM cards")==2);
 assert(number(db,"SELECT count(*) FROM idempotency_keys")==0);
 assert(number(db,"SELECT version FROM checklists WHERE id='cl'")==1);
 assert(number(db,"SELECT version FROM checklist_items WHERE id='i'")==1);
 assert(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0);
 assert(sqlite3_get_autocommit(db));
}
static void run(char **paths,int item,int cross,int same_card,unsigned long ordinal)
{
 sqlite3 *db;WenaChecklistMutation adapter;WenaChecklistEdit edit;char query[1024];
 const char *board,*card,*table,*moved;sqlite3_stmt *st;int index;const char *expected[3];
 WenaDomainCommand command;WenaRegionResponse response;
 board=cross?"x":"b";card=same_card?"src":"dst";
 table=item?"checklist_items":"checklists";moved=item?"i":"cl";
 assert(sqlite3_open(":memory:",&db)==SQLITE_OK);sql(db,"PRAGMA foreign_keys=ON");
 schema(db,paths[1]);schema(db,paths[2]);schema(db,paths[3]);
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1),('x','Other',1);"
 "INSERT INTO lists VALUES('l','b','List',0,1),('xl','x','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('xs','x','Lane',0,1);"
 "INSERT INTO cards VALUES('src','b','s','l','Source',0,0,1)");
 sprintf(query,"INSERT INTO cards VALUES('dst','%s','%s','%s','Target',1,0,1)",board,cross?"xs":"s",cross?"xl":"l");sql(db,query);
 sql(db,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('cl','b','src','Source tasks',9);"
 "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('i','b','src','cl','Completed',19,1)");
 if(item){
  sprintf(query,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('dest','%s','%s','Target tasks',0)",board,card);sql(db,query);
  sprintf(query,"INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES('a','%s','%s','dest','First',0),('z','%s','%s','dest','Last',2147483647)",board,card,board,card);sql(db,query);
 }else{
  sprintf(query,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('a','%s','%s','First',0),('z','%s','%s','Last',2147483647)",board,card,board,card);sql(db,query);
 }
 assert(wena_checklist_mutation_init(&adapter,db,"u","b"));memset(&edit,0,sizeof(edit));
 edit.action=item?WENA_CHECKLIST_MOVE_ITEM:WENA_CHECKLIST_MOVE;edit.checklist_id="cl";edit.item_id="i";
 edit.target_board_id=board;edit.target_card_id=card;edit.target_checklist_id="dest";
 edit.expected_card_version=edit.expected_checklist_version=edit.expected_item_version=1;
 edit.expected_target_card_version=edit.expected_target_checklist_version=1;
 /* Default append still rejects exhausted positions; explicit insertion can
  * safely compact them with the same writer used by same-parent reorder. */
 unchanged(db,&adapter,&edit);edit.insert_at_position=1;edit.target_position=3;unchanged(db,&adapter,&edit);
 edit.insert_at_position=2;edit.target_position=ordinal;unchanged(db,&adapter,&edit);edit.insert_at_position=1;
 sprintf(query,"CREATE TRIGGER late BEFORE UPDATE OF position ON %s WHEN OLD.id='z' BEGIN SELECT RAISE(ABORT,'sibling'); END",table);sql(db,query);
 unchanged(db,&adapter,&edit);sql(db,"DROP TRIGGER late");
 sql(db,"CREATE TRIGGER late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END");
 unchanged(db,&adapter,&edit);sql(db,"DROP TRIGGER late");
 /* A changing destination sibling at its terminal revision cannot wrap. */
 sprintf(query,"UPDATE %s SET version=%lu WHERE id='z'",table,WENA_VERSION_READ_MAX);sql(db,query);unchanged(db,&adapter,&edit);
 sprintf(query,"UPDATE %s SET version=1 WHERE id='z'",table);sql(db,query);
 memset(&command,0,sizeof(command));command.operation=item?WENA_DOMAIN_MOVE_CHECKLIST_ITEM:WENA_DOMAIN_MOVE_CHECKLIST;
 strcpy(command.user_id,"u");strcpy(command.route,"/b/b/native");command.request_version=8;
 sprintf(command.form_body,"cardId=src&expectedVersion=1&checklistId=cl&expectedChecklistVersion=1&targetBoardId=%s&targetCardId=%s&expectedTargetVersion=1&itemId=i&expectedItemVersion=1&targetChecklistId=dest&expectedTargetChecklistVersion=1&targetPosition=0&targetPosition=1",board,card);
 command.form_body_length=strlen(command.form_body);assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
 assert(wena_checklist_mutation_save_request(&adapter,"b","src",&edit,1));
 assert(!wena_checklist_mutation_save_request(&adapter,"b","src",&edit,1));
 index=0;if(ordinal==0)expected[index++]=moved;expected[index++]="a";if(ordinal==1)expected[index++]=moved;expected[index++]="z";if(ordinal==2)expected[index++]=moved;assert(index==3);
 sprintf(query,"SELECT id,position,version FROM %s WHERE board_id='%s' AND card_id='%s'%s ORDER BY position",table,board,card,item?" AND checklist_id='dest'":"");
 assert(sqlite3_prepare_v2(db,query,-1,&st,NULL)==SQLITE_OK);
 for(index=0;index<3;++index){
  const char *stored;int version;assert(sqlite3_step(st)==SQLITE_ROW);stored=(const char*)sqlite3_column_text(st,0);
  assert(!strcmp(stored,expected[index])&&sqlite3_column_int(st,1)==index);
  version=!strcmp(stored,"a")&&ordinal!=0?1:2;assert(sqlite3_column_int(st,2)==version);
 }
 assert(sqlite3_step(st)==SQLITE_DONE&&sqlite3_finalize(st)==SQLITE_OK);
 assert(number(db,"SELECT version FROM cards WHERE id='src'")==2);
 assert(number(db,"SELECT version FROM cards WHERE id='dst'")==(same_card?1:2));
 assert(number(db,"SELECT is_finished FROM checklist_items WHERE id='i'")==1);
 if(item)assert(number(db,"SELECT version FROM checklists WHERE id='dest'")==2);
 else assert(number(db,"SELECT position FROM checklist_items WHERE id='i'")==19);
 assert(number(db,"SELECT version FROM checklist_items WHERE id='i'")==2);
 assert(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0);
 assert(sqlite3_close(db)==SQLITE_OK);
}
int main(int argc,char **argv)
{
 int item,cross;unsigned long ordinal;assert(argc==4);
 for(item=0;item<2;++item)for(cross=0;cross<2;++cross)for(ordinal=0;ordinal<3;++ordinal)run(argv,item,cross,0,ordinal);
 for(ordinal=0;ordinal<3;++ordinal)run(argv,1,0,1,ordinal);
 puts("Transfer insertion: shared compaction, exact ordinals, max positions, revisions, cross-board scope and rollback passed");return 0;
}
