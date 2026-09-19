#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/checklists.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static WenaChecklistSnapshot store;
static int writes;
static int load(void*c,const char*b,const char*id,WenaChecklistSnapshot*out)
{(void)c;(void)b;(void)id;*out=store;return 1;}
static int save(void*c,const char*b,const char*id,const WenaChecklistEdit*e)
{(void)c;(void)b;(void)id;assert(e->expected_card_version==store.card_version);++writes;++store.card_version;if(e->action==WENA_CHECKLIST_CREATE){assert(wena_checklist_init(&store.checklists[0],"cl","b","c",e->title,0));store.checklist_count=1;store.checklist_versions[0]=1;}if(e->action==WENA_CHECKLIST_SET_FLAGS){store.checklists[0].hide_all_items=e->hide_all_items;store.checklists[0].hide_checked_items=e->hide_checked_items;store.checklists[0].show_on_minicard=e->show_on_minicard;}if(e->action==WENA_CHECKLIST_DELETE_ITEM)store.item_count=0;if(e->action==WENA_CHECKLIST_DELETE){store.checklist_count=0;store.item_count=0;}return 1;}
static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card)
{
    const struct nk_command *command;
    if(state->visible) assert(wena_checklists_render(ctx,state,card,1,640,480));
    nk_foreach(command,ctx) { (void)command; }
}
static void character(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,nk_rune rune)
{ nk_clear(ctx);nk_input_begin(ctx);nk_input_unicode(ctx,rune);nk_input_end(ctx);render(ctx,state,card); }
static void key(struct nk_context *ctx,WenaChecklistsState *state,WenaCard *card,enum nk_keys key,int down)
{ nk_clear(ctx);nk_input_begin(ctx);nk_input_key(ctx,key,down);nk_input_end(ctx);render(ctx,state,card); }
/* Locate rendered text, avoiding assumptions about font-specific button widths. */
static struct nk_vec2 label_center(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, ctx) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                           (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing rendered control: %s\n", label);
    assert(0);
    return nk_vec2(0.0f, 0.0f);
}

static void click_at(struct nk_context *ctx, WenaChecklistsState *state,
                     WenaCard *card, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(ctx);
        nk_input_begin(ctx);
        nk_input_motion(ctx, (int)point.x, (int)point.y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(ctx);
        render(ctx, state, card);
    }
}

static void click(struct nk_context *ctx, WenaChecklistsState *state,
                  WenaCard *card, const char *label)
{
    click_at(ctx, state, card, label_center(ctx, label));
}

