#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_move.h"
#include "../client/features/hierarchy_move.h"
#include "../client/features/card_archives.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Panel {
    int kind;
    WenaCardMoveState card_move;
    WenaHierarchyMoveState hierarchy_move;
    WenaCardArchivesState archives;
} Panel;
static int writes;
static float text_width(nk_handle h,float height,const char *text,int length)
{ (void)h; (void)text; return height*(float)length*0.5f; }
static int load_card(void *data,const char *board,const char *card,char *title,
    size_t capacity,unsigned long *version)
{ (void)data; (void)board; (void)card; (void)capacity; strcpy(title,"Card"); *version=1; return 1; }
static int move_card(void *data,const char *board,const char *card,unsigned long version,
    const char *list,const char *lane)
{ (void)data; (void)board; (void)card; (void)version; (void)list; (void)lane; ++writes; return 1; }
static int load_hierarchy(void *data,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long *version,unsigned long *position)
{ (void)data; (void)board; (void)kind; (void)id; *version=1; *position=0; return 1; }
static int move_hierarchy(void *data,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long version,unsigned long position)
{ (void)data; (void)board; (void)kind; (void)id; (void)version; (void)position; ++writes; return 1; }
static int restore(void *data,const char *board,const char *id,unsigned long version)
{ (void)data; (void)board; (void)id; (void)version; ++writes; return 1; }
static int visible(Panel *p)
{ return p->kind==0?p->card_move.visible:p->kind==1?p->hierarchy_move.visible:p->archives.visible; }
static const char *window_id(Panel *p)
{ return p->kind==0?"Move card":p->kind==1?"Move hierarchy":"Archives"; }
static void open_panel(Panel *p,WenaBoardLayout *layout)
{
    if(p->kind==0) {
        wena_card_move_init(&p->card_move,load_card,move_card,NULL);
        assert(wena_card_move_open(&p->card_move,layout,"active"));
    } else if(p->kind==1) {
        wena_hierarchy_move_init(&p->hierarchy_move,load_hierarchy,move_hierarchy,NULL);
        assert(wena_hierarchy_move_open(&p->hierarchy_move,layout,WENA_HIERARCHY_LIST,"first-list"));
    } else {
        wena_card_archives_init(&p->archives,load_card,restore,NULL);
        assert(wena_card_archives_open(&p->archives,layout));
    }
}
static void draw(struct nk_context *ctx,Panel *p,WenaBoardLayout *layout,int other)
{
    const struct nk_command *command;
    if(visible(p)) {
        if(p->kind==0) (void)wena_card_move_render(ctx,&p->card_move,layout,640,480);
        else if(p->kind==1) (void)wena_hierarchy_move_render(ctx,&p->hierarchy_move,layout,640,480);
        else (void)wena_card_archives_render(ctx,&p->archives,layout,640,480);
    }
    if(other) {
        if(nk_begin(ctx,"Other",nk_rect(0,0,100,70),NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx,24,1); nk_label(ctx,"Other",NK_TEXT_LEFT);
        }
        nk_end(ctx);
    }
    nk_foreach(command,ctx) { (void)command; }
}
static void key_frame(struct nk_context *ctx,Panel *p,WenaBoardLayout *layout,
    int key,int down,int other)
{
    nk_clear(ctx); nk_input_begin(ctx);
    if(key>=0) nk_input_key(ctx,(enum nk_keys)key,down);
    nk_input_end(ctx); draw(ctx,p,layout,other);
}
static struct nk_vec2 label_center(struct nk_context *ctx,const char *label)
{
    const struct nk_command *command; const struct nk_command_text *text;
    nk_foreach(command,ctx) {
        if(command->type!=NK_COMMAND_TEXT) continue;
        text=(const struct nk_command_text*)command;
        if((size_t)text->length==strlen(label) && !memcmp(text->string,label,(size_t)text->length))
            return nk_vec2((float)text->x+(float)text->w*0.5f,(float)text->y+(float)text->h*0.5f);
    }
    assert(0); return nk_vec2(0,0);
}
static void click(struct nk_context *ctx,Panel *p,WenaBoardLayout *layout,const char *label)
{
    int down; struct nk_vec2 point; point=label_center(ctx,label);
    for(down=1;down>=0;--down) {
        nk_clear(ctx); nk_input_begin(ctx);
        nk_input_motion(ctx,(int)point.x,(int)point.y);
        nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);
        nk_input_end(ctx); draw(ctx,p,layout,0);
    }
}
static void test_panel(int kind)
{
    struct nk_context ctx; struct nk_user_font font; Panel p;
    WenaBoard board; WenaList lists[2]; WenaSwimlane lanes[2]; WenaCard cards[2]; WenaBoardLayout layout;
    struct nk_vec2 submit;
    memset(&font,0,sizeof(font)); font.height=13; font.width=text_width;
    assert(nk_init_default(&ctx,&font)); memset(&p,0,sizeof(p)); p.kind=kind;
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"first-list","board","","First list",0,0));
    assert(wena_list_init(&lists[1],"second-list","board","","Second list",1,0));
    assert(wena_swimlane_init(&lanes[0],"first-lane","board","First lane",0,0));
    assert(wena_swimlane_init(&lanes[1],"second-lane","board","Second lane",1,0));
    assert(wena_card_init(&cards[0],"active","board","first-lane","first-list","Active",0,0));
    assert(wena_card_init(&cards[1],"archived","board","first-lane","first-list","Archived",1,1));
    memset(&layout,0,sizeof(layout)); layout.board=&board; layout.lists=lists; layout.list_count=2;
    layout.swimlanes=lanes; layout.swimlane_count=2; layout.cards=cards; layout.card_count=2;
    open_panel(&p,&layout); draw(&ctx,&p,&layout,0);
    key_frame(&ctx,&p,&layout,NK_KEY_ENTER,1,0); assert(visible(&p) && writes==0);
    key_frame(&ctx,&p,&layout,NK_KEY_ENTER,0,0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,1,0);
    assert(!visible(&p) && writes==0);
    /* A held key without a new press cannot cancel a newly opened panel. */
    open_panel(&p,&layout); key_frame(&ctx,&p,&layout,-1,0,0);
    assert(visible(&p) && writes==0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,0,0);
    click(&ctx,&p,&layout,kind==0?"First lane [first-lane]":kind==1?"1. First list [first-list]":"Archived [archived]");
    assert(!!nk_window_find(&ctx,window_id(&p))->popup.active==(kind!=2));
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,1,0);
    assert(!visible(&p) && writes==0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,0,0);
    open_panel(&p,&layout); key_frame(&ctx,&p,&layout,-1,0,1);
    nk_window_set_focus(&ctx,"Other");
    nk_input_motion(&ctx,10,10);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,1,1);
    assert(visible(&p) && writes==0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,0,1);
    nk_window_set_focus(&ctx,window_id(&p));
    nk_input_motion(&ctx,450,180);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,1,1);
    assert(!visible(&p) && writes==0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,0,0);
    open_panel(&p,&layout); key_frame(&ctx,&p,&layout,-1,0,0);
    submit=label_center(&ctx,kind==2?"Restore":"Save");
    nk_clear(&ctx); nk_input_begin(&ctx);
    nk_input_motion(&ctx,(int)submit.x,(int)submit.y);
    nk_input_button(&ctx,NK_BUTTON_LEFT,(int)submit.x,(int)submit.y,1);
    nk_input_key(&ctx,NK_KEY_TEXT_RESET_MODE,1);
    nk_input_end(&ctx); draw(&ctx,&p,&layout,0);
    assert(!visible(&p) && writes==0);
    nk_clear(&ctx); nk_input_begin(&ctx);
    nk_input_button(&ctx,NK_BUTTON_LEFT,(int)submit.x,(int)submit.y,0);
    nk_input_key(&ctx,NK_KEY_TEXT_RESET_MODE,0);
    nk_input_end(&ctx); draw(&ctx,&p,&layout,0);
    open_panel(&p,&layout);
    if(kind==0) p.card_move.apply=NULL;
    else if(kind==1) p.hierarchy_move.apply=NULL;
    else p.archives.restore=NULL;
    key_frame(&ctx,&p,&layout,-1,0,0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,1,0);
    assert(!visible(&p) && writes==0);
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,0,0);
    open_panel(&p,&layout); strcpy(board.id,"stale");
    key_frame(&ctx,&p,&layout,NK_KEY_TEXT_RESET_MODE,1,0);
    assert(!visible(&p) && writes==0);
    nk_free(&ctx);
}
int main(void)
{
    test_panel(0); test_panel(1); test_panel(2);
    puts("Focused panel Escape, popup, held key, Enter and stale scope passed"); return 0;
}
