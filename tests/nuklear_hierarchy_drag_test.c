#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/hierarchy_drag.h"
#include "../client/features/hierarchy_move_mutation.h"
#include "../imports/ui/page_contract.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Fixture {
 struct nk_context ctx;struct nk_user_font font;struct nk_vec2 points[3];
 sqlite3 *db;WenaSqliteBoardSnapshot *snapshot;WenaBoardLayout layout;
 WenaHierarchyMoveMutation mutation;WenaHierarchyDrag drag;WenaHierarchyKind kind;
 int enabled,hide_source;
} Fixture;
static unsigned long statements;
static int trace(unsigned int k,void *p,void *q,void *e)
{(void)k;(void)p;(void)q;(void)e;++statements;return 0;}
static void sql(sqlite3 *db,const char *q)
{assert(sqlite3_exec(db,q,NULL,NULL,NULL)==SQLITE_OK);}
static int number(sqlite3 *db,const char *q)
{sqlite3_stmt *s;int n;assert(sqlite3_prepare_v2(db,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static float width(nk_handle h,float z,const char *s,int n)
{(void)h;(void)s;return z*(float)n*0.5f;}
static const char *id(Fixture *f,size_t i)
{return f->kind==WENA_HIERARCHY_LIST?f->snapshot->lists[i].id:f->snapshot->swimlanes[i].id;}
static void frame(Fixture *f,struct nk_vec2 point,int down,int escape)
{
 unsigned long before;size_t i,found,visible[3],count;const struct nk_command *command;const char *label;
 before=statements;count=0;
 nk_clear(&f->ctx);nk_input_begin(&f->ctx);nk_input_motion(&f->ctx,(int)point.x,(int)point.y);
 nk_input_button(&f->ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);
 nk_input_key(&f->ctx,NK_KEY_TEXT_RESET_MODE,escape);nk_input_end(&f->ctx);
 wena_hierarchy_drag_begin(&f->ctx,&f->drag,&f->layout);
 if(nk_begin(&f->ctx,"Hierarchy",nk_rect(0,0,500,500),NK_WINDOW_BORDER)){
  for(i=0;i<3;++i){
   if((f->kind==WENA_HIERARCHY_LIST?f->snapshot->lists[i].archived:f->snapshot->swimlanes[i].archived))continue;
   if(f->hide_source&&!strcmp(f->drag.gesture.source_id,id(f,i)))continue;
   visible[count++]=i;
   wena_hierarchy_drag_handle(&f->ctx,&f->drag,&f->layout,f->kind,id(f,i),i,f->enabled);
  }
 }
 nk_end(&f->ctx);wena_hierarchy_drag_end(&f->ctx,&f->drag);assert(statements==before);
 found=0;label=wena_ui_control_text(f->kind==WENA_HIERARCHY_LIST?WENA_UI_MOVE_LIST_TO:WENA_UI_MOVE_SWIMLANE_TO);
 nk_foreach(command,&f->ctx)if(command->type==NK_COMMAND_TEXT){
  const struct nk_command_text *text;text=(const struct nk_command_text*)command;
  if((size_t)text->length==strlen(label)&&!memcmp(text->string,label,(size_t)text->length)){
   assert(found<3);f->points[visible[found++]]=nk_vec2(text->x+text->w*0.5f,text->y+text->h*0.5f);
  }
 }
 assert(found==count);
}
static void press(Fixture *f,int from)
{
 frame(f,nk_vec2(0,0),0,0);frame(f,f->points[from],1,0);
 assert(f->drag.gesture.active&&!f->drag.move.visible);
 assert(!wena_hierarchy_drag_process(&f->drag,&f->layout)&&f->drag.move.visible);
}
static void release(Fixture *f,int to)
{
 frame(f,f->points[to],1,0);frame(f,f->points[to],0,0);assert(f->drag.gesture.pending);
}
static void refresh(Fixture *f)
{
 wena_hierarchy_drag_cancel(&f->drag);f->drag.error=0;
 assert(wena_sqlite_board_load(f->db,"b",f->snapshot));
}
int main(int argc,char **argv)
{
 Fixture f;int kind;FILE *file;char *schema,query[512];const char *table;long size;
 unsigned long before;WenaId captured[3];size_t i;struct nk_vec2 source;
 assert(argc==2);file=fopen(argv[1],"rb");assert(file&&!fseek(file,0,SEEK_END));size=ftell(file);assert(size>0);rewind(file);
 schema=(char*)malloc((size_t)size+1);assert(schema&&fread(schema,1,(size_t)size,file)==(size_t)size);schema[size]=0;fclose(file);
 for(kind=0;kind<2;++kind){
 memset(&f,0,sizeof(f));f.enabled=1;f.kind=kind?WENA_HIERARCHY_SWIMLANE:WENA_HIERARCHY_LIST;table=kind?"swimlanes":"lists";
 assert(sqlite3_open(":memory:",&f.db)==SQLITE_OK);sql(f.db,"PRAGMA foreign_keys=ON");sql(f.db,schema);
 sql(f.db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);"
 "INSERT INTO lists VALUES('one','b','Shared',0,1),('two','b','Shared',1,1),('three','b','Shared',2,1);"
 "INSERT INTO swimlanes VALUES('one','b','Shared',0,1),('two','b','Shared',1,1),('three','b','Shared',2,1)");
 f.snapshot=(WenaSqliteBoardSnapshot*)calloc(1,sizeof(*f.snapshot));assert(f.snapshot&&wena_sqlite_board_load(f.db,"b",f.snapshot));
 f.layout.board=&f.snapshot->board;f.layout.lists=f.snapshot->lists;f.layout.list_count=3;f.layout.swimlanes=f.snapshot->swimlanes;f.layout.swimlane_count=3;
 assert(wena_hierarchy_move_mutation_init(&f.mutation,f.db,"u","b",f.snapshot));
 wena_hierarchy_drag_init(&f.drag,wena_hierarchy_move_mutation_load,wena_hierarchy_move_mutation_move,&f.mutation);
 f.font.height=13;f.font.width=width;assert(nk_init_default(&f.ctx,&f.font));assert(sqlite3_trace_v2(f.db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 press(&f,0);release(&f,2);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==1);
 assert(!strcmp(id(&f,2),"one")&&!strcmp(id(&f,0),"two"));
 before=statements;assert(!wena_hierarchy_drag_process(&f.drag,&f.layout)&&statements==before);
 press(&f,2);release(&f,0);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==1&&!strcmp(id(&f,0),"one"));
 /* Threshold, Escape, read-only and absent source cancel without writes. */
 before=(unsigned long)number(f.db,"SELECT count(*) FROM idempotency_keys");
 press(&f,0);source=f.points[0];frame(&f,source,0,0);assert(!f.drag.gesture.pending&&!f.drag.move.visible);
 press(&f,0);frame(&f,f.points[2],1,1);assert(!f.drag.gesture.active);frame(&f,nk_vec2(0,0),0,0);
 f.enabled=0;frame(&f,f.points[0],1,0);assert(!f.drag.gesture.active);frame(&f,nk_vec2(0,0),0,0);f.enabled=1;
 press(&f,0);f.hide_source=1;frame(&f,f.points[2],1,0);assert(!f.drag.gesture.active);f.hide_source=0;frame(&f,nk_vec2(0,0),0,0);
 assert((unsigned long)number(f.db,"SELECT count(*) FROM idempotency_keys")==before);
 /* Stale database source revision and late metadata failure preserve cache. */
 press(&f,0);sprintf(query,"UPDATE %s SET version=version+1 WHERE id='one'",table);sql(f.db,query);
 release(&f,2);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==-1&&f.drag.error&&!strcmp(id(&f,0),"one"));
 before=statements;assert(!wena_hierarchy_drag_process(&f.drag,&f.layout)&&statements==before);refresh(&f);
 press(&f,0);for(i=0;i<3;++i)strcpy(captured[i],id(&f,i));
 sql(f.db,"CREATE TRIGGER late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
 release(&f,2);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==-1);
 for(i=0;i<3;++i)assert(!strcmp(captured[i],id(&f,i)));
 sprintf(query,"SELECT position FROM %s WHERE id='one'",table);assert(number(f.db,query)==0);
 sql(f.db,"DROP TRIGGER late");refresh(&f);
 /* A changed sibling fingerprint rejects even with unchanged source revision. */
 press(&f,0);sprintf(query,"UPDATE %s SET position=9 WHERE id='two';UPDATE %s SET position=1 WHERE id='three';UPDATE %s SET position=2 WHERE id='two'",table,table,table);sql(f.db,query);
 release(&f,2);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==-1&&!strcmp(id(&f,1),"two"));refresh(&f);
 assert(!strcmp(id(&f,1),"three"));press(&f,0);release(&f,2);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==1);
 /* Local source disappearance/change is detected before rendering a drop. */
 press(&f,0);if(kind)f.snapshot->swimlanes[0].sort=9;else f.snapshot->lists[0].sort=9;
 frame(&f,f.points[2],1,0);assert(!f.drag.gesture.active&&!f.drag.move.visible);refresh(&f);
 frame(&f,nk_vec2(0,0),0,0);
 if(!kind){
  /* Real mouse drop uses complete ordinals across a hidden middle list. */
  sprintf(query,"INSERT INTO list_archive_state VALUES('%s','b',1,123456)",id(&f,1));sql(f.db,query);
  refresh(&f);strcpy(captured[0],id(&f,0));strcpy(captured[1],id(&f,1));
  press(&f,0);assert(f.drag.move.count==3);release(&f,2);
  assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==1&&!strcmp(id(&f,2),captured[0]));
  assert(!strcmp(id(&f,0),captured[1])&&f.snapshot->lists[0].archived);
  press(&f,2);release(&f,1);assert(wena_hierarchy_drag_process(&f.drag,&f.layout)==1);
  assert(!strcmp(id(&f,1),captured[0])&&f.snapshot->lists[0].archived);
  assert(number(f.db,"SELECT archived_at FROM list_archive_state")==123456);
  /* A source becoming archived during a drag cancels before applying. */
  press(&f,1);f.snapshot->lists[1].archived=1;
  frame(&f,f.points[2],1,0);assert(!f.drag.gesture.active&&!f.drag.move.visible);
  refresh(&f);sql(f.db,"DELETE FROM list_archive_state");refresh(&f);
  frame(&f,nk_vec2(0,0),0,0);
 }
 strcpy(f.mutation.actor_id,"missing");frame(&f,f.points[0],1,0);
 assert(f.drag.gesture.active&&wena_hierarchy_drag_process(&f.drag,&f.layout)==-1&&f.drag.error);
 before=statements;assert(!wena_hierarchy_drag_process(&f.drag,&f.layout)&&statements==before);
 wena_hierarchy_drag_cancel(&f.drag);nk_free(&f.ctx);free(f.snapshot);assert(sqlite3_close(f.db)==SQLITE_OK);
 }
 free(schema);puts("Shared hierarchy drag: lists/swimlanes, SQL-free frames, captured revisions, cancel, fingerprint, rollback and cache publication passed");return 0;
}
