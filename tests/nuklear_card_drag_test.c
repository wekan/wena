#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_drag.h"
#include "../client/features/boards/reload.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Fixture {
 struct nk_context ctx;
 struct nk_user_font font;
 struct nk_vec2 points[2];
 sqlite3 *db;
 WenaSqliteBoardSnapshot *board;
 WenaCardMutation mutation;
 WenaCardDrag drag;
 unsigned long versions[4];
} Fixture;
static unsigned long statements;
static int trace(unsigned int kind,void *data,void *query,void *extra)
{(void)kind;(void)data;(void)query;(void)extra;++statements;return 0;}
static void sql(sqlite3 *db,const char *query)
{assert(sqlite3_exec(db,query,NULL,NULL,NULL)==SQLITE_OK);}
static int number(sqlite3 *db,const char *query)
{sqlite3_stmt *s;int n;assert(sqlite3_prepare_v2(db,query,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
static float width(nk_handle handle,float height,const char *text,int length)
{(void)handle;(void)text;return height*(float)length*0.5f;}
static void refresh(Fixture *f)
{
 size_t i;char query[200];
 assert(wena_sqlite_board_load(f->db,"b",f->board)&&f->board->card_count==4);
 for(i=0;i<4;++i){sprintf(query,"SELECT version FROM cards WHERE id='%s'",f->board->cards[i].id);f->versions[i]=(unsigned long)number(f->db,query);}
 assert(wena_card_mutation_init(&f->mutation,f->db,"u","b",f->board->cards,4));
}
static void frame(Fixture *f,int x,int y,int down)
{
 unsigned long source_revision;size_t i,ordinal,point;
 const struct nk_command *command;
 source_revision=0;
 for(i=0;i<4;++i)if(!strcmp(f->board->cards[i].id,f->drag.gesture.source_id))source_revision=f->versions[i];
 nk_clear(&f->ctx);nk_input_begin(&f->ctx);nk_input_motion(&f->ctx,x,y);
 nk_input_button(&f->ctx,NK_BUTTON_LEFT,x,y,down);nk_input_end(&f->ctx);
 wena_card_drag_begin(&f->ctx,&f->drag,f->board->cards,4,source_revision);
 if(nk_begin(&f->ctx,"Cards",nk_rect(0,0,500,500),NK_WINDOW_BORDER)){
  ordinal=0;
  for(i=0;i<4;++i){
   const WenaCard *card;card=&f->board->cards[i];
   if(strcmp(card->list_id,"l1"))continue;
   if(!card->archived){
    nk_layout_row_dynamic(&f->ctx,28,1);nk_label(&f->ctx,card->title,NK_TEXT_LEFT);
    wena_card_drag_handle(&f->ctx,&f->drag,f->board->cards,4,card,ordinal,f->versions[i],1);
   }
   ++ordinal;
  }
 }
 nk_end(&f->ctx);wena_card_drag_end(&f->ctx,&f->drag);
 point=0;
 nk_foreach(command,&f->ctx)if(command->type==NK_COMMAND_TEXT){
  const struct nk_command_text *text;text=(const struct nk_command_text *)command;
  if(text->length==14&&!memcmp(text->string,"Move selection",14)){
   assert(point<2);f->points[point++]=nk_vec2(text->x+text->w*0.5f,text->y+text->h*0.5f);
  }
 }
 assert(point==2);
}
static void drag(Fixture *f,int from,int to)
{
 struct nk_vec2 source,target;
 frame(f,0,0,0);source=f->points[from];target=f->points[to];
 frame(f,(int)source.x,(int)source.y,1);assert(f->drag.gesture.active&&f->drag.order);
 frame(f,(int)target.x,(int)target.y,1);assert(!f->drag.gesture.pending);
 frame(f,(int)target.x,(int)target.y,0);assert(f->drag.gesture.pending&&!f->drag.gesture.active);
}
int main(int argc,char **argv)
{
 Fixture f;FILE *file;unsigned char *schema;long size;WenaCard before[4];
 const char *hash="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
 memset(&f,0,sizeof(f));assert(argc==2);
 file=fopen(argv[1],"rb");assert(file);assert(!fseek(file,0,SEEK_END));size=ftell(file);assert(size>0);rewind(file);
 schema=(unsigned char *)malloc((size_t)size);assert(schema&&fread(schema,1,(size_t)size,file)==(size_t)size);fclose(file);
 assert(wena_sqlite_open(":memory:",schema,(size_t)size,hash,&f.db));free(schema);
 sql(f.db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l1','b','One',0,1),('l2','b','Two',1,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1)");
 sql(f.db,"INSERT INTO cards VALUES('c0','b','s','l1','Same title',0,0,1),('c1','b','s','l1','Archived',3,1,1),('c2','b','s','l1','Same title',8,0,1),('other','b','s','l2','Other',0,0,1)");
 f.board=(WenaSqliteBoardSnapshot *)calloc(1,sizeof(*f.board));assert(f.board);refresh(&f);
 f.font.height=13;f.font.width=width;assert(nk_init_default(&f.ctx,&f.font));
 assert(sqlite3_trace_v2(f.db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 drag(&f,0,1);assert(!statements&&!strcmp(f.drag.gesture.source_id,"c0")&&f.drag.gesture.target_position==2);
 assert(sqlite3_trace_v2(f.db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_card_drag_apply(&f.drag,&f.mutation)==1&&!f.drag.order&&!f.drag.gesture.pending);
 assert(!wena_card_drag_apply(&f.drag,&f.mutation));
 assert(number(f.db,"SELECT position FROM cards WHERE id='c0'")==2);
 assert(number(f.db,"SELECT position FROM cards WHERE id='c1'")==0);
 refresh(&f);drag(&f,1,0);memcpy(before,f.board->cards,sizeof(before));
 sql(f.db,"CREATE TRIGGER reject_card_drag BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(wena_card_drag_apply(&f.drag,&f.mutation)==-1&&f.drag.error&&!f.drag.order);
 assert(!memcmp(before,f.board->cards,sizeof(before))&&!wena_card_drag_apply(&f.drag,&f.mutation));
 sql(f.db,"DROP TRIGGER reject_card_drag");f.drag.error=0;
 drag(&f,1,0);sql(f.db,"UPDATE cards SET position=11 WHERE id='c1'");
 assert(wena_card_drag_apply(&f.drag,&f.mutation)==-1&&!memcmp(before,f.board->cards,sizeof(before)));
 assert(wena_board_reload(&f.mutation,f.board));
 refresh(&f);f.drag.error=0;drag(&f,1,0);
 assert(wena_card_drag_apply(&f.drag,&f.mutation)==1);
 assert(number(f.db,"SELECT position FROM cards WHERE id='c0'")==0);
 assert(number(f.db,"SELECT version FROM cards WHERE id='other'")==1);
 refresh(&f);frame(&f,0,0,0);
 frame(&f,(int)f.points[0].x,(int)f.points[0].y,1);assert(f.drag.order);
 f.board->cards[0].sort+=0.5;
 frame(&f,(int)f.points[1].x,(int)f.points[1].y,0);
 assert(!f.drag.gesture.pending&&!f.drag.order);
 wena_card_drag_cancel(&f.drag);
 refresh(&f);memcpy(before,f.board->cards,sizeof(before));
 sql(f.db,"UPDATE cards SET position=0.5 WHERE id='c1'");
 assert(!wena_board_reload(&f.mutation,f.board)&&!memcmp(before,f.board->cards,sizeof(before)));
 sql(f.db,"UPDATE cards SET position=2 WHERE id='c1'");
 assert(wena_card_mutation_set_create_cache(&f.mutation,&f.board->card_count,WENA_SQLITE_BOARD_MAX_CARDS));
 sql(f.db,"INSERT INTO cards VALUES('new','b','s','l2','New',1,0,1)");
 assert(wena_board_reload(&f.mutation,f.board)&&f.board->card_count==5&&f.mutation.card_count==5);
 assert(f.mutation.cards==f.board->cards&&f.mutation.published_card_count==&f.board->card_count);
 nk_free(&f.ctx);free(f.board);
 assert(sqlite3_close(f.db)==SQLITE_OK);
 puts("Real card drag: archived ordinals, exact IDs, zero SQL rendering, rollback, stale order and consumed intents passed");
 return 0;
}
