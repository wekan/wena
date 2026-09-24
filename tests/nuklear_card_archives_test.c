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
static WenaList lists[8];
static int list_reads,list_writes,list_fail;
static WenaSwimlane lanes[8];
static int lane_reads,lane_writes,lane_fail;
static unsigned long lane_version=12;
static int lane_load(void *context,const char *board,const char *id,unsigned long *version)
{
    (void)context;(void)id;assert(!strcmp(board,"board"));++lane_reads;
    *version=lane_version;return !lane_fail;
}
static int lane_restore(void *context,const char *board,const char *id,unsigned long version)
{
    (void)context;assert(!strcmp(board,"board")&&!strcmp(id,"two"));
    if(lane_fail||version!=lane_version)return 0;
    lanes[5].archived=0;++lane_writes;return 1;
}
static int list_load(void *context,const char *board,const char *id,unsigned long *version)
{
    (void)context;(void)id;assert(!strcmp(board,"board"));++list_reads;
    *version=9;return !list_fail;
}
static int list_restore(void *context,const char *board,const char *id,unsigned long version)
{
    (void)context;assert(!strcmp(board,"board")&&!strcmp(id,"two")&&version==9);
    if(list_fail)return 0;
    lists[5].archived=0;++list_writes;return 1;
}

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
    for(i=0;i<6;++i)assert(wena_list_init(&lists[i],ids[i],"board","","Repeated",i,1));
    assert(wena_list_init(&lists[6],"active","board","","Active",6,0));
    assert(wena_list_init(&lists[7],"foreign","other","","Foreign",7,1));
    layout.lists=lists;layout.list_count=8;
    wena_card_archives_set_lists(&state,list_load,list_restore,NULL);
    assert(wena_card_archives_open(&state,&layout));
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Lists");
    assert(state.kind&&state.version==9&&list_reads==1&&!strcmp(state.card_id,"one"));
    click(&ctx,&state,&layout,"Next Page");
    assert(state.table.page==1&&list_reads==1&&writes==1&&!list_writes);
    click(&ctx,&state,&layout,"Repeated [two]");assert(list_reads==2&&!strcmp(state.card_id,"two"));
    click(&ctx,&state,&layout,"Previous Page");
    assert(!state.table.page&&list_reads==2);(void)label_center(&ctx,"Repeated [two]");
    list_fail=1;click(&ctx,&state,&layout,"Restore");
    assert(state.error&&lists[5].archived&&!list_writes&&writes==1);
    list_fail=0;click(&ctx,&state,&layout,"Restore");
    assert(!lists[5].archived&&list_writes==1&&writes==1&&!strcmp(state.card_id,"one"));
    click(&ctx,&state,&layout,"Cards");assert(!state.kind&&state.version==1&&!state.table.page);
    list_fail=1;click(&ctx,&state,&layout,"Lists");assert(state.kind&&state.error&&!state.version);
    click(&ctx,&state,&layout,"Restore");assert(list_writes==1&&writes==1);
    list_fail=0;click(&ctx,&state,&layout,"Cards");click(&ctx,&state,&layout,"Lists");
    click(&ctx,&state,&layout,"Next Page");assert(state.table.page==1);
    for(i=0;i<6;++i)lists[i].archived=0;
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
    assert(!state.table.page&&!state.card_id[0]&&!state.version);
    (void)label_center(&ctx,"No lists in Archive.");
    click(&ctx,&state,&layout,"Cancel");assert(!state.visible);
    for(i=0;i<6;++i)assert(wena_swimlane_init(&lanes[i],ids[i],"board","Repeated",i,1));
    assert(wena_swimlane_init(&lanes[6],"active","board","Active",6,0));
    assert(wena_swimlane_init(&lanes[7],"foreign","other","Foreign",7,1));
    layout.swimlanes=lanes;layout.swimlane_count=8;
    wena_card_archives_set_provider(&state,WENA_ARCHIVE_SWIMLANES,lane_load,lane_restore,NULL);
    assert(wena_card_archives_open(&state,&layout));
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
    click(&ctx,&state,&layout,"Swimlanes");
    assert(state.kind==WENA_ARCHIVE_SWIMLANES&&state.version==12&&lane_reads==1);
    click(&ctx,&state,&layout,"Next Page");assert(state.table.page==1&&lane_reads==1);
    click(&ctx,&state,&layout,"Repeated [two]");
    assert(!strcmp(state.card_id,"two")&&lane_reads==2);
    click(&ctx,&state,&layout,"Previous Page");assert(!state.table.page&&lane_reads==2);
    (void)label_center(&ctx,"Repeated [two]");
    ++lane_version;click(&ctx,&state,&layout,"Restore");
    assert(state.error&&lanes[5].archived&&!lane_writes);
    click(&ctx,&state,&layout,"Lists");assert(state.kind==WENA_ARCHIVE_LISTS&&!state.version);
    click(&ctx,&state,&layout,"Swimlanes");
    click(&ctx,&state,&layout,"Next Page");click(&ctx,&state,&layout,"Repeated [two]");
    lane_fail=1;click(&ctx,&state,&layout,"Restore");
    assert(state.error&&lanes[5].archived&&!lane_writes);
    lane_fail=0;click(&ctx,&state,&layout,"Restore");
    assert(!lanes[5].archived&&lane_writes==1&&list_writes==1&&writes==1);
    click(&ctx,&state,&layout,"Cards");assert(state.kind==WENA_ARCHIVE_CARDS&&!state.table.page);
    lane_fail=1;click(&ctx,&state,&layout,"Swimlanes");assert(state.error&&!state.version);
    click(&ctx,&state,&layout,"Restore");assert(lane_writes==1);
    for(i=0;i<6;++i)lanes[i].archived=0;
    nk_clear(&ctx);nk_input_begin(&ctx);nk_input_end(&ctx);render(&ctx,&state,&layout);
    assert(!state.card_id[0]&&!state.version&&!state.table.page);
    (void)label_center(&ctx,"No swimlanes in Archive.");
    click(&ctx,&state,&layout,"Cancel");assert(!state.visible);
    nk_free(&ctx); puts("Real Nuklear paginated archives, exact selection, restore and cancel passed");
    return 0;
}
