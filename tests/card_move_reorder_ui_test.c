#include "../client/features/card_move.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

typedef struct Store { int appended; int reordered; int fail; unsigned long target; } Store;
static int load(void *data,const char *board,const char *card,char *title,
    size_t capacity,unsigned long *version)
{ (void)data; (void)capacity; assert(!strcmp(board,"board")&&!strcmp(card,"card")); strcpy(title,"Card"); *version=1; return 1; }
static int append(void *data,const char *board,const char *card,unsigned long version,
    const char *list,const char *lane)
{ Store *s=(Store*)data; (void)board;(void)card;(void)list;(void)lane;assert(version==1);++s->appended;return !s->fail; }
static int reorder(void *data,const char *board,const char *card,unsigned long version,unsigned long target)
{ Store *s=(Store*)data;assert(!strcmp(board,"board")&&!strcmp(card,"card")&&version==1);++s->reordered;s->target=target;return !s->fail; }
static void frame(WenaCardMoveState *state,WenaBoardLayout *layout,const char *button,const char *option)
{ struct nk_context c;memset(&c,0,sizeof(c));c.button_to_press=button;c.combo_item_to_press=option;assert(wena_card_move_render(&c,state,layout,800,600)); }
int main(void)
{
    WenaBoard board;WenaList lists[2];WenaSwimlane lane;WenaCard cards[4];
    WenaBoardLayout layout;WenaCardMoveState state;Store store;
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"list","board","","List",0,0));
    assert(wena_list_init(&lists[1],"other","board","","Other",1,0));
    assert(wena_swimlane_init(&lane,"lane","board","Lane",0,0));
    assert(wena_card_init(&cards[0],"card","board","lane","list","Card",3,0));
    assert(wena_card_init(&cards[1],"old","board","lane","list","Old",1,1));
    assert(wena_card_init(&cards[2],"last","board","lane","list","Last",8,0));
    assert(wena_card_init(&cards[3],"foreign","board","lane","other","Foreign",0,0));
    memset(&layout,0,sizeof(layout));memset(&store,0,sizeof(store));
    layout.board=&board;layout.lists=lists;layout.list_count=2;layout.swimlanes=&lane;layout.swimlane_count=1;layout.cards=cards;layout.card_count=4;
    wena_card_move_init(&state,load,append,&store);
    wena_card_move_set_reorder_adapter(&state,reorder);
    assert(wena_card_move_open(&state,&layout,"card"));
    assert(state.order_count==3 && !strcmp(state.order[0].id,"old")&&state.order[0].archived);
    frame(&state,&layout,"Save",NULL);assert(!state.visible&&store.appended==1&&store.reordered==0&&state.order==NULL);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"2. Card [card]");frame(&state,&layout,"Save",NULL);
    assert(!state.visible&&store.reordered==1&&store.target==1);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"1. Old (Archived) [old]");frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible&&state.order==NULL&&store.reordered==1);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"1. Old (Archived) [old]");
    cards[1].sort=2;frame(&state,&layout,"Save",NULL);assert(state.error&&store.reordered==1);
    cards[1].sort=1;cards[1].archived=0;frame(&state,&layout,"Save",NULL);assert(store.reordered==1);
    cards[1].archived=1;store.fail=1;frame(&state,&layout,"Save",NULL);assert(state.error&&state.reorder_choice==1&&store.reordered==2);
    store.fail=0;frame(&state,&layout,"Save",NULL);assert(!state.visible&&store.reordered==3&&store.target==0);
    assert(wena_card_move_open(&state,&layout,"card"));
    state.reorder_choice=4;frame(&state,&layout,"Save",NULL);assert(state.error&&store.reordered==3);
    state.reorder_choice=-1;frame(&state,&layout,"Save",NULL);assert(state.error&&store.reordered==3);
    frame(&state,&layout,NULL,"1. Old (Archived) [old]");frame(&state,&layout,NULL,"Other [other]");
    assert(state.reorder_choice==0);frame(&state,&layout,"Save",NULL);assert(store.appended==2&&!state.visible);
    cards[1].sort=3;assert(!wena_card_move_open(&state,&layout,"card"));assert(state.order==NULL);
    cards[1].sort=-1;assert(!wena_card_move_open(&state,&layout,"card"));assert(state.order==NULL);
    cards[1].sort=1.5;assert(!wena_card_move_open(&state,&layout,"card"));assert(state.order==NULL);
    cards[1].sort=1;assert(wena_card_move_open(&state,&layout,"card"));
    wena_card_move_set_reorder_adapter(&state,NULL);assert(state.order==NULL&&!state.visible);
    wena_card_move_close(&state);wena_card_move_close(&state);
    return 0;
}
