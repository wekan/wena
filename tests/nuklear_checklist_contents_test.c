#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/components/cards/checklist_contents.h"
#include "../client/components/cards/card_body.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long statements;
static int trace(unsigned int kind, void *data, void *query, void *extra)
{(void)kind;(void)data;(void)query;(void)extra;++statements;return 0;}
static void sql(sqlite3 *db,const char *query)
{assert(sqlite3_exec(db,query,NULL,NULL,NULL)==SQLITE_OK);}
static float width(nk_handle handle,float height,const char *text,int length)
{(void)handle;(void)text;return height*(float)length*0.5f;}
static int label(struct nk_context *ctx,const char *value,struct nk_vec2 *point)
{
 const struct nk_command *command;
 nk_foreach(command,ctx)if(command->type==NK_COMMAND_TEXT){
  const struct nk_command_text *text;text=(const struct nk_command_text *)command;
  if((size_t)text->length==strlen(value)&&!memcmp(text->string,value,(size_t)text->length)){
   if(point)*point=nk_vec2(text->x+text->w*0.5f,text->y+text->h*0.5f);return 1;
  }
 }
 return 0;
}
static unsigned int draw(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,int board_default)
{
 unsigned int action;action=0;
 if(nk_begin(ctx,"Preview",nk_rect(0,0,600,700),NK_WINDOW_BORDER))
  action=wena_checklist_contents_render(ctx,contents,card,board_default);
 nk_end(ctx);return action;
}
static void frame(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,int board_default)
{nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);assert(!draw(ctx,contents,card,board_default));}
int main(int argc,char **argv)
{
 sqlite3 *db;FILE *file;long size;char *schema;
 struct nk_context ctx;struct nk_user_font font;struct nk_vec2 point;
 WenaChecklistBoardContents *contents;WenaCard card;int i,down;unsigned int action;
 assert(argc==2);file=fopen(argv[1],"rb");assert(file);assert(!fseek(file,0,SEEK_END));size=ftell(file);assert(size>0);rewind(file);
 schema=(char*)malloc((size_t)size+1);assert(schema);assert(fread(schema,1,(size_t)size,file)==(size_t)size);schema[size]=0;fclose(file);
 assert(sqlite3_open(":memory:",&db)==SQLITE_OK);sql(db,schema);free(schema);
 sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1)");
 sql(db,"INSERT INTO checklists(id,board_id,card_id,title,position,show_on_minicard) VALUES('inherit','b','c','Inherited',0,NULL),('hidden','b','c','Hidden',1,0),('shown','b','c','Shown',2,1)");
 sql(db,"INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('todo','b','c','inherit','Todo',0,0),('done','b','c','inherit','Done',1,1),('secret','b','c','hidden','Invisible',0,0)");
 contents=NULL;assert(wena_checklist_contents_load(db,"u","b",&contents));
 assert(wena_card_init(&card,"c","b","s","l","Card",0,0));
 memset(&font,0,sizeof(font));font.height=13;font.width=width;assert(nk_init_default(&ctx,&font));
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 for(i=0;i<100;++i)frame(&ctx,contents,&card,1);
 assert(!statements&&label(&ctx,"Inherited",NULL)&&label(&ctx,"Shown",NULL));
 assert(label(&ctx,"[ ] Todo",NULL)&&label(&ctx,"[x] Done",NULL));
 assert(!label(&ctx,"Hidden",NULL)&&!label(&ctx,"[ ] Invisible",NULL));
 frame(&ctx,contents,&card,0);assert(!label(&ctx,"Inherited",NULL)&&label(&ctx,"Shown",NULL));
 assert(label(&ctx,"Shown",&point));action=0;
 for(down=1;down>=0;--down){nk_clear(&ctx);nk_input_begin(&ctx);nk_input_motion(&ctx,(int)point.x,(int)point.y);nk_input_button(&ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(&ctx);action|=draw(&ctx,contents,&card,0);}
 assert(action==WENA_CARD_BODY_OPEN_CHECKLISTS&&!statements);
 card.archived=1;frame(&ctx,contents,&card,1);assert(!label(&ctx,"Shown",NULL));card.archived=0;
 strcpy(card.board_id,"other");frame(&ctx,contents,&card,1);assert(!label(&ctx,"Shown",NULL));strcpy(card.board_id,"b");
 strcpy(card.id,"missing");frame(&ctx,contents,&card,1);assert(!label(&ctx,"Shown",NULL));strcpy(card.id,"c");
 frame(&ctx,contents,&card,2);assert(!label(&ctx,"Shown",NULL));
 assert(!statements);assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 sql(db,"UPDATE checklists SET hide_checked_items=1 WHERE id='inherit'");assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);assert(label(&ctx,"[ ] Todo",NULL)&&!label(&ctx,"[x] Done",NULL));
 assert(contents->summary.cards[0].progress.finished==1&&contents->summary.cards[0].progress.total==3);
 sql(db,"UPDATE checklists SET hide_all_items=1 WHERE id='inherit'");assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);assert(label(&ctx,"Inherited",NULL)&&!label(&ctx,"[ ] Todo",NULL));
 sql(db,"UPDATE cards SET archived=1");assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);assert(!label(&ctx,"Shown",NULL));
 wena_checklist_contents_free(contents);nk_free(&ctx);assert(sqlite3_close(db)==SQLITE_OK);
 puts("Real minicard previews: overrides, hidden/completed items, scoped actions and zero-SQL frames passed");return 0;
}
