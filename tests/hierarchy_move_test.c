#include "../client/features/hierarchy_move.h"
#include "../imports/ui/page_contract.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

typedef struct Store { int calls; int writes; int fail; unsigned long version; } Store;
static int load(void *data,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long *version,unsigned long *position)
{
    Store *store; store=(Store*)data;
    assert(!strcmp(board,"board") && !strcmp(id,"one"));
    assert(kind==WENA_HIERARCHY_LIST || kind==WENA_HIERARCHY_SWIMLANE);
    *version=store->version; *position=0; return !store->fail;
}
static int move(void *data,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long version,unsigned long position)
{
    Store *store; store=(Store*)data; ++store->calls;
    assert(!strcmp(board,"board") && !strcmp(id,"one"));
    assert(kind==WENA_HIERARCHY_LIST || kind==WENA_HIERARCHY_SWIMLANE);
    if(store->fail || version!=store->version) return 0;
    if(position!=0) ++store->writes;
    return 1;
}
static int title_load(void *data,const char *board,WenaHierarchyKind kind,
    const char *id,char *title,size_t capacity,unsigned long *version)
{
    unsigned long position;
    assert(capacity>9); strcpy(title,"Repeated");
    return load(data,board,kind,id,version,&position);
}
static int title_save(void *data,const char *board,WenaHierarchyKind kind,
    const char *id,unsigned long version,const char *title)
{
    (void)data; (void)board; (void)kind; (void)id; (void)version; (void)title;
    assert(0); return 0;
}
static void frame(WenaHierarchyMoveState *state,WenaBoardLayout *layout,
    const char *button,const char *option)
{
    struct nk_context context; memset(&context,0,sizeof(context));
    context.button_to_press=button; context.combo_item_to_press=option;
    assert(wena_hierarchy_move_render(&context,state,layout,800,600));
    assert(context.begin_count==context.end_count);
}
static void test_kind(WenaHierarchyKind kind)
{
    WenaBoard board; WenaList lists[2]; WenaSwimlane lanes[2];
    WenaBoardLayout layout; WenaHierarchyMoveState state; Store store;
    struct nk_context context; WenaHierarchyTitleState title_state;
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"one","board","","Repeated",0,0));
    assert(wena_list_init(&lists[1],"two","board","","Repeated",1,0));
    assert(wena_swimlane_init(&lanes[0],"one","board","Repeated",0,0));
    assert(wena_swimlane_init(&lanes[1],"two","board","Repeated",1,0));
    memset(&layout,0,sizeof(layout)); memset(&store,0,sizeof(store));
    memset(&context,0,sizeof(context)); store.version=1;
    layout.board=&board; layout.lists=lists; layout.list_count=2;
    layout.swimlanes=lanes; layout.swimlane_count=2;
    wena_hierarchy_title_init(&title_state);
    wena_hierarchy_title_set_adapter(&title_state,title_load,title_save,&store);
    assert(wena_hierarchy_title_open(&title_state,&layout,kind,"one"));
    context.button_to_press=wena_ui_control_text(kind==WENA_HIERARCHY_LIST ?
        WENA_UI_MOVE_LIST_TO : WENA_UI_MOVE_SWIMLANE_TO);
    assert(wena_hierarchy_title_render(&context,&title_state,&layout,800,600));
    assert(title_state.requested_action==WENA_HIERARCHY_TITLE_MOVE);
    assert(!strcmp(title_state.target_id,"one") && title_state.kind==kind);
    assert(wena_hierarchy_title_render(&context,&title_state,&layout,800,600));
    assert(title_state.requested_action==0u);
    assert(!strcmp(title_state.title_input,"Repeated"));
    wena_hierarchy_move_init(&state,load,move,&store);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    frame(&state,&layout,"Save",NULL);
    assert(!state.visible && store.calls==1 && store.writes==0);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    frame(&state,&layout,NULL,"2. Repeated [two]");
    assert(state.target_position==1);
    frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible && store.calls==1);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    ++store.version;
    frame(&state,&layout,"Save",NULL);
    assert(state.visible && state.error && store.calls==2 && store.writes==0);
    frame(&state,&layout,"Cancel",NULL);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    frame(&state,&layout,NULL,"2. Repeated [two]");
    store.fail=1; frame(&state,&layout,"Save",NULL);
    assert(state.error && state.visible && state.target_position==1);
    store.fail=0; frame(&state,&layout,"Save",NULL);
    assert(!state.visible && store.writes==1);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    state.target_position=2; frame(&state,&layout,"Save",NULL);
    assert(state.error && state.visible && store.writes==1);
    state.target_position=-1; frame(&state,&layout,"Save",NULL);
    assert(state.error && state.visible && store.writes==1);
    frame(&state,&layout,"Close details",NULL);
    assert(!state.visible);
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    lists[0].sort=1; lists[1].sort=0; lanes[0].sort=1; lanes[1].sort=0;
    assert(!wena_hierarchy_move_render(&context,&state,&layout,800,600));
    assert(!state.visible);
    assert(!wena_hierarchy_move_open(&state,&layout,kind,"one"));
    lists[0].sort=0; lanes[0].sort=0;
    assert(!wena_hierarchy_move_open(&state,&layout,kind,"one"));
    lists[1].sort=1.5; lanes[1].sort=1.5;
    assert(!wena_hierarchy_move_open(&state,&layout,kind,"one"));
    lists[1].sort=1; lanes[1].sort=1;
    assert(wena_hierarchy_move_open(&state,&layout,kind,"one"));
    strcpy(board.id,"wrong");
    assert(!wena_hierarchy_move_render(&context,&state,&layout,800,600));
    strcpy(board.id,"board");
    assert(!wena_hierarchy_move_open(&state,&layout,WENA_HIERARCHY_BOARD,"board"));
    assert(!wena_hierarchy_move_open(&state,&layout,kind,"missing"));
}
int main(void) { test_kind(WENA_HIERARCHY_LIST); test_kind(WENA_HIERARCHY_SWIMLANE); return 0; }
