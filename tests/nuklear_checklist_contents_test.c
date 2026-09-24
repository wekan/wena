#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/components/cards/checklist_contents.h"
#include "../client/components/cards/card_body.h"
#include "../client/features/checklist_mutation.h"
#include "../client/features/checklists.h"
#include "../imports/preferences/sections.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long statements;
static WenaChecklistCompletionIntent *active_intent;
static WenaCardSectionControl *active_sections;
static WenaChecklistInlineEdit *active_edit;
static WenaChecklistDrag *active_drag;
static WenaCard *extra_card;
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
 if(active_drag)wena_reorder_drag_begin(ctx,&active_drag->gesture);
 if(nk_begin(ctx,"Preview",nk_rect(0,0,600,700),NK_WINDOW_BORDER)) {
  action=wena_checklist_contents_render_editable(ctx,contents,card,board_default,active_intent,active_sections,active_edit,active_drag);
  if(extra_card) {
   nk_layout_row_dynamic(ctx,28,1);nk_label(ctx,extra_card->title,NK_TEXT_LEFT);
   action|=wena_checklist_contents_render_editable(ctx,contents,extra_card,board_default,
    active_intent,active_sections,active_edit,active_drag);
  }
 }
 nk_end(ctx);if(active_drag)wena_reorder_drag_end(ctx,&active_drag->gesture);return action;
}
static void frame(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,int board_default)
{nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);assert(!draw(ctx,contents,card,board_default));}
static void click_item(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,const char *text)
{
 struct nk_vec2 point;int down;
 assert(label(ctx,text,&point));
 for(down=1;down>=0;--down){
  nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);
  nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(ctx);
  assert(!draw(ctx,contents,card,1));
 }
}
static void panel_frame(struct nk_context *ctx,WenaChecklistsState *panel,WenaCard *card)
{
 nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);
 assert(wena_checklists_render(ctx,panel,card,1,640,700));
}
static void shared_sections(sqlite3 *db,struct nk_context *ctx,WenaChecklistBoardContents **contents,
 WenaChecklistMutation *adapter,WenaCard *card)
{
 WenaCardSectionsSnapshot *preferences;WenaCardSectionControl controls;
 WenaChecklistsState panel;struct nk_vec2 point;int down;
 preferences=NULL;memset(&controls,0,sizeof(controls));
 sql(db,"UPDATE checklists SET hide_all_items=0,hide_checked_items=0 WHERE id='inherit'");
 assert(wena_checklist_contents_load(db,"u","b",contents));
 assert(wena_card_sections_load(db,"u","b",&preferences));controls.snapshot=preferences;
 active_sections=&controls;active_intent=NULL;
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 frame(ctx,*contents,card,1);assert(label(ctx,"[ ] Todo",NULL));
 click_item(ctx,*contents,card,"Collapse");assert(controls.pending&&controls.collapsed&&!strcmp(controls.key,"checklist-inherit"));
 frame(ctx,*contents,card,1);assert(!label(ctx,"[ ] Todo",NULL)&&!statements);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_card_section_save(db,"u","b",controls.card_id,controls.key,controls.version,controls.collapsed));controls.pending=0;
 assert(wena_card_sections_load(db,"u","b",&preferences));controls.snapshot=preferences;
 wena_checklists_init(&panel,wena_checklist_mutation_load,NULL,adapter);panel.sections=&controls;
 assert(wena_checklists_open(&panel,card));panel_frame(ctx,&panel,card);
 assert(label(ctx,"Inherited",NULL)&&!label(ctx,"Todo",NULL));
 assert(label(ctx,"Uncollapse",&point));
 for(down=1;down>=0;--down){nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(ctx);assert(wena_checklists_render(ctx,&panel,card,1,640,700));}
 assert(controls.pending&&!controls.collapsed&&controls.version==1);
 assert(wena_card_section_save(db,"u","b",controls.card_id,controls.key,controls.version,controls.collapsed));controls.pending=0;
 assert(wena_card_sections_load(db,"u","b",&preferences));controls.snapshot=preferences;
 wena_checklists_close(&panel);frame(ctx,*contents,card,1);assert(label(ctx,"[ ] Todo",NULL));
 controls.readonly=1;frame(ctx,*contents,card,1);click_item(ctx,*contents,card,"Collapse");assert(!controls.pending);
 active_sections=NULL;wena_card_sections_free(preferences);
}
static void inline_forms(sqlite3 *db,struct nk_context *ctx,
 WenaChecklistBoardContents **contents,WenaChecklistMutation *adapter,WenaCard *card)
{
 WenaChecklistInlineEdit edit;const WenaChecklistContents *list;int i;
 memset(&edit,0,sizeof(edit));active_edit=&edit;
 frame(ctx,*contents,card,1);click_item(ctx,*contents,card,"Rename");
 assert(edit.action==WENA_CHECKLIST_RENAME&&!strcmp(edit.checklist_id,"inherit"));
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 frame(ctx,*contents,card,1);assert(label(ctx,"Save",NULL)&&label(ctx,"Cancel",NULL));
 click_item(ctx,*contents,card,"Cancel");assert(!edit.action&&!statements);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 frame(ctx,*contents,card,1);click_item(ctx,*contents,card,"Rename");
 strcpy(edit.input,"Renamed");edit.length=7;frame(ctx,*contents,card,1);
 click_item(ctx,*contents,card,"Save");assert(edit.pending);
 assert(wena_checklist_mutation_inline(adapter,&edit)==1&&!edit.action);
 assert(!wena_checklist_mutation_inline(adapter,&edit));
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");assert(!strcmp(list->checklist.title,"Renamed"));
 assert(wena_checklist_inline_begin(&edit,*contents,card,list,0,WENA_CHECKLIST_RENAME_ITEM));
 strcpy(edit.input,"Changed item");edit.length=12;edit.pending=1;
 assert(wena_checklist_mutation_inline(adapter,&edit)==1);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");assert(!strcmp(list->items[0].title,"Changed item")&&!list->items[0].is_finished);
 assert(wena_checklist_inline_begin(&edit,*contents,card,list,0,WENA_CHECKLIST_ADD_ITEM));
 strcpy(edit.input,"  Added  ");edit.length=9;edit.pending=1;
 assert(wena_checklist_mutation_inline(adapter,&edit)==1);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");assert(list->item_count==3&&!strcmp(list->items[2].title,"Added"));
 assert(wena_checklist_inline_begin(&edit,*contents,card,list,0,WENA_CHECKLIST_RENAME));
 edit.length=0;edit.pending=1;assert(wena_checklist_mutation_inline(adapter,&edit)==-1&&edit.error&&!edit.pending);
 for(i=0;i<129;++i)edit.input[i]='a';
 edit.length=129;edit.pending=1;assert(wena_checklist_mutation_inline(adapter,&edit)==-1&&edit.length==129);
 strcpy(edit.input,"Retry");edit.length=5;edit.pending=1;
 sql(db,"CREATE TRIGGER reject_inline BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(wena_checklist_mutation_inline(adapter,&edit)==-1&&!edit.pending&&edit.action);
 assert(!wena_checklist_mutation_inline(adapter,&edit));
 sql(db,"DROP TRIGGER reject_inline");edit.pending=1;
 assert(wena_checklist_mutation_inline(adapter,&edit)==1);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");
 assert(wena_checklist_inline_begin(&edit,*contents,card,list,0,WENA_CHECKLIST_RENAME));
 sql(db,"UPDATE cards SET version=version+1 WHERE id='c'");
 strcpy(edit.input,"Stale");edit.length=5;edit.pending=1;
 assert(wena_checklist_mutation_inline(adapter,&edit)==-1&&!edit.pending);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 wena_checklist_inline_sync(&edit,*contents,1);assert(edit.action);
 wena_checklist_inline_sync(&edit,*contents,0);assert(!edit.action);
 list=wena_checklist_contents_find(*contents,"c");
 assert(wena_checklist_inline_begin(&edit,*contents,card,list,0,WENA_CHECKLIST_ADD_ITEM));
 frame(ctx,*contents,card,1);
 click_item(ctx,*contents,card,"Each line of text becomes one of the checklist items");
 assert(edit.action==WENA_CHECKLIST_ADD_ITEMS&&!edit.pending);
 strcpy(edit.input,"  Batch one  \n\n Batch two ");edit.length=(int)strlen(edit.input);
 frame(ctx,*contents,card,1);
 click_item(ctx,*contents,card,"Each line of text becomes one of the checklist items");
 assert(edit.action==WENA_CHECKLIST_ADD_ITEMS&&edit.error&&strchr(edit.input,'\n'));
 frame(ctx,*contents,card,1);click_item(ctx,*contents,card,"Save");assert(edit.pending);
 sql(db,"CREATE TRIGGER reject_batch BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(wena_checklist_mutation_inline(adapter,&edit)==-1&&!edit.pending);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 assert(wena_checklist_contents_find(*contents,"c")->item_count==3);
 sql(db,"DROP TRIGGER reject_batch");edit.pending=1;
 assert(wena_checklist_mutation_inline(adapter,&edit)==1&&!edit.action);
 assert(!wena_checklist_mutation_inline(adapter,&edit));
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");
 assert(list->item_count==5&&!strcmp(list->items[3].title,"Batch one")&&!strcmp(list->items[4].title,"Batch two"));
 assert(wena_checklist_inline_begin(&edit,*contents,card,list,0,WENA_CHECKLIST_ADD_ITEM));
 edit.action=WENA_CHECKLIST_ADD_ITEMS;
 strcpy(edit.input,"a\nb\nc\nd\ne\nf\ng\nh\ni");edit.length=(int)strlen(edit.input);edit.pending=1;
 assert(wena_checklist_mutation_inline(adapter,&edit)==-1&&!edit.pending&&edit.action);
 wena_checklist_inline_cancel(&edit);
 active_edit=NULL;
}
static struct nk_vec2 nth_label(struct nk_context *ctx,const char *value,int wanted)
{
 const struct nk_command *command;
 nk_foreach(command,ctx)if(command->type==NK_COMMAND_TEXT){
  const struct nk_command_text *text;text=(const struct nk_command_text *)command;
  if((size_t)text->length==strlen(value)&&!memcmp(text->string,value,(size_t)text->length)&&wanted--==0)
   return nk_vec2(text->x+text->w*0.5f,text->y+text->h*0.5f);
 }
 assert(0);return nk_vec2(0,0);
}
static void drag_between(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,const char *label_text,int from,int to)
{
 struct nk_vec2 points[2];int step;
 frame(ctx,contents,card,1);
 points[0]=nth_label(ctx,label_text,from);points[1]=nth_label(ctx,label_text,to);
 for(step=0;step<3;++step){
  struct nk_vec2 point;point=points[step?1:0];
  nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);
  nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,step<2);nk_input_end(ctx);
  assert(!draw(ctx,contents,card,1));
 }
}
static void drag_reordering(sqlite3 *db,struct nk_context *ctx,
 WenaChecklistBoardContents **contents,WenaChecklistMutation *adapter,WenaCard *card)
{
 WenaChecklistDrag drag;const WenaChecklistContents *list;
 memset(&drag,0,sizeof(drag));active_drag=&drag;
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 drag_between(ctx,*contents,card,"Move selection",0,1);
 assert(drag.gesture.pending&&drag.action==WENA_CHECKLIST_REORDER_ITEM);
 assert(!strcmp(drag.source.item_id,"todo")&&drag.gesture.target_position==1&&!statements);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_checklist_mutation_drag(adapter,&drag)==1&&!drag.gesture.pending);
 assert(!wena_checklist_mutation_drag(adapter,&drag));
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");
 assert(!strcmp(list->items[0].id,"done")&&!strcmp(list->items[1].id,"todo"));
 drag_between(ctx,*contents,card,"Move selection",1,0);
 sql(db,"CREATE TRIGGER reject_drag BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(wena_checklist_mutation_drag(adapter,&drag)==-1&&drag.error&&!drag.gesture.pending);
 assert(!wena_checklist_mutation_drag(adapter,&drag));
 sql(db,"DROP TRIGGER reject_drag");
 assert(wena_checklist_contents_load(db,"u","b",contents));
 assert(!strcmp(wena_checklist_contents_find(*contents,"c")->items[0].id,"done"));
 drag.error=0;drag_between(ctx,*contents,card,"Move selection",1,0);
 sql(db,"UPDATE cards SET version=version+1 WHERE id='c'");
 assert(wena_checklist_mutation_drag(adapter,&drag)==-1&&drag.error);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 drag.error=0;drag_between(ctx,*contents,card,"Move Checklist",0,1);
 assert(drag.gesture.pending&&drag.action==WENA_CHECKLIST_REORDER);
 assert(!strcmp(drag.source.checklist_id,"inherit")&&drag.gesture.target_position==2);
 assert(wena_checklist_mutation_drag(adapter,&drag)==1);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c");
 assert(!strcmp(list->checklist.id,"hidden")&&!strcmp(list->next->checklist.id,"shown"));
 assert(!strcmp(list->next->next->checklist.id,"inherit")&&list->next->next->item_count==5);
 active_drag=NULL;
}
static void drag_to_slot(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,const char *label_text,int from,const char *slot)
{
 struct nk_vec2 point;int step;
 frame(ctx,contents,card,1);point=nth_label(ctx,label_text,from);
 for(step=0;step<3;++step) {
  if(step==1)point.x+=8;
  if(step==2)point=nth_label(ctx,slot,0);
  nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);
  nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,step<2);nk_input_end(ctx);
  assert(!draw(ctx,contents,card,1));
 }
}
static void drag_to_destination(struct nk_context *ctx,WenaChecklistBoardContents *contents,
 WenaCard *card,const char *label_text,int from)
{drag_to_slot(ctx,contents,card,label_text,from,"Destination");}
static void drag_transfers(sqlite3 *db,struct nk_context *ctx,
 WenaChecklistBoardContents **contents,WenaChecklistMutation *adapter,WenaCard *card)
{
 WenaChecklistDrag drag;WenaCard target;const WenaChecklistContents *list;
 memset(&drag,0,sizeof(drag));active_drag=&drag;
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 drag_to_destination(ctx,*contents,card,"Move selection",0);
 assert(drag.gesture.pending&&drag.action==WENA_CHECKLIST_MOVE_ITEM&&!statements);
 assert(!strcmp(drag.source.item_id,"done")&&!strcmp(drag.target_checklist_id,"shown"));
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_checklist_mutation_drag(adapter,&drag)==1);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"c")->next;
 assert(!strcmp(list->checklist.id,"shown")&&list->item_count==1&&list->items[0].is_finished);
 assert(list->next->item_count==4);
 sql(db,"INSERT INTO cards VALUES('d','b','s','l','Destination card',1,0,1)");
 assert(wena_card_init(&target,"d","b","s","l","Destination card",1,0));extra_card=&target;
 assert(wena_checklist_contents_load(db,"u","b",contents));
 drag_to_destination(ctx,*contents,card,"Move Checklist",1);
 assert(drag.gesture.pending&&drag.action==WENA_CHECKLIST_MOVE);
 assert(!strcmp(drag.source.checklist_id,"inherit")&&!strcmp(drag.target_card_id,"d"));
 sql(db,"UPDATE cards SET version=version+1 WHERE id='d'");
 assert(wena_checklist_mutation_drag(adapter,&drag)==-1&&drag.error&&!drag.gesture.pending);
 assert(!wena_checklist_mutation_drag(adapter,&drag));
 assert(wena_checklist_contents_load(db,"u","b",contents));drag.error=0;
 drag_to_destination(ctx,*contents,card,"Move Checklist",1);
 sql(db,"CREATE TRIGGER reject_transfer_drag BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(wena_checklist_mutation_drag(adapter,&drag)==-1&&drag.error);
 sql(db,"DROP TRIGGER reject_transfer_drag");
 assert(wena_checklist_contents_load(db,"u","b",contents));
 assert(!wena_checklist_contents_find(*contents,"d"));drag.error=0;
 drag_to_destination(ctx,*contents,card,"Move Checklist",1);
 assert(wena_checklist_mutation_drag(adapter,&drag)==1&&!drag.gesture.pending);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"d");
 assert(list&&!strcmp(list->checklist.id,"inherit")&&list->item_count==4);
 assert(!strcmp(list->items[0].card_id,"d"));
 drag_to_slot(ctx,*contents,card,"Move selection",0,"Destination 1");
 assert(drag.gesture.pending&&drag.action==WENA_CHECKLIST_MOVE_ITEM&&!strcmp(drag.target_card_id,"d")&&drag.insert_at_position&&drag.gesture.target_position==0);
 assert(wena_checklist_mutation_drag(adapter,&drag)==1);
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"d");
 assert(list->item_count==5&&!strcmp(list->items[0].id,"done")&&list->items[0].is_finished);
 /* Hidden destination siblings still participate in exact insertion ordinals. */
 sql(db,"UPDATE checklists SET position=7,version=version+1 WHERE id='inherit';"
  "INSERT INTO checklists(id,board_id,card_id,title,position,show_on_minicard) VALUES('targethidden','b','d','Hidden destination',0,0);"
  "UPDATE cards SET version=version+1 WHERE id='d'");
 assert(wena_checklist_contents_load(db,"u","b",contents));
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 drag_to_slot(ctx,*contents,card,"Move Checklist",0,"Destination 2");
 assert(drag.gesture.pending&&drag.action==WENA_CHECKLIST_MOVE&&drag.insert_at_position&&drag.gesture.target_position==1&&!statements);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_checklist_mutation_drag(adapter,&drag)==1&&!wena_checklist_mutation_drag(adapter,&drag));
 assert(wena_checklist_contents_load(db,"u","b",contents));
 list=wena_checklist_contents_find(*contents,"d");
 assert(!strcmp(list->checklist.id,"targethidden")&&list->checklist.position==0);
 list=list->next;assert(!strcmp(list->checklist.id,"shown")&&list->checklist.position==1);
 list=list->next;assert(!strcmp(list->checklist.id,"inherit")&&list->checklist.position==2);
 extra_card=NULL;active_drag=NULL;
}
int main(int argc,char **argv)
{
 sqlite3 *db;FILE *file;long size;char *schema;
 struct nk_context ctx;struct nk_user_font font;struct nk_vec2 point;
 WenaChecklistCompletionIntent intent;WenaChecklistMutation adapter;
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
 /* Interactive rendering captures immutable scope/revisions, then saves once. */
 assert(wena_checklist_mutation_init(&adapter,db,"u","b"));
 memset(&intent,0,sizeof(intent));active_intent=&intent;
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 frame(&ctx,contents,&card,1);click_item(&ctx,contents,&card,"Todo");
 assert(intent.pending&&intent.is_finished&&!strcmp(intent.item_id,"todo"));
 assert(!strcmp(intent.card_id,"c")&&!strcmp(intent.checklist_id,"inherit")&&intent.item_version==1&&intent.card_version==1);
 assert(!statements&&!wena_checklist_contents_find(contents,"c")->items[0].is_finished);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_checklist_mutation_complete(&adapter,&intent)==1&&!intent.pending);
 statements=0;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 assert(!wena_checklist_mutation_complete(&adapter,&intent)&&!statements);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_checklist_contents_load(db,"u","b",&contents));
 assert(wena_checklist_contents_find(contents,"c")->items[0].is_finished);
 frame(&ctx,contents,&card,1);click_item(&ctx,contents,&card,"Todo");
 assert(intent.pending&&!intent.is_finished&&intent.item_version==2);
 sql(db,"CREATE TRIGGER reject_completion BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(wena_checklist_mutation_complete(&adapter,&intent)==-1&&!intent.pending);
 assert(!wena_checklist_mutation_complete(&adapter,&intent));sql(db,"DROP TRIGGER reject_completion");
 assert(wena_checklist_contents_load(db,"u","b",&contents));
 assert(wena_checklist_contents_find(contents,"c")->items[0].is_finished);
 frame(&ctx,contents,&card,1);click_item(&ctx,contents,&card,"Todo");
 sql(db,"UPDATE cards SET version=version+1 WHERE id='c'");
 assert(wena_checklist_mutation_complete(&adapter,&intent)==-1&&!intent.pending);
 assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);click_item(&ctx,contents,&card,"Todo");
 assert(wena_checklist_mutation_complete(&adapter,&intent)==1);
 assert(wena_checklist_contents_load(db,"u","b",&contents));
 assert(!wena_checklist_contents_find(contents,"c")->items[0].is_finished);
 active_intent=NULL;
 sql(db,"UPDATE checklists SET hide_checked_items=1 WHERE id='inherit'");assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);assert(label(&ctx,"[ ] Todo",NULL)&&!label(&ctx,"[x] Done",NULL));
 assert(contents->summary.cards[0].progress.finished==1&&contents->summary.cards[0].progress.total==3);
 sql(db,"UPDATE checklists SET hide_all_items=1 WHERE id='inherit'");assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);assert(label(&ctx,"Inherited",NULL)&&!label(&ctx,"[ ] Todo",NULL));
 shared_sections(db,&ctx,&contents,&adapter,&card);
 inline_forms(db,&ctx,&contents,&adapter,&card);
 drag_reordering(db,&ctx,&contents,&adapter,&card);
 drag_transfers(db,&ctx,&contents,&adapter,&card);
 sql(db,"UPDATE cards SET archived=1");assert(wena_checklist_contents_load(db,"u","b",&contents));
 frame(&ctx,contents,&card,1);assert(!label(&ctx,"Shown",NULL));
 wena_checklist_contents_free(contents);nk_free(&ctx);assert(sqlite3_close(db)==SQLITE_OK);
 puts("Real minicard previews: overrides, hidden/completed items, scoped actions and zero-SQL frames passed");return 0;
}
