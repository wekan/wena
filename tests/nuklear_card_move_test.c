#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_move.h"
#include "../client/components/lists/list_header.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int writes;
static int load(void *context,const char *board,const char *card,char *title,
    size_t capacity,unsigned long *version)
{
    (void)context; (void)board; (void)card; (void)capacity;
    strcpy(title,"Card"); *version=1; return 1;
}
static int move(void *context,const char *board,const char *card,
    unsigned long version,const char *list,const char *lane)
{
    (void)context;
    assert(!strcmp(board,"board") && !strcmp(card,"card") && version==1);
    assert(!strcmp(list,"second-list") && !strcmp(lane,"second-lane"));
    ++writes; return 1;
}

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx, WenaCardMoveState *state,
                   WenaBoardLayout *layout)
{
    const struct nk_command *command;
    if (state->visible)
        assert(wena_card_move_render(ctx, state, layout, 640, 480));
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

static void click_at(struct nk_context *ctx, WenaCardMoveState *state,
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

static void click(struct nk_context *ctx, WenaCardMoveState *state,
                  WenaBoardLayout *layout, const char *label)
{
    click_at(ctx, state, layout, label_center(ctx, label));
}

int main(void)
{
    struct nk_context ctx; struct nk_user_font font;
    WenaCardMoveState state; WenaBoardLayout layout;
    WenaBoard board; WenaList lists[2]; WenaSwimlane lanes[2]; WenaCard card;
    memset(&font,0,sizeof(font)); font.height=13; font.width=text_width;
    assert(nk_init_default(&ctx,&font));
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"first-list","board","","First list",0,0));
    assert(wena_list_init(&lists[1],"second-list","board","","Second list",1,0));
    assert(wena_swimlane_init(&lanes[0],"first-lane","board","First lane",0,0));
    assert(wena_swimlane_init(&lanes[1],"second-lane","board","Second lane",1,0));
    assert(wena_card_init(&card,"card","board","first-lane","first-list","Card",0,0));
    memset(&layout,0,sizeof(layout)); layout.board=&board;
    layout.lists=lists; layout.list_count=2; layout.swimlanes=lanes; layout.swimlane_count=2;
    layout.cards=&card; layout.card_count=1;
    wena_card_move_init(&state,load,move,NULL);
    assert(wena_card_move_open(&state,&layout,"card"));
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"First lane [first-lane]");
    click(&ctx,&state,&layout,"Second lane [second-lane]");
    assert(!strcmp(state.target_swimlane_id,"second-lane"));
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"First list [first-list]");
    click(&ctx,&state,&layout,"Second list [second-list]");
    assert(!strcmp(state.target_list_id,"second-list"));
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Save");
    assert(!state.visible && writes==1);
    assert(wena_card_move_open(&state,&layout,"card"));
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Cancel");
    assert(!state.visible && writes==1);
    nk_free(&ctx); puts("Real Nuklear move selectors, save and cancel passed");
    return 0;
}
