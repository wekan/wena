#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_move.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int writes,reads,fail;
static unsigned long expected_position;
static int load(void *data,const char *board,const char *card,char *title,size_t capacity,unsigned long *version)
{(void)data;(void)capacity;assert(!strcmp(board,"b")&&!strcmp(card,"src"));strcpy(title,"Same");*version=1;++reads;return 1;}
static int append(void *data,const char *board,const char *card,unsigned long version,const char *list,const char *lane)
{(void)data;(void)board;(void)card;(void)version;(void)list;(void)lane;assert(0);return 0;}
static int insert(void *data,const char *board,const char *card,unsigned long version,const char *list,const char *lane,unsigned long position)
{(void)data;assert(!strcmp(board,"b")&&!strcmp(card,"src")&&version==1&&!strcmp(list,"dst")&&!strcmp(lane,"lane")&&position==expected_position);++writes;return !fail;}
static float width(nk_handle handle,float height,const char *text,int length)
{(void)handle;(void)text;return height*(float)length*.5f;}
static void render(struct nk_context *ctx,WenaCardMoveState *state,WenaBoardLayout *layout)
{const struct nk_command *command;if(state->visible)assert(wena_card_move_render(ctx,state,layout,800,600));assert(!ctx->current);nk_foreach(command,ctx){(void)command;}}
static void idle(struct nk_context *ctx,WenaCardMoveState *state,WenaBoardLayout *layout)
{nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);render(ctx,state,layout);}
static void click_at(struct nk_context *ctx,WenaCardMoveState *state,WenaBoardLayout *layout,struct nk_vec2 point)
{
 int down;for(down=1;down>=0;--down){nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(ctx);render(ctx,state,layout);}
}
static void click(struct nk_context *ctx,WenaCardMoveState *state,WenaBoardLayout *layout,const char *label)
{
 const struct nk_command *command;struct nk_vec2 point;int found;found=0;point=nk_vec2(0,0);
 nk_foreach(command,ctx)if(command->type==NK_COMMAND_TEXT){const struct nk_command_text *text;text=(const struct nk_command_text*)command;
  if((size_t)text->length==strlen(label)&&!memcmp(text->string,label,(size_t)text->length)){
   point=nk_vec2(text->x+text->w*.5f,text->y+text->h*.5f);found=1;break;}}
 if(!found)fprintf(stderr,"Missing control: %s\n",label);assert(found);click_at(ctx,state,layout,point);
}
static void increment(struct nk_context *ctx,WenaCardMoveState *state,WenaBoardLayout *layout)
{
 const struct nk_command *command;struct nk_vec2 point;point=nk_vec2(0,0);
 nk_foreach(command,ctx)if(command->type==NK_COMMAND_TRIANGLE_FILLED){
  const struct nk_command_triangle_filled *t;t=(const struct nk_command_triangle_filled*)command;
  point=nk_vec2((t->a.x+t->b.x+t->c.x)/3.0f,(t->a.y+t->b.y+t->c.y)/3.0f);}
 assert(point.x>0);click_at(ctx,state,layout,point);idle(ctx,state,layout);
}
static void choose(struct nk_context *ctx,WenaCardMoveState *state,WenaBoardLayout *layout,size_t count)
{
 int before;assert(wena_card_move_open(state,layout,"src"));before=reads;idle(ctx,state,layout);
 click(ctx,state,layout,"Source [src-list]");click(ctx,state,layout,"Destination [dst]");idle(ctx,state,layout);
 assert(state->destination_ready&&state->destination_count==count&&!state->insert_choice);
 click(ctx,state,layout,"Your Manual Order");idle(ctx,state,layout);
 assert(state->insert_choice&&!state->insert_position&&reads==before);
}
int main(void)
{
 struct nk_context ctx;struct nk_user_font font;WenaBoard board;WenaList lists[2];WenaSwimlane lane;
 WenaCard cards[3];WenaBoardLayout layout;WenaCardMoveState state;
 memset(&font,0,sizeof(font));font.height=13;font.width=width;assert(nk_init_default(&ctx,&font));
 assert(wena_board_init(&board,"b","Board",0));assert(wena_list_init(&lists[0],"src-list","b","","Source",0,0));
 assert(wena_list_init(&lists[1],"dst","b","","Destination",1,0));assert(wena_swimlane_init(&lane,"lane","b","Lane",0,0));
 assert(wena_card_init(&cards[0],"src","b","lane","src-list","Same",0,0));
 assert(wena_card_init(&cards[1],"old","b","lane","dst","Same",1,1));assert(wena_card_init(&cards[2],"other","b","lane","dst","Same",3,0));
 memset(&layout,0,sizeof(layout));layout.board=&board;layout.lists=lists;layout.list_count=2;layout.swimlanes=&lane;layout.swimlane_count=1;layout.cards=cards;layout.card_count=3;
 wena_card_move_init(&state,load,append,NULL);wena_card_move_set_insert_adapter(&state,insert);
 choose(&ctx,&state,&layout,2);click(&ctx,&state,&layout,"Cancel");assert(!state.visible&&!state.order&&!state.destination_order&&!writes);
 choose(&ctx,&state,&layout,2);expected_position=0;click(&ctx,&state,&layout,"Save");assert(!state.visible&&writes==1&&!state.destination_order);
 choose(&ctx,&state,&layout,2);increment(&ctx,&state,&layout);assert(state.insert_position==1);increment(&ctx,&state,&layout);assert(state.insert_position==2);
 increment(&ctx,&state,&layout);assert(state.insert_position==2);expected_position=2;fail=1;
 click(&ctx,&state,&layout,"Save");assert(state.visible&&state.error&&writes==2&&state.insert_position==2);
 fail=0;click(&ctx,&state,&layout,"Save");assert(!state.visible&&writes==3);
 choose(&ctx,&state,&layout,2);cards[1].sort=2;click(&ctx,&state,&layout,"Save");assert(state.visible&&state.error&&writes==3);
 click(&ctx,&state,&layout,"Save");assert(writes==3);cards[1].sort=1;click(&ctx,&state,&layout,"Cancel");
 choose(&ctx,&state,&layout,2);lists[1].archived=1;click(&ctx,&state,&layout,"Save");assert(state.visible&&state.error&&writes==3);
 lists[1].archived=0;click(&ctx,&state,&layout,"Cancel");
 choose(&ctx,&state,&layout,2);cards[0].sort=1;click(&ctx,&state,&layout,"Save");assert(state.visible&&state.error&&writes==3);
 cards[0].sort=0;click(&ctx,&state,&layout,"Cancel");
 layout.card_count=1;choose(&ctx,&state,&layout,0);increment(&ctx,&state,&layout);assert(state.insert_position==0);
 expected_position=0;click(&ctx,&state,&layout,"Save");assert(!state.visible&&writes==4);layout.card_count=3;
 choose(&ctx,&state,&layout,2);nk_clear(&ctx);nk_input_begin(&ctx);nk_input_key(&ctx,NK_KEY_TEXT_RESET_MODE,1);nk_input_end(&ctx);render(&ctx,&state,&layout);
 assert(!state.visible&&!state.order&&!state.destination_order&&writes==4);
 wena_card_move_close(&state);nk_free(&ctx);puts("Real Move form insertion: position bounds, stale targets, retry, cancel and Escape passed");return 0;
}