static int has_label(struct nk_context *ctx,const char *label)
{
 const struct nk_command *command;const struct nk_command_text *text;
 nk_foreach(command,ctx)if(command->type==NK_COMMAND_TEXT){text=(const struct nk_command_text*)command;if((size_t)text->length==strlen(label)&&!memcmp(text->string,label,(size_t)text->length))return 1;}
 return 0;
}
int main(void)
{
 struct nk_context ctx;struct nk_user_font font;WenaChecklistsState state;WenaCard card;int i;
 memset(&store,0,sizeof(store));strcpy(store.board_id,"b");strcpy(store.card_id,"c");store.card_version=1;
 memset(&font,0,sizeof(font));font.height=13;font.width=text_width;assert(nk_init_default(&ctx,&font));
 assert(wena_card_init(&card,"c","b","s","l","Card",0,0));wena_checklists_init(&state,load,save,NULL);assert(wena_checklists_open(&state,&card));render(&ctx,&state,&card);
 click(&ctx,&state,&card,"Add Checklist");assert(state.action==WENA_CHECKLIST_CREATE);
 character(&ctx,&state,&card,0);click_at(&ctx,&state,&card,nk_vec2(350,55));character(&ctx,&state,&card,'A');
 assert(state.length==1);key(&ctx,&state,&card,NK_KEY_ENTER,1);assert(writes==1&&!state.action&&!strcmp(state.snapshot->checklists[0].title,"A"));
 key(&ctx,&state,&card,NK_KEY_ENTER,0);character(&ctx,&state,&card,0);assert(label_center(&ctx,"A").x>0);assert(label_center(&ctx,"0 / 0 (0%)").x>0);
 click(&ctx,&state,&card,"Checklist Actions");character(&ctx,&state,&card,0);assert(state.action==WENA_CHECKLIST_SET_FLAGS);
 click(&ctx,&state,&card,"Hide all checklist items");assert(state.hide_all_items&&!store.checklists[0].hide_all_items);
 click(&ctx,&state,&card,"Hide checked checklist items");assert(state.hide_checked_items);
 assert(!has_label(&ctx,"Default")&&!has_label(&ctx,"Yes")&&!has_label(&ctx,"No")&&!has_label(&ctx,"Show on minicard"));
 click(&ctx,&state,&card,"Save");assert(writes==2&&!state.action&&state.snapshot->checklists[0].hide_all_items&&state.snapshot->checklists[0].show_on_minicard==WENA_CHECKLIST_MINICARD_INHERIT);
 character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Checklist Actions");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Hide all checklist items");click(&ctx,&state,&card,"Cancel");assert(state.snapshot->checklists[0].hide_all_items&&writes==2);character(&ctx,&state,&card,0);
 click(&ctx,&state,&card,"Rename");character(&ctx,&state,&card,0);click_at(&ctx,&state,&card,nk_vec2(350,55));
 for(i=0;i<140;++i)character(&ctx,&state,&card,'x');
 assert(state.length==132);click(&ctx,&state,&card,"Save");assert(state.error&&writes==2);
 click(&ctx,&state,&card,"Cancel");assert(!state.action);key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible&&!state.snapshot&&writes==2);
 key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,0);wena_checklists_init(&state,load,NULL,NULL);assert(wena_checklists_open(&state,&card));character(&ctx,&state,&card,0);assert(label_center(&ctx,"A").x>0);key(&ctx,&state,&card,NK_KEY_ENTER,1);assert(state.visible&&writes==2);key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible&&writes==2);
 key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,0);
 store.item_count=1;store.item_versions[0]=1;assert(wena_checklist_item_init(&store.items[0],"it","b","c","cl","Hidden item",0,1));store.checklists[0].hide_checked_items=1;
 assert(wena_checklists_open(&state,&card));character(&ctx,&state,&card,0);assert(has_label(&ctx,"1 / 1 (100%)")&&!has_label(&ctx,"Hidden item"));wena_checklists_close(&state);
 store.items[0].is_finished=0;store.checklists[0].hide_checked_items=0;store.checklists[0].hide_all_items=1;
 assert(wena_checklists_open(&state,&card));character(&ctx,&state,&card,0);assert(has_label(&ctx,"0 / 1 (0%)")&&!has_label(&ctx,"Hidden item"));wena_checklists_close(&state);
 wena_checklists_init(&state,load,save,NULL);assert(wena_checklists_open(&state,&card));character(&ctx,&state,&card,0);
 click(&ctx,&state,&card,"Rename");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Delete");character(&ctx,&state,&card,0);assert(state.action==WENA_CHECKLIST_DELETE&&has_label(&ctx,"Hidden item"));
 key(&ctx,&state,&card,NK_KEY_ENTER,1);assert(state.action==WENA_CHECKLIST_DELETE&&writes==2);key(&ctx,&state,&card,NK_KEY_ENTER,0);click(&ctx,&state,&card,"Cancel");assert(!state.action&&store.item_count==1&&writes==2);wena_checklists_close(&state);
 store.checklists[0].hide_all_items=0;assert(wena_checklists_open(&state,&card));character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Edit");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Delete");character(&ctx,&state,&card,0);assert(state.action==WENA_CHECKLIST_DELETE_ITEM);
 key(&ctx,&state,&card,NK_KEY_ENTER,1);assert(writes==2&&state.action==WENA_CHECKLIST_DELETE_ITEM);key(&ctx,&state,&card,NK_KEY_ENTER,0);key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible&&writes==2);key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,0);
 assert(wena_checklists_open(&state,&card));character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Edit");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Delete");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Delete");assert(!state.action&&!state.snapshot->item_count&&writes==3);
 character(&ctx,&state,&card,0);assert(has_label(&ctx,"0 / 0 (0%)"));click(&ctx,&state,&card,"Rename");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Delete");character(&ctx,&state,&card,0);click(&ctx,&state,&card,"Delete");assert(!state.action&&!state.snapshot->checklist_count&&writes==4);wena_checklists_close(&state);
 nk_free(&ctx);puts("Real checklist creation, progress, bounded editing, readonly and Escape passed");return 0;
}
