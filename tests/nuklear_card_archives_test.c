#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_archives.h"
#include "../client/components/lists/list_header.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int writes;
static WenaCard cards[6];
static int reads;
static int load(void *context,const char *board,const char *card,char *title,
    size_t capacity,unsigned long *version)
{
    (void)context; (void)board; (void)card; (void)capacity;
    ++reads; strcpy(title,"Archived"); *version=1; return 1;
}
static int restore(void *context,const char *board,const char *card,unsigned long version)
{
    (void)context;
    assert(!strcmp(board,"board") && !strcmp(card,"two") && version==1);
    cards[5].archived=0; ++writes; return 1;
}

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx, WenaCardArchivesState *state,
                   WenaBoardLayout *layout)
{
    const struct nk_command *command;
    if (state->visible)
        assert(wena_card_archives_render(ctx, state, layout, 640, 480));
    assert(ctx->current == NULL);
    nk_foreach(command, ctx) { (void)command; }
}

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

static void click_at(struct nk_context *ctx, WenaCardArchivesState *state,
                     WenaBoardLayout *layout, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(ctx);
        nk_input_begin(ctx);
        nk_input_motion(ctx, (int)point.x, (int)point.y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(ctx);
        render(ctx, state, layout);
    }
}

static void click(struct nk_context *ctx, WenaCardArchivesState *state,
                  WenaBoardLayout *layout, const char *label)
{
    click_at(ctx, state, layout, label_center(ctx, label));
}

int main(void)
{
    struct nk_context ctx; struct nk_user_font font;
    int i;
    const char *ids[]={"one","p2","p3","p4","p5","two"};
    WenaCardArchivesState state; WenaBoardLayout layout; WenaBoard board;
    memset(&font,0,sizeof(font)); font.height=13; font.width=text_width;
    assert(nk_init_default(&ctx,&font));
    assert(wena_board_init(&board,"board","Board",0));
    for(i=0;i<6;++i)assert(wena_card_init(&cards[i],ids[i],"board","lane","list","Repeated",i,1));
    memset(&layout,0,sizeof(layout)); layout.board=&board; layout.cards=cards; layout.card_count=6;
    wena_card_archives_init(&state,load,restore,NULL);
    assert(wena_card_archives_open(&state,&layout));
    render(&ctx,&state,&layout);
    assert(state.table.page==0 && reads==1);
    click(&ctx,&state,&layout,"Next Page");
    assert(state.table.page==1 && reads==1 && writes==0);
    click(&ctx,&state,&layout,"Repeated [two]");
    assert(!strcmp(state.card_id,"two") && reads==2);
    click(&ctx,&state,&layout,"Previous Page");
    assert(state.table.page==0 && !strcmp(state.card_id,"two") && reads==2);
    /* The exact restore target remains visible when its row is on another page. */
    (void)label_center(&ctx,"Repeated [two]");
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Restore");
    assert(state.visible && writes==1 && !cards[5].archived);
    assert(!strcmp(state.card_id,"one"));
    click(&ctx,&state,&layout,"Next Page");
    assert(state.table.page==1);
    for(i=1;i<5;++i)cards[i].archived=0;
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);render(&ctx,&state,&layout);
    assert(state.table.page==0 && !strcmp(state.card_id,"one"));
    click(&ctx,&state,&layout,"Cancel");
    assert(!state.visible && writes==1);
    nk_free(&ctx); puts("Real Nuklear paginated archives, exact selection, restore and cancel passed");
    return 0;
}
