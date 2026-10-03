#include "../client/features/card_create.h"
#include "../client/components/lists/list_header.h"
#include "../client/components/common/wekan_look.h"
#include "../client/platform/nuklear_options.h"
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

static const WenaBoardLayout *current_layout;
static int top_calls, top_fail, composer_calls;
static int to_top(void *data, const WenaBoardLayout *layout)
{
    (void)layout;
    ++*(int *)data;
    return !top_fail;
}
static int inline_frame(WenaCardCreateState *state, WenaBoardLayout *layout,
    const WenaSwimlane *lane, int bottom, const char *button, const char *input)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.current = &context.window; context.window.layout = &context.panel;
    context.button_to_press = button; context.edit_text = input;
    return wena_card_create_render_inline(&context, state, layout, &layout->lists[0], lane, bottom);
}
static int composer(struct nk_context *context, void *data, const WenaList *list,
                    const WenaSwimlane *lane, int bottom)
{
    ++composer_calls;
    return wena_card_create_render_inline(context, (WenaCardCreateState *)data, current_layout,
                                          list, lane, bottom);
}
static int control(const char *name)
{
    size_t count, index;
    const WenaUiControl *controls = wena_ui_controls(&count);
    for (index = 0; index < count; ++index)
        if (!strcmp(controls[index].name, name)) return 1;
    return 0;
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
    context.button_to_press = "Add Card to Top of List";
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

    /* WeKan's inline composer. "+ Add Card" asks for the bottom one. */
    memset(&context, 0, sizeof(context));
    context.button_to_press = "Add Card";
    assert(wena_board_layout_render(&context, &layout));
    assert(interaction.actions == (WENA_LIST_HEADER_ADD_CARD | WENA_LIST_HEADER_ADD_CARD_BOTTOM));
    assert(!strcmp(interaction.swimlane_id, "second"));
    assert(wena_card_create_open(&state, &layout, &interaction) && state.bottom && state.focus);
    /* Only its own list, swimlane and end draw it. */
    memset(&context, 0, sizeof(context));
    assert(!inline_frame(&state, &layout, &lanes[1], 0, NULL, NULL));
    assert(!inline_frame(&state, &layout, &lanes[0], 1, NULL, NULL));
    assert(inline_frame(&state, &layout, &lanes[1], 1, NULL, NULL) && !state.focus);
    /* Add creates it and keeps the composer open, empty, for the next card. */
    top_calls = 0; state.to_top = to_top; state.to_top_context = &top_calls;
    assert(inline_frame(&state, &layout, &lanes[1], 1, "Add", "Inline card"));
    assert(store.calls == 3 && !strcmp(store.title, "Inline card"));
    assert(state.visible && state.title_length == 0 && !state.error && top_calls == 0);
    /* Negative: an empty title creates nothing and says so. */
    assert(inline_frame(&state, &layout, &lanes[1], 1, "Add", ""));
    assert(store.calls == 3 && state.error && state.visible);
    /* The cross closes it. */
    assert(inline_frame(&state, &layout, &lanes[1], 1, "Close", NULL) && !state.visible);
    /* While the composer is at the bottom it replaces "+ Add Card". */
    assert(wena_card_create_open(&state, &layout, &interaction));
    layout.card_composer = composer; layout.card_composer_context = &state; current_layout = &layout;
    memset(&context, 0, sizeof(context));
    wena_ui_controls_begin();
    assert(wena_board_layout_render(&context, &layout));
    assert(composer_calls == 2 && !control("Add Card") && control("Add Card to Bottom of List"));
    /* Add Card to Top of List: the composer above the cards, and the new card
     * moved first. */
    wena_card_create_close(&state);
    interaction.actions = WENA_LIST_HEADER_ADD_CARD;
    strcpy(interaction.board_id, "board"); strcpy(interaction.list_id, "list");
    strcpy(interaction.swimlane_id, "second");
    assert(wena_card_create_open(&state, &layout, &interaction) && !state.bottom);
    state.to_top = to_top; state.to_top_context = &top_calls;
    memset(&context, 0, sizeof(context));
    assert(!inline_frame(&state, &layout, &lanes[1], 1, NULL, NULL));
    assert(inline_frame(&state, &layout, &lanes[1], 0, "Add", "On top"));
    assert(store.calls == 4 && top_calls == 1 && !state.error);
    /* Negative: a card that could not be moved first is reported. */
    top_fail = 1;
    assert(inline_frame(&state, &layout, &lanes[1], 0, "Add", "Left last"));
    assert(store.calls == 5 && top_calls == 2 && state.error && state.title_length == 0);
    layout.card_composer = NULL;
    return 0;
}
