#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/hierarchy_move.h"
#include "../client/components/lists/list_header.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int writes;
static int load(void *context,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long *version,unsigned long *position)
{
    (void)context; (void)kind;
    assert(!strcmp(board,"board") && !strcmp(id,"one"));
    *version=1; *position=0; return 1;
}
static int move(void *context,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long version,unsigned long position)
{
    (void)context; (void)kind;
    assert(!strcmp(board,"board") && !strcmp(id,"one") && version==1 && position==1);
    ++writes; return 1;
}

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *ctx, WenaHierarchyMoveState *state,
                   WenaBoardLayout *layout)
{
    const struct nk_command *command;
    if (state->visible)
        assert(wena_hierarchy_move_render(ctx, state, layout, 640, 480));
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

static void click_at(struct nk_context *ctx, WenaHierarchyMoveState *state,
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

static void click(struct nk_context *ctx, WenaHierarchyMoveState *state,
                  WenaBoardLayout *layout, const char *label)
{
    click_at(ctx, state, layout, label_center(ctx, label));
}

static void test_kind(WenaHierarchyKind kind)
{
    struct nk_context ctx; struct nk_user_font font;
    WenaHierarchyMoveState state; WenaBoardLayout layout;
    WenaBoard board; WenaList lists[2]; WenaSwimlane lanes[2];
    int previous;
    previous=writes;
    memset(&font,0,sizeof(font)); font.height=13; font.width=text_width;
    assert(nk_init_default(&ctx,&font));
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"one","board","","Repeated",0,0));
    assert(wena_list_init(&lists[1],"two","board","","Repeated",1,1));
    assert(wena_swimlane_init(&lanes[0],"one","board","Repeated",0,0));
    assert(wena_swimlane_init(&lanes[1],"two","board","Repeated",1,1));
    memset(&layout,0,sizeof(layout)); layout.board=&board;
    layout.lists=lists; layout.list_count=2; layout.swimlanes=lanes; layout.swimlane_count=2;
    wena_hierarchy_move_init(&state,load,move,NULL);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"1. Repeated [one]");
    click(&ctx,&state,&layout,"2. Repeated [two]");
    assert(state.target_position==1);
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Save");
    assert(!state.visible && writes==previous+1);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Cancel");
    assert(!state.visible && writes==previous+1);
    nk_free(&ctx);
}
int main(void)
{
    test_kind(WENA_HIERARCHY_LIST); test_kind(WENA_HIERARCHY_SWIMLANE);
    puts("Real Nuklear hierarchy reorder selector, save and cancel passed");
    return 0;
}
