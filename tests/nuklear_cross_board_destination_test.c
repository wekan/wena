#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/checklists.h"
#include "../client/features/checklist_mutation.h"
#include "../server/sqlite_directory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long statements;
static sqlite3 *active_database;
static int stale_listing;
static int trace(unsigned int k,void *p,void *q,void *e)
{(void)k;(void)p;(void)q;(void)e;++statements;return 0;}
static void sql(sqlite3 *db,const char *query)
{assert(sqlite3_exec(db,query,NULL,NULL,NULL)==SQLITE_OK);}
static void schema(sqlite3 *db,const char *path)
{
 FILE *f;long n;char *s;f=fopen(path,"rb");assert(f&&!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0);rewind(f);
 s=(char*)malloc((size_t)n+1);assert(s&&fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);sql(db,s);free(s);
}
static int number(sqlite3 *db,const char *query)
{
 sqlite3_stmt *st;int n;assert(sqlite3_prepare_v2(db,query,-1,&st,NULL)==SQLITE_OK);
 assert(sqlite3_step(st)==SQLITE_ROW);n=sqlite3_column_int(st,0);assert(sqlite3_finalize(st)==SQLITE_OK);return n;
}
static float width(nk_handle h,float z,const char *s,int n)
{(void)h;(void)s;return z*(float)n*0.5f;}
static void render(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card)
{assert(wena_checklists_render(ctx,state,card,1,1000,800));}
static void frame(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card)
{nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);render(ctx,state,card);}
static struct nk_vec2 label(struct nk_context *ctx,const char *value)
{
 const struct nk_command *command;nk_foreach(command,ctx)if(command->type==NK_COMMAND_TEXT){
  const struct nk_command_text *text;text=(const struct nk_command_text*)command;
  if((size_t)text->length==strlen(value)&&!memcmp(text->string,value,(size_t)text->length))
   return nk_vec2(text->x+text->w*0.5f,text->y+text->h*0.5f);
 }
 fprintf(stderr,"Missing destination label: %s\n",value);assert(0);return nk_vec2(0,0);
}
static void click(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,const char *value)
{
 struct nk_vec2 p;int down;p=label(ctx,value);
 for(down=1;down>=0;--down){nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)p.x,(int)p.y);
 nk_input_button(ctx,NK_BUTTON_LEFT,(int)p.x,(int)p.y,down);nk_input_end(ctx);render(ctx,state,card);}
}
static void open_move(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,int item)
{
 frame(ctx,state,card);click(ctx,state,card,item?"Edit":"Rename");frame(ctx,state,card);
 click(ctx,state,card,item?"Destination":"Move Checklist");frame(ctx,state,card);
 assert(wena_checklists_poll_destination(state)==1);frame(ctx,state,card);
}
static void choose(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,int item)
{
 unsigned long before;before=statements;click(ctx,state,card,"Boards");assert(statements==before&&!state->target_card_id[0]);
 assert(wena_checklists_poll_destination(state)==1);frame(ctx,state,card);
 before=statements;click(ctx,state,card,"Other Board");assert(statements==before);
 assert(wena_checklists_poll_destination(state)==1);frame(ctx,state,card);
 assert(state->destination->picker.page.total==5);
 before=statements;click(ctx,state,card,"Next Page");assert(statements==before);
 assert(wena_checklists_poll_destination(state)==1);frame(ctx,state,card);
 if(stale_listing)sql(active_database,"UPDATE cards SET version=version+1 WHERE id='a4'");
 before=statements;click(ctx,state,card,"Destination");assert(statements==before&&!state->target_card_id[0]);
 if(stale_listing){assert(wena_checklists_poll_destination(state)==-1&&state->error&&!state->target_card_id[0]);return;}
 assert(wena_checklists_poll_destination(state)==1);frame(ctx,state,card);
 assert(!strcmp(state->target_board_id,"x")&&!strcmp(state->target_card_id,"a4"));
 if(item){click(ctx,state,card,"Checklist");frame(ctx,state,card);click(ctx,state,card,"Target tasks [dest]");frame(ctx,state,card);}
}
int main(int argc,char **argv)
{
 sqlite3 *db;WenaChecklistMutation mutation;WenaSqliteDirectoryReader reader;
 WenaCardDestination chooser;WenaChecklistsState state;WenaCard card;
 struct nk_context ctx;struct nk_user_font font;unsigned long before;int item,i;char query[200];
 assert(argc==4);
 for(item=0;item<2;++item){
 assert(sqlite3_open(":memory:",&db)==SQLITE_OK);active_database=db;sql(db,"PRAGMA foreign_keys=ON");
 schema(db,argv[1]);schema(db,argv[2]);schema(db,argv[3]);
 sql(db,"INSERT INTO actors VALUES('u','Local user',1);INSERT INTO boards VALUES('b','Source Board',1),('x','Other Board',1);"
 "INSERT INTO lists VALUES('l','b','List',0,1),('xl','x','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('xs','x','Lane',0,1);"
 "INSERT INTO cards VALUES('src','b','s','l','Source card',0,0,1),('archived','x','xs','xl','Hidden',99,1,1)");
 for(i=0;i<5;++i){sprintf(query,"INSERT INTO cards VALUES('a%d','x','xs','xl','%s',%d,0,1)",i,i==4?"Destination":"Shared title",i);sql(db,query);}
 sql(db,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('cl','b','src','Tasks',0),('dest','x','a4','Target tasks',0);"
 "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('i','b','src','cl','Completed',19,1)");
 assert(wena_card_init(&card,"src","b","s","l","Source card",0,0));
 assert(wena_checklist_mutation_init(&mutation,db,"u","b"));
 assert(wena_sqlite_directory_reader_init(&reader,db,"u"));
 assert(wena_card_destination_init(&chooser,4,wena_sqlite_directory_read,&reader));
 wena_checklists_init(&state,wena_checklist_mutation_load,wena_checklist_mutation_save,&mutation);
 state.destination=&chooser;state.load_destination=wena_checklist_mutation_load_destination;state.destination_context=&mutation;
 memset(&font,0,sizeof(font));font.height=13;font.width=width;assert(nk_init_default(&ctx,&font));
 assert(wena_checklists_open(&state,&card));assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 open_move(&ctx,&state,&card,item);stale_listing=1;choose(&ctx,&state,&card,item);stale_listing=0;
 before=statements;frame(&ctx,&state,&card);assert(!wena_checklists_poll_destination(&state)&&statements==before);
 click(&ctx,&state,&card,"Cancel");sql(db,"UPDATE cards SET version=1 WHERE id='a4'");
 open_move(&ctx,&state,&card,item);choose(&ctx,&state,&card,item);
 before=statements;for(i=0;i<20;++i){frame(&ctx,&state,&card);assert(!wena_checklists_poll_destination(&state));}assert(statements==before);
 click(&ctx,&state,&card,"Cancel");assert(!state.action&&!chooser.picker.open&&!state.target_snapshot);
 open_move(&ctx,&state,&card,item);choose(&ctx,&state,&card,item);
 sql(db,"UPDATE cards SET version=2 WHERE id='a4'");click(&ctx,&state,&card,"Save");
 assert(state.error&&state.target_card_version==1&&state.snapshot->item_count==1);
 click(&ctx,&state,&card,"Cancel");open_move(&ctx,&state,&card,item);choose(&ctx,&state,&card,item);
 assert(state.target_card_version==2);
 sql(db,"CREATE TRIGGER late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 click(&ctx,&state,&card,"Save");assert(state.error&&state.action&&state.snapshot->item_count==1);
 assert(number(db,"SELECT count(*) FROM checklist_items WHERE board_id='b'")==1);sql(db,"DROP TRIGGER late");
 click(&ctx,&state,&card,"Save");assert(!state.error&&!state.action&&!state.snapshot->item_count&&!chooser.picker.open);
 assert(number(db,"SELECT count(*) FROM checklist_items WHERE board_id='x' AND card_id='a4' AND is_finished=1 AND version=2")==1);
 assert(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0);
 assert(number(db,"SELECT version FROM cards WHERE id='a4'")==3);
 assert(number(db,"SELECT version FROM cards WHERE id='src'")==2);
 before=statements;frame(&ctx,&state,&card);assert(!wena_checklists_poll_destination(&state)&&statements==before);
 wena_checklists_close(&state);assert(!chooser.picker.open);nk_free(&ctx);assert(sqlite3_close(db)==SQLITE_OK);
 }
 puts("Shared board/card destination: checklist/item moves, paging, SQL-free drawing, cancel, stale, rollback and one-shot saves passed");return 0;
}
