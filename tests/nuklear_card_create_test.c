#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_create.h"
#include "../client/components/lists/list_header.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int writes;
static int create(void *context, const char *board, const char *list,
                  const char *lane, const char *title)
{
    (void)context;
    assert(!strcmp(board,"board") && !strcmp(list,"list") && !strcmp(lane,"lane"));
    assert(!strcmp(title,"X"));
    ++writes;
    return 1;
}

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx, WenaCardCreateState *state,
                   WenaBoardLayout *layout)
{
    if (state->visible)
        assert(wena_card_create_render(ctx, state, layout, 640, 480));
    assert(ctx->current == NULL);
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

static void click_at(struct nk_context *ctx, WenaCardCreateState *state,
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

static void click(struct nk_context *ctx, WenaCardCreateState *state,
                  WenaBoardLayout *layout, const char *label)
{
    click_at(ctx, state, layout, label_center(ctx, label));
}

static void character(struct nk_context *ctx, WenaCardCreateState *state,
                      WenaBoardLayout *layout, nk_rune rune)
{
    nk_clear(ctx);
    nk_input_begin(ctx);
    nk_input_unicode(ctx, rune);
    nk_input_end(ctx);
    render(ctx, state, layout);
}

int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    WenaCardCreateState state;
    WenaBoardLayout layout;
    WenaListInteraction intent;
    WenaBoard board;
    WenaList list;
    WenaSwimlane lane;
    int index;
    memset(&font, 0, sizeof(font)); font.height=13; font.width=text_width;
    assert(nk_init_default(&ctx, &font));
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&list,"list","board","","List",0,0));
    assert(wena_swimlane_init(&lane,"lane","board","Lane",0,0));
    memset(&layout,0,sizeof(layout)); memset(&intent,0,sizeof(intent));
    layout.board=&board; layout.lists=&list; layout.list_count=1;
    layout.swimlanes=&lane; layout.swimlane_count=1;
    intent.actions=WENA_LIST_HEADER_ADD_CARD;
    strcpy(intent.board_id,"board"); strcpy(intent.list_id,"list");
    strcpy(intent.swimlane_id,"lane");
    wena_card_create_init(&state,create,NULL);
    assert(wena_card_create_open(&state,&layout,&intent));
    render(&ctx,&state,&layout);
    click_at(&ctx,&state,&layout,nk_vec2(420,55));
    character(&ctx,&state,&layout,(nk_rune)'X');
    assert(state.title_length==1);
    click(&ctx,&state,&layout,"Save");
    assert(!state.visible && writes==1);
    assert(wena_card_create_open(&state,&layout,&intent));
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click_at(&ctx,&state,&layout,nk_vec2(420,55));
    for(index=0;index<160;++index)
        character(&ctx,&state,&layout,(nk_rune)'a');
    assert(state.title_length ==
        WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY) - 1);
    click(&ctx,&state,&layout,"Save");
    assert(state.visible && state.error && writes==1);
    click(&ctx,&state,&layout,"Cancel");
    assert(!state.visible && writes==1);
    nk_free(&ctx);
    puts("Real Nuklear Add card input, save, bounds and cancel passed");
    return 0;
}
