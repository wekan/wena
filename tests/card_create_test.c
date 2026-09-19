#include "../client/features/card_create.h"
#include "../client/components/lists/list_header.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

typedef struct Store {
    int calls;
    int fail;
    char title[129];
} Store;

static int create(void *data, const char *board, const char *list,
    const char *lane, const char *title)
{
    Store *store;
    store = (Store *)data;
    ++store->calls;
    assert(!strcmp(board, "board") && !strcmp(list, "list") && !strcmp(lane, "second"));
    if (store->fail) return 0;
    strcpy(store->title, title);
    return 1;
}

static void frame(WenaCardCreateState *state, WenaBoardLayout *layout,
    const char *button, const char *input)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button; context.edit_text = input;
    assert(wena_card_create_render(&context, state, layout, 800, 600));
    assert(context.begin_count == context.end_count);
}

int main(void)
{
    WenaBoard board;
    WenaList list;
    WenaSwimlane lanes[2];
    WenaBoardLayout layout;
    WenaBoardCollapseState collapse;
    WenaListInteraction interaction;
    WenaCardCreateState state;
    Store store;
    struct nk_context context;
    char oversized[200];
    assert(wena_board_init(&board, "board", "Board", 0));
    assert(wena_list_init(&list, "list", "board", "", "List", 0, 0));
    assert(wena_swimlane_init(&lanes[0], "first", "board", "First", 0, 0));
    assert(wena_swimlane_init(&lanes[1], "second", "board", "Second", 1, 0));
    memset(&layout, 0, sizeof(layout)); memset(&store, 0, sizeof(store));
    memset(&context, 0, sizeof(context)); memset(&interaction, 0, sizeof(interaction));
    layout.board = &board; layout.lists = &list; layout.list_count = 1;
    layout.swimlanes = lanes; layout.swimlane_count = 2;
    layout.list_interaction = &interaction; layout.collapse = &collapse;
    wena_board_collapse_init(&collapse);
    assert(wena_board_collapse_sync(&collapse, &layout));
    assert(wena_board_collapse_set(&collapse, &layout, WENA_COLLAPSE_SWIMLANE, "first", 1));
    context.button_to_press = "Add card";
    assert(wena_board_layout_render(&context, &layout));
    assert(interaction.actions == WENA_LIST_HEADER_ADD_CARD);
    assert(!strcmp(interaction.board_id, "board"));
    assert(!strcmp(interaction.list_id, "list"));
    assert(!strcmp(interaction.swimlane_id, "second"));
    wena_card_create_init(&state, create, &store);
    assert(wena_card_create_open(&state, &layout, &interaction));
    frame(&state, &layout, "Save", "");
    assert(state.error && state.visible && store.calls == 0);
    memset(oversized, 'x', sizeof(oversized)); oversized[sizeof(oversized)-1] = 0;
    frame(&state, &layout, "Save", oversized);
    /* The edit buffer retains one complete over-limit UTF-8 scalar; saving
     * still uses the tighter persisted-title bound. */
    assert(state.error && state.title_length ==
        WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY) - 1 &&
        store.calls == 0);
    frame(&state, &layout, "Save", "Bad\nTitle");
    assert(state.error && store.calls == 0);
    frame(&state, &layout, "Save", "Bad\300\257");
    assert(state.error && store.calls == 0);
    store.fail = 1;
    frame(&state, &layout, "Save", "Retry title");
    assert(state.error && state.visible && store.calls == 1);
    store.fail = 0;
    frame(&state, &layout, "Save", NULL);
    assert(!state.visible && store.calls == 2 && !strcmp(store.title, "Retry title"));
    assert(wena_card_create_open(&state, &layout, &interaction));
    frame(&state, &layout, "Cancel", "Discarded");
    assert(!state.visible && store.calls == 2 && state.title_input[0] == 0);
    assert(wena_card_create_open(&state, &layout, &interaction));
    frame(&state, &layout, "Close details", NULL);
    assert(!state.visible && store.calls == 2);
    assert(wena_card_create_open(&state, &layout, &interaction));
    lanes[1].archived = 1;
    assert(!wena_card_create_render(&context, &state, &layout, 800, 600));
    assert(!state.visible && store.calls == 2);
    assert(!wena_card_create_open(&state, &layout, &interaction));
    lanes[1].archived = 0;
    strcpy(interaction.board_id, "wrong");
    assert(!wena_card_create_open(&state, &layout, &interaction));
    strcpy(interaction.board_id, "board"); strcpy(interaction.list_id, "wrong");
    assert(!wena_card_create_open(&state, &layout, &interaction));
    strcpy(interaction.list_id, "list"); strcpy(list.swimlane_id, "first");
    assert(!wena_card_create_open(&state, &layout, &interaction));
    list.swimlane_id[0] = 0;
    assert(wena_card_create_open(&state, &layout, &interaction));
    strcpy(board.id, "changed");
    assert(!wena_card_create_render(&context, &state, &layout, 800, 600));
    strcpy(board.id, "board");
    assert(wena_card_create_open(&state, &layout, &interaction));
    list.archived = 1;
    assert(!wena_card_create_render(&context, &state, &layout, 800, 600));
    list.archived = 0;
    assert(wena_board_layout_render(&context, &layout));
    assert(interaction.actions == 0u && interaction.board_id[0] == 0 &&
           interaction.swimlane_id[0] == 0 && interaction.list_id[0] == 0);
    assert(!wena_card_create_open(&state, &layout, &interaction));
    return 0;
}
