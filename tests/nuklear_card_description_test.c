#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_description.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char stored[WENA_DESCRIPTION_CAPACITY];
static int writes;
static int other_window;
static int load(void *context,const char *board,const char *card,char *text,size_t capacity,unsigned long *version)
{ (void)context;(void)board;(void)card;assert(strlen(stored)<capacity);strcpy(text,stored);*version=1;return 1; }
static int save(void *context,const char *board,const char *card,unsigned long version,const char *text)
{ (void)context;(void)board;(void)card;assert(version==1);strcpy(stored,text);++writes;return 1; }
static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx,WenaCardDescriptionState *state,WenaCard *card)
{
    const struct nk_command *command;
    if(other_window) { nk_begin(ctx,"Other window",nk_rect(0,0,180,150),NK_WINDOW_BORDER);nk_end(ctx); }
    if(state->visible) assert(wena_card_description_render(ctx,state,card,1,640,480));
    nk_foreach(command,ctx) { (void)command; }
}
static void character(struct nk_context *ctx,WenaCardDescriptionState *state,WenaCard *card,nk_rune rune)
{ nk_clear(ctx);nk_input_begin(ctx);nk_input_unicode(ctx,rune);nk_input_end(ctx);render(ctx,state,card); }
static void key(struct nk_context *ctx,WenaCardDescriptionState *state,WenaCard *card,enum nk_keys key,int down)
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

static void click_at(struct nk_context *ctx, WenaCardDescriptionState *state,
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

static void click(struct nk_context *ctx, WenaCardDescriptionState *state,
                  WenaCard *card, const char *label)
{
    click_at(ctx, state, card, label_center(ctx, label));
}

int main(void)
{
    struct nk_context ctx;struct nk_user_font font;WenaCardDescriptionState state;WenaCard card;int i;
    memset(&font,0,sizeof(font));font.height=13;font.width=text_width;assert(nk_init_default(&ctx,&font));
    assert(wena_card_init(&card,"card","board","lane","list","Card",0,0));
    wena_card_description_init(&state,load,save,NULL);assert(wena_card_description_open(&state,&card));
    render(&ctx,&state,&card);click_at(&ctx,&state,&card,nk_vec2(350,70));
    character(&ctx,&state,&card,'A');key(&ctx,&state,&card,NK_KEY_ENTER,1);
    assert(state.visible&&state.length==2&&state.input[1]=='\n'&&writes==0);
    key(&ctx,&state,&card,NK_KEY_ENTER,0);character(&ctx,&state,&card,'B');
    assert(state.length==3&&!memcmp(state.input,"A\nB",3));
    click(&ctx,&state,&card,"Save");assert(!state.visible&&writes==1&&!strcmp(stored,"A\nB"));
    wena_card_description_init(&state,load,NULL,NULL);assert(wena_card_description_open(&state,&card));
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&card);
    click_at(&ctx,&state,&card,nk_vec2(350,70));character(&ctx,&state,&card,'X');
    key(&ctx,&state,&card,NK_KEY_ENTER,1);key(&ctx,&state,&card,NK_KEY_ENTER,0);
    assert(state.length==3&&!memcmp(state.input,"A\nB",3)&&writes==1);
    key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible&&writes==1);
    assert(wena_card_description_open(&state,&card));nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&card);
    assert(state.visible);key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,0);
    click(&ctx,&state,&card,"Cancel");assert(!state.visible&&writes==1);
    stored[0]=0;wena_card_description_init(&state,load,save,NULL);assert(wena_card_description_open(&state,&card));
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&card);
    click_at(&ctx,&state,&card,nk_vec2(350,70));
    for(i=0;i<1100;++i) character(&ctx,&state,&card,'x');
    assert(state.length==WENA_DESCRIPTION_CAPACITY);click(&ctx,&state,&card,"Save");
    assert(state.visible&&state.error&&writes==1);
    key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible&&writes==1);
    key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,0);
    assert(wena_card_description_open(&state,&card));
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&card);
    other_window=1;nk_clear(&ctx);render(&ctx,&state,&card);
    nk_window_set_focus(&ctx,"Other window");
    nk_input_motion(&ctx,10,10);
    key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(state.visible&&writes==1);
    key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,0);
    nk_window_set_focus(&ctx,"Card description");
    nk_input_motion(&ctx,350,70);
    key(&ctx,&state,&card,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible&&writes==1);
    nk_free(&ctx);puts("Real multiline description Enter, readonly, bounds and Escape passed");return 0;
}
