#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/board_filter.h"
#include "../client/features/board.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static float text_width(nk_handle handle,float height,const char *text,int length)
{ (void)handle;(void)text;return height*(float)length*0.5f; }
static int text_seen(struct nk_context *ctx,const char *value)
{
    const struct nk_command *command;
    nk_foreach(command,ctx) if(command->type==NK_COMMAND_TEXT) {
        const struct nk_command_text *text;
        text=(const struct nk_command_text*)command;
        if ((size_t)text->length==strlen(value)&&!memcmp(text->string,value,strlen(value)))return 1;
    }
    return 0;
}
static void draft(WenaBoardFilterState *state,const char *value)
{ strcpy(state->input,value);state->length=(int)strlen(value); }
static int filter_frame(struct nk_context *ctx,WenaBoardFilterState *state,int key,int down,int click)
{
    const struct nk_command *command;
    int changed;
    nk_clear(ctx);nk_input_begin(ctx);
    if(key>=0)nk_input_key(ctx,(enum nk_keys)key,down);
    if(click>=0){nk_input_motion(ctx,40,55);nk_input_button(ctx,NK_BUTTON_LEFT,40,55,click);}
    nk_input_end(ctx);changed=0;
    if(nk_begin(ctx,"Filter test",nk_rect(0,0,640,180),NK_WINDOW_BORDER))changed=wena_board_filter_render(ctx,state);
    nk_end(ctx);nk_foreach(command,ctx){(void)command;}return changed;
}
int main(void)
{
    WenaBoardFilterState filter;
    WenaBoard board;
    WenaSwimlane lane;
    WenaList list;
    WenaCard cards[4], original[4];
    WenaBoardLayout layout;
    WenaCardDetailsState details;
    struct nk_context ctx;
    struct nk_user_font font;
    int i;
    wena_board_filter_init(&filter);
    assert(!wena_board_filter_sync(&filter,"bad/id"));
    assert(wena_board_filter_sync(&filter,"board"));
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_swimlane_init(&lane,"lane","board","Lane",0,0));
    assert(wena_list_init(&list,"list","board","","List",0,0));
    assert(wena_card_init(&cards[0],"one","board","lane","list","Alpha",0,0));
    assert(wena_card_init(&cards[1],"two","board","lane","list","Beta",1,0));
    assert(wena_card_init(&cards[2],"three","board","lane","list","Archived",2,1));
    assert(wena_card_init(&cards[3],"four","other","lane","list","Other",0,0));
    memcpy(original,cards,sizeof(cards));
    assert(wena_board_filter_matches(&filter,&cards[0]));
    assert(!wena_board_filter_matches(&filter,&cards[2]));
    assert(!wena_board_filter_matches(&filter,&cards[3]));
    draft(&filter,"ALp");assert(wena_board_filter_apply(&filter));
    assert(wena_board_filter_matches(&filter,&cards[0]));assert(!wena_board_filter_matches(&filter,&cards[1]));
    draft(&filter,".*");assert(wena_board_filter_apply(&filter));assert(!wena_board_filter_matches(&filter,&cards[0]));
    strcpy(cards[0].title,"\303\204iti");draft(&filter,"\303\204");assert(wena_board_filter_apply(&filter));assert(wena_board_filter_matches(&filter,&cards[0]));
    draft(&filter,"\303\244");assert(wena_board_filter_apply(&filter));assert(!wena_board_filter_matches(&filter,&cards[0]));
    memcpy(cards,original,sizeof(cards));draft(&filter,"alp");assert(wena_board_filter_apply(&filter));
    draft(&filter,"bad\n");assert(!wena_board_filter_apply(&filter));assert(!strcmp(filter.query,"alp"));
    draft(&filter,"\300\257");assert(!wena_board_filter_apply(&filter));
    draft(&filter,"\302\200");assert(!wena_board_filter_apply(&filter));
    memset(filter.input,'x',sizeof(filter.input));filter.length=129;assert(!wena_board_filter_apply(&filter));
    wena_board_filter_cancel(&filter);assert(!strcmp(filter.input,"alp")&&!filter.error);
    memset(filter.input,'x',128);filter.length=128;assert(wena_board_filter_apply(&filter));
    assert(strlen(filter.query)==128);wena_board_filter_clear(&filter);
    memset(&font,0,sizeof(font));font.height=14;font.width=text_width;assert(nk_init_default(&ctx,&font));
    memset(&layout,0,sizeof(layout));layout.board=&board;layout.swimlanes=&lane;layout.swimlane_count=1;
    layout.lists=&list;layout.list_count=1;layout.cards=cards;layout.card_count=4;
    layout.card_visible=wena_board_filter_matches;layout.card_visible_context=&filter;
    wena_card_details_init(&details);assert(wena_card_details_open(&details,&cards[1]));
    draft(&filter,"alp");assert(wena_board_filter_apply(&filter));
    assert(wena_board_feature_render_with_state(&ctx,&layout,640,480,&details));
    assert(!details.visible&&text_seen(&ctx,"Alpha")&&!text_seen(&ctx,"Beta"));
    assert(!memcmp(cards,original,sizeof(cards)));
    nk_clear(&ctx);draft(&filter,"none");assert(wena_board_filter_apply(&filter));
    assert(wena_board_feature_render(&ctx,&layout,640,480));assert(text_seen(&ctx,"No Cards Found"));
    /* Parent controls remain usable while leaf titles are filtered. */
    assert(text_seen(&ctx,"Add card")&&text_seen(&ctx,"List"));
    /* New/renamed/moved records use the live snapshot, never cached indices. */
    strcpy(cards[1].title,"none");nk_clear(&ctx);assert(wena_board_feature_render(&ctx,&layout,640,480));assert(text_seen(&ctx,"none"));
    strcpy(cards[1].list_id,"elsewhere");nk_clear(&ctx);assert(wena_board_feature_render(&ctx,&layout,640,480));assert(!text_seen(&ctx,"none"));
    memcpy(cards,original,sizeof(cards));wena_board_filter_clear(&filter);
    assert(wena_board_filter_sync(&filter,"other"));assert(!filter.query[0]);assert(wena_board_filter_sync(&filter,"board"));
    nk_free(&ctx);assert(nk_init_default(&ctx,&font));
    filter_frame(&ctx,&filter,-1,0,-1);filter_frame(&ctx,&filter,-1,0,1);filter_frame(&ctx,&filter,-1,0,0);
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_unicode(&ctx,'A');nk_input_end(&ctx);
    assert(nk_begin(&ctx,"Filter test",nk_rect(0,0,640,180),NK_WINDOW_BORDER));assert(!wena_board_filter_render(&ctx,&filter));nk_end(&ctx);
    {const struct nk_command *command;nk_foreach(command,&ctx){(void)command;}}
    assert(filter.length==1&&filter.input[0]=='A');
    assert(filter_frame(&ctx,&filter,NK_KEY_ENTER,1,-1));assert(!strcmp(filter.query,"A"));
    assert(!filter_frame(&ctx,&filter,-1,0,-1));filter_frame(&ctx,&filter,NK_KEY_ENTER,0,-1);
    draft(&filter,"unsaved");filter_frame(&ctx,&filter,NK_KEY_TEXT_RESET_MODE,1,-1);assert(!strcmp(filter.input,"A"));
    filter_frame(&ctx,&filter,NK_KEY_TEXT_RESET_MODE,0,-1);
    /* Exact sentinel capacity rejects overflow without altering active query. */
    for(i=0;i<129;++i) filter.input[i]='x';
    filter.length=129;
    filter_frame(&ctx,&filter,NK_KEY_ENTER,1,-1);assert(!strcmp(filter.query,"A")&&filter.error);
    nk_free(&ctx);puts("board filter model, scope, rendering and real key tests passed");return 0;
}
