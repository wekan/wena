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

static int reorder(void *context,const char *board,const char *card,
    unsigned long version,unsigned long target)
{
    (void)context; assert(!strcmp(board,"board")&&!strcmp(card,"card")&&version==1&&target==0);
    ++writes; return 1;
}
int main(void)
{
    struct nk_context ctx;struct nk_user_font font;WenaCardMoveState state;
    WenaBoardLayout layout;WenaBoard board;WenaList list;WenaSwimlane lane;WenaCard cards[3];
    memset(&font,0,sizeof(font));font.height=13;font.width=text_width;assert(nk_init_default(&ctx,&font));
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&list,"list","board","","List",0,0));
    assert(wena_swimlane_init(&lane,"lane","board","Lane",0,0));
    assert(wena_card_init(&cards[0],"old","board","lane","list","Old",1,1));
    assert(wena_card_init(&cards[1],"card","board","lane","list","Card",3,0));
    assert(wena_card_init(&cards[2],"last","board","lane","list","Last",8,0));
    memset(&layout,0,sizeof(layout));layout.board=&board;layout.lists=&list;layout.list_count=1;
    layout.swimlanes=&lane;layout.swimlane_count=1;layout.cards=cards;layout.card_count=3;
    wena_card_move_init(&state,load,move,NULL);wena_card_move_set_reorder_adapter(&state,reorder);
    assert(wena_card_move_open(&state,&layout,"card"));render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Move to Bottom");
    click(&ctx,&state,&layout,"1. Old (Archived) [old]");assert(state.reorder_choice==1);
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Save");assert(!state.visible&&state.order==NULL&&writes==1);
    assert(wena_card_move_open(&state,&layout,"card"));
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Move to Bottom");
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_key(&ctx,NK_KEY_TEXT_RESET_MODE,1);nk_input_end(&ctx);
    render(&ctx,&state,&layout);assert(!state.visible&&state.order==NULL&&writes==1);
    wena_card_move_close(&state);nk_free(&ctx);
    puts("Real Nuklear indexed card order, archived slot and popup cancel passed");return 0;
}
