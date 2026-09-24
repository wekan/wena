#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/directory_picker.h"
#include "../server/sqlite_directory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long statements;
static int deny_reads,wrong_scope;
static int load(void *ctx,WenaDirectoryKind kind,const char *board,size_t page,size_t size,WenaDirectoryPage *out)
{
 int valid;valid=wena_sqlite_directory_read(ctx,kind,board,page,size,out);
 if(valid&&wrong_scope)strcpy(out->board_id,"b00");
 return valid;
}
static int trace(unsigned int kind,void *data,void *query,void *extra)
{(void)kind;(void)data;(void)query;(void)extra;++statements;return 0;}
static int authorize(void *data,int action,const char *first,const char *second,const char *db,const char *trigger)
{(void)data;(void)first;(void)second;(void)db;(void)trigger;return deny_reads&&action==SQLITE_READ?SQLITE_DENY:SQLITE_OK;}
static float width(nk_handle handle,float height,const char *text,int length)
{(void)handle;(void)text;return height*(float)length*0.5f;}
static void sql(sqlite3 *db,const char *query)
{assert(sqlite3_exec(db,query,NULL,NULL,NULL)==SQLITE_OK);}
static void draw(struct nk_context *ctx,WenaDirectoryPicker *picker)
{
 if(nk_begin(ctx,"Choose",nk_rect(0,0,600,600),NK_WINDOW_BORDER))
  (void)wena_directory_picker_render(ctx,picker);
 nk_end(ctx);
}
static void frame(struct nk_context *ctx,WenaDirectoryPicker *picker)
{nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);draw(ctx,picker);}
static struct nk_vec2 label(struct nk_context *ctx,const char *value)
{
 const struct nk_command *command;
 nk_foreach(command,ctx)if(command->type==NK_COMMAND_TEXT){
  const struct nk_command_text *text;text=(const struct nk_command_text *)command;
  if((size_t)text->length==strlen(value)&&!memcmp(text->string,value,(size_t)text->length))
   return nk_vec2(text->x+text->w*0.5f,text->y+text->h*0.5f);
 }
 fprintf(stderr,"Missing picker label: %s\n",value);assert(0);return nk_vec2(0,0);
}
static void click(struct nk_context *ctx,WenaDirectoryPicker *picker,const char *value)
{
 struct nk_vec2 point;int down;point=label(ctx,value);
 for(down=1;down>=0;--down){
  nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);
  nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(ctx);draw(ctx,picker);
 }
}
int main(int argc,char **argv)
{
 sqlite3 *db;WenaSqliteDirectoryReader reader;WenaDirectoryPicker picker;
 struct nk_context ctx;struct nk_user_font font;FILE *file;char *schema,query[200];long size;
 unsigned long before;int i;
 assert(argc==2);file=fopen(argv[1],"rb");assert(file&&!fseek(file,0,SEEK_END));size=ftell(file);assert(size>0);rewind(file);
 schema=(char*)malloc((size_t)size+1);assert(schema&&fread(schema,1,(size_t)size,file)==(size_t)size);schema[size]=0;fclose(file);
 assert(sqlite3_open(":memory:",&db)==SQLITE_OK);sql(db,schema);free(schema);
 sql(db,"INSERT INTO actors VALUES('u','Local user',1),('v','Other user',2)");
 for(i=0;i<17;++i){sprintf(query,"INSERT INTO boards VALUES('b%02d','Board %02d',1)",i,i);sql(db,query);}
 assert(wena_sqlite_directory_reader_init(&reader,db,"u"));
 assert(wena_directory_picker_init(&picker,8,load,&reader));
 assert(wena_directory_picker_open(&picker,WENA_DIRECTORY_BOARDS));
 memset(&font,0,sizeof(font));font.height=13;font.width=width;assert(nk_init_default(&ctx,&font));
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 frame(&ctx,&picker);label(&ctx,"Loading, please wait.");assert(!statements&&picker.read_pending);
 assert(wena_directory_picker_poll(&picker)==1&&picker.page.count==8);
 before=statements;frame(&ctx,&picker);label(&ctx,"Board 00");label(&ctx,"b00");
 click(&ctx,&picker,"Next Page");
 assert(picker.table.page==1&&picker.read_pending&&!picker.selection_pending&&statements==before);
 label(&ctx,"Loading, please wait.");
 sql(db,"UPDATE boards SET title='Shared board' WHERE id IN ('b09','b10')");
 assert(wena_directory_picker_poll(&picker)==1&&picker.page.first==8);
 before=statements;frame(&ctx,&picker);click(&ctx,&picker,"Shared board");
 assert(picker.selection_pending&&!strcmp(picker.selected.id,"b09")&&picker.selected.version==1&&statements==before);
 picker.selection_pending=0;
 click(&ctx,&picker,"Next Page");assert(picker.table.page==2&&picker.read_pending);
 deny_reads=1;assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);
 assert(wena_directory_picker_poll(&picker)==-1&&picker.error&&!picker.read_pending);
 before=statements;
 for(i=0;i<50;++i){frame(&ctx,&picker);assert(!wena_directory_picker_poll(&picker));}
 assert(statements==before);
 click(&ctx,&picker,"Refresh");assert(picker.read_pending&&statements==before);
 deny_reads=0;assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);
 assert(wena_directory_picker_poll(&picker)==1&&picker.page.count==1&&!picker.error);
 frame(&ctx,&picker);label(&ctx,"Board 16");
 assert(wena_directory_picker_open(&picker,WENA_DIRECTORY_ACTORS)&&!picker.selection_pending&&!picker.loaded);
 assert(wena_directory_picker_poll(&picker)==1&&picker.page.count==2);
 before=statements;frame(&ctx,&picker);click(&ctx,&picker,"Other user");
 assert(picker.selection_pending&&!strcmp(picker.selected.id,"v")&&picker.selected.version==2&&statements==before);
 sql(db,"INSERT INTO lists VALUES('l','b01','List',0,1);INSERT INTO swimlanes VALUES('s','b01','Lane',0,1);"
  "INSERT INTO cards VALUES('c','b01','s','l','A card',0,0,1)");
 assert(!wena_directory_picker_open(&picker,WENA_DIRECTORY_CARDS));
 assert(wena_directory_picker_open_scoped(&picker,WENA_DIRECTORY_CARDS,"b01"));
 wrong_scope=1;assert(wena_directory_picker_poll(&picker)==-1&&picker.error&&!picker.loaded);
 frame(&ctx,&picker);click(&ctx,&picker,"Refresh");wrong_scope=0;
 assert(wena_directory_picker_poll(&picker)==1&&picker.page.count==1);
 before=statements;frame(&ctx,&picker);click(&ctx,&picker,"A card");
 assert(picker.selection_pending&&!strcmp(picker.selected.id,"c")&&statements==before);
 assert(wena_directory_picker_open_scoped(&picker,WENA_DIRECTORY_CARDS,picker.board_id));
 assert(!strcmp(picker.board_id,"b01")&&wena_directory_picker_poll(&picker)==1);
 wena_directory_picker_close(&picker);assert(!picker.open&&!picker.selection_pending&&!wena_directory_picker_poll(&picker));
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 nk_free(&ctx);assert(sqlite3_close(db)==SQLITE_OK);
 puts("Shared board/actor picker: page-on-demand, exact selections, no SQL drawing and explicit retry passed");
 return 0;
}
