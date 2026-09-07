#include "../client/features/card_archives.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

typedef struct Store { WenaCard *cards; int calls; int fail; unsigned long version; } Store;
static int load(void *data, const char *board, const char *id, char *title,
    size_t capacity, unsigned long *version)
{
    Store *store; store=(Store *)data;
    assert(!strcmp(board,"board")); assert(capacity>1);
    assert(!strcmp(id,"one") || !strcmp(id,"two"));
    strcpy(title,"X"); *version=store->version; return !store->fail;
}
static int restore(void *data,const char *board,const char *id,unsigned long version)
{
    Store *store; int i; store=(Store *)data; ++store->calls;
    assert(!strcmp(board,"board"));
    if(store->fail || version!=store->version) return 0;
    for(i=0;i<3;++i) if(!strcmp(store->cards[i].id,id)) {
        assert(store->cards[i].archived); store->cards[i].archived=0; return 1;
    }
    return 0;
}
static void frame(WenaCardArchivesState *state,WenaBoardLayout *layout,
    const char *button,const char *option)
{
    struct nk_context context; memset(&context,0,sizeof(context));
    context.button_to_press=button; context.combo_item_to_press=option;
    assert(wena_card_archives_render(&context,state,layout,800,600));
    assert(context.begin_count==context.end_count);
}
int main(void)
{
    WenaBoard board; WenaCard cards[4]; WenaBoardLayout layout;
    WenaCardArchivesState state; Store store; struct nk_context context;
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_card_init(&cards[0],"one","board","lane","list","Repeated",0,1));
    assert(wena_card_init(&cards[1],"two","board","lane","list","Repeated",1,1));
    assert(wena_card_init(&cards[2],"active","board","lane","list","Active",2,0));
    assert(wena_card_init(&cards[3],"foreign","wrong","lane","list","Foreign",0,1));
    memset(&layout,0,sizeof(layout)); memset(&store,0,sizeof(store));
    memset(&context,0,sizeof(context)); store.cards=cards; store.version=1;
    layout.board=&board; layout.cards=cards; layout.card_count=4;
    wena_card_archives_init(&state,load,restore,&store);
    assert(wena_card_archives_open(&state,&layout));
    assert(!strcmp(state.card_id,"one") && state.version==1);
    frame(&state,&layout,NULL,"Repeated [two]");
    assert(!strcmp(state.card_id,"two"));
    frame(&state,&layout,NULL,"Foreign [foreign]");
    frame(&state,&layout,NULL,"Active [active]");
    assert(!strcmp(state.card_id,"two"));
    frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible && store.calls==0 && cards[1].archived);
    assert(wena_card_archives_open(&state,&layout));
    ++store.version;
    frame(&state,&layout,"Restore",NULL);
    assert(state.error && !strcmp(state.card_id,"one") && cards[0].archived);
    frame(&state,&layout,NULL,"Repeated [two]");
    store.fail=1; frame(&state,&layout,"Restore",NULL);
    assert(state.error && !strcmp(state.card_id,"two") && cards[1].archived);
    store.fail=0; frame(&state,&layout,"Restore",NULL);
    assert(!cards[1].archived && !strcmp(state.card_id,"one"));
    frame(&state,&layout,"Restore",NULL);
    assert(!cards[0].archived && state.card_id[0]==0);
    assert(store.calls==4);
    frame(&state,&layout,"Restore",NULL);
    assert(store.calls==4 && state.visible);
    frame(&state,&layout,"Close details",NULL);
    assert(!state.visible);
    cards[0].archived=1; store.fail=1;
    assert(wena_card_archives_open(&state,&layout));
    assert(state.error && state.version==0);
    frame(&state,&layout,"Restore",NULL); assert(store.calls==4);
    store.fail=0; assert(wena_card_archives_open(&state,&layout));
    cards[0].archived=0;
    frame(&state,&layout,"Restore",NULL);
    assert(store.calls==4 && state.card_id[0]==0);
    strcpy(board.id,"wrong");
    assert(!wena_card_archives_render(&context,&state,&layout,800,600));
    assert(!state.visible);
    return 0;
}
