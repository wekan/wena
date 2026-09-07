#include "../client/features/card_move.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

typedef struct Store { int calls; int fail; unsigned long version; } Store;
static int load(void *data, const char *board, const char *card, char *title,
    size_t capacity, unsigned long *version)
{
    Store *store; store = (Store *)data;
    assert(!strcmp(board,"board") && !strcmp(card,"card") && capacity > 1);
    strcpy(title,"X"); *version=store->version; return !store->fail;
}
static int move(void *data, const char *board, const char *card,
    unsigned long version, const char *list, const char *lane)
{
    Store *store; store = (Store *)data; ++store->calls;
    assert(!strcmp(board,"board") && !strcmp(card,"card"));
    assert(!strcmp(list,"second-list") && !strcmp(lane,"second-lane"));
    return !store->fail && version == store->version;
}
static void frame(WenaCardMoveState *state, WenaBoardLayout *layout,
    const char *button, const char *option)
{
    struct nk_context context; memset(&context,0,sizeof(context));
    context.button_to_press=button; context.combo_item_to_press=option;
    assert(wena_card_move_render(&context,state,layout,800,600));
    assert(context.begin_count==context.end_count);
}
int main(void)
{
    WenaBoard board; WenaList lists[2]; WenaSwimlane lanes[2]; WenaCard card;
    WenaBoardLayout layout; WenaCardMoveState state; Store store;
    struct nk_context context;
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"first-list","board","","Repeated",0,0));
    assert(wena_list_init(&lists[1],"second-list","board","","Repeated",1,0));
    assert(wena_swimlane_init(&lanes[0],"first-lane","board","Repeated",0,0));
    assert(wena_swimlane_init(&lanes[1],"second-lane","board","Repeated",1,0));
    assert(wena_card_init(&card,"card","board","first-lane","first-list","Card",0,0));
    memset(&layout,0,sizeof(layout)); memset(&store,0,sizeof(store));
    memset(&context,0,sizeof(context)); store.version=1;
    layout.board=&board; layout.lists=lists; layout.list_count=2;
    layout.swimlanes=lanes; layout.swimlane_count=2; layout.cards=&card; layout.card_count=1;
    wena_card_move_init(&state,load,move,&store);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"Repeated [second-lane]");
    frame(&state,&layout,NULL,"Repeated [second-list]");
    frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible && store.calls==0);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"Repeated [second-lane]");
    frame(&state,&layout,NULL,"Repeated [second-list]");
    ++store.version;
    frame(&state,&layout,"Save",NULL);
    assert(state.visible && state.error && store.calls==1);
    assert(!strcmp(state.target_list_id,"second-list"));
    frame(&state,&layout,"Cancel",NULL);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"Repeated [second-lane]");
    frame(&state,&layout,NULL,"Repeated [second-list]");
    lists[1].archived=1;
    frame(&state,&layout,"Save",NULL);
    assert(state.error && store.calls==1);
    lists[1].archived=0; lanes[1].archived=1;
    frame(&state,&layout,"Save",NULL);
    assert(state.error && store.calls==1);
    lanes[1].archived=0; strcpy(lists[1].board_id,"wrong");
    frame(&state,&layout,"Save",NULL);
    assert(state.error && store.calls==1);
    strcpy(lists[1].board_id,"board");
    store.fail=1; frame(&state,&layout,"Save",NULL);
    assert(state.error && state.visible && store.calls==2);
    store.fail=0; frame(&state,&layout,"Save",NULL);
    assert(!state.visible && store.calls==3);
    assert(wena_card_move_open(&state,&layout,"card"));
    strcpy(card.list_id,"second-list");
    assert(!wena_card_move_render(&context,&state,&layout,800,600));
    assert(!state.visible);
    strcpy(card.list_id,"first-list");
    assert(wena_card_move_open(&state,&layout,"card"));
    card.archived=1;
    assert(!wena_card_move_render(&context,&state,&layout,800,600));
    assert(!wena_card_move_open(&state,&layout,"card"));
    card.archived=0; store.fail=1;
    assert(!wena_card_move_open(&state,&layout,"card"));
    store.fail=0; assert(!wena_card_move_open(&state,&layout,"missing"));
    assert(wena_card_move_open(&state,&layout,"card"));
    strcpy(board.id,"wrong");
    assert(!wena_card_move_render(&context,&state,&layout,800,600));
    strcpy(board.id,"board");
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,"Close details",NULL);
    assert(!state.visible && store.calls==3);
    return 0;
}
