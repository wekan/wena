#include "../client/components/boards/board_layout.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char groups[256][240];
static size_t group_count;
static size_t collapse_skip;

void nk_layout_row_dynamic(struct nk_context *context, float height, int columns)
{ (void)context; (void)height; (void)columns; }
void nk_layout_row_begin(struct nk_context *context, int format,
                         float height, int columns)
{ (void)context; (void)format; (void)height; (void)columns; }
void nk_layout_row_push(struct nk_context *context, float value)
{ (void)context; (void)value; }
void nk_layout_row_end(struct nk_context *context)
{ (void)context; }
void nk_label(struct nk_context *context, const char *text, int alignment)
{
    (void)alignment;
    assert(context->label_count < 256);
    context->labels[context->label_count++] = text;
}
int nk_button_label(struct nk_context *context, const char *text)
{
    ++context->button_count;
    if (context->button_to_press == NULL ||
        strcmp(context->button_to_press, text) != 0) { return 0; }
    if (collapse_skip != 0) { --collapse_skip; return 0; }
    context->button_to_press = NULL;
    return 1;
}
int nk_group_begin(struct nk_context *context, const char *title,
                   unsigned int flags)
{
    (void)flags;
    assert(group_count < 256 && strlen(title) < sizeof(groups[0]));
    strcpy(groups[group_count++], title);
    ++context->group_depth;
    return 1;
}
void nk_group_end(struct nk_context *context)
{ --context->group_depth; }

static int has_label(struct nk_context *context, const char *label)
{
    int index;
    for (index = 0; index < context->label_count; ++index) {
        if (strcmp(context->labels[index], label) == 0) { return 1; }
    }
    return 0;
}
static void render(struct nk_context *context, WenaBoardLayout *layout,
                    const char *button, size_t skip)
{
    memset(context, 0, sizeof(*context));
    group_count = 0;
    collapse_skip = skip;
    context->button_to_press = button;
    assert(wena_board_layout_render(context, layout));
    assert(context->group_depth == 0);
    if (button != NULL) { assert(context->button_to_press == NULL); }
}

static void test_board_wide_lists(void)
{
    WenaBoard board;
    WenaSwimlane lanes[3];
    WenaList lists[2];
    WenaCard cards[4];
    WenaBoardLayout layout;
    WenaBoardCollapseState state;
    struct nk_context context;

    assert(wena_board_init(&board, "board", "Board", 0));
    assert(wena_swimlane_init(&lanes[0], "first", "board", "First", 1.0, 0));
    assert(wena_swimlane_init(&lanes[1], "second", "board", "Second", 2.0, 0));
    assert(wena_swimlane_init(&lanes[2], "foreign", "other", "Foreign lane", 3.0, 0));
    assert(wena_list_init(&lists[0], "shared", "board", "", "Shared", 1.0, 0));
    assert(wena_list_init(&lists[1], "local", "board", "first", "Local", 2.0, 0));
    assert(wena_card_init(&cards[0], "one", "board", "first", "shared", "One", 1.0, 0));
    assert(wena_card_init(&cards[1], "two", "board", "second", "shared", "Two", 1.0, 0));
    assert(wena_card_init(&cards[2], "local-card", "board", "first", "local", "Local card", 1.0, 0));
    assert(wena_card_init(&cards[3], "foreign-card", "other", "first", "shared", "Foreign card", 1.0, 0));
    memset(&layout, 0, sizeof(layout));
    layout.board = &board;
    layout.swimlanes = lanes;
    layout.swimlane_count = 3;
    layout.lists = lists;
    layout.list_count = 2;
    layout.cards = cards;
    layout.card_count = 4;
    layout.collapse = &state;
    wena_board_collapse_init(&state);
    render(&context, &layout, NULL, 0);
    assert(group_count == 5);
    assert(strcmp(groups[1], groups[4]) != 0);
    assert(has_label(&context, "One") && has_label(&context, "Two"));
    assert(has_label(&context, "Local card"));
    assert(!has_label(&context, "Foreign card"));
    assert(!has_label(&context, "Foreign lane"));
    /* Shared-list state follows the board-wide list in every displayed lane. */
    render(&context, &layout, "Collapse", 1);
    assert(!has_label(&context, "One") && !has_label(&context, "Two"));
    assert(has_label(&context, "Local card"));
    assert(wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "shared"));
    render(&context, &layout, "Uncollapse", 0);
    assert(has_label(&context, "One") && has_label(&context, "Two"));
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "shared", 1));
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "local", 1));
    lanes[0].archived = 1;
    render(&context, &layout, NULL, 0);
    assert(state.list_count == 1);
    assert(wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "shared"));
    assert(!wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "local"));
    lanes[1].archived = 1;
    render(&context, &layout, NULL, 0);
    assert(group_count == 0 && state.list_count == 0);
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "shared", 1));
    lanes[0].archived = 0;
    strcpy(lists[0].board_id, "other");
    render(&context, &layout, NULL, 0);
    assert(!has_label(&context, "Shared") && has_label(&context, "Local card"));
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "shared", 1));
    strcpy(lists[0].board_id, "board");
    strcpy(lists[0].swimlane_id, "missing");
    render(&context, &layout, NULL, 0);
    assert(!has_label(&context, "Shared"));
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "shared", 1));
}

int main(void)
{
    WenaBoard board;
    WenaBoard other;
    WenaSwimlane lanes[WENA_BOARD_COLLAPSE_CAPACITY + 1];
    WenaList lists[WENA_BOARD_COLLAPSE_CAPACITY + 1];
    WenaCard cards[2];
    WenaCard original[2];
    WenaBoardLayout layout;
    WenaBoardCollapseState state;
    WenaBoardCollapseState before;
    WenaList swapped;
    WenaListInteraction list_action;
    WenaCardInteraction card_action;
    struct nk_context context;
    char id[32];
    char oversized[WENA_ID_CAPACITY + 1];
    char first_group[240];
    size_t index;

    assert(wena_board_init(&board, "board", "Board", 0));
    assert(wena_board_init(&other, "other", "Other", 0));
    for (index = 0; index <= WENA_BOARD_COLLAPSE_CAPACITY; ++index) {
        (void)sprintf(id, "id-%lu", (unsigned long)index);
        assert(wena_swimlane_init(&lanes[index], id, "board", "Lane", 1.0, 0));
        assert(wena_list_init(&lists[index], id, "board", "id-0", "List", 1.0, 0));
    }
    assert(wena_card_init(&cards[0], "card", "board", "id-0", "id-0", "Card", 1.0, 0));
    assert(wena_card_init(&cards[1], "foreign", "other", "id-0", "id-0", "Foreign", 1.0, 0));
    memcpy(original, cards, sizeof(cards));
    memset(&layout, 0, sizeof(layout));
    layout.board = &board;
    layout.swimlanes = lanes;
    layout.swimlane_count = 2;
    layout.lists = lists;
    layout.list_count = 2;
    layout.cards = cards;
    layout.card_count = 2;
    layout.collapse = &state;
    layout.list_interaction = &list_action;
    layout.card_interaction = &card_action;
    wena_board_collapse_init(&state);
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    render(&context, &layout, NULL, 0);
    assert(has_label(&context, "Card") && !has_label(&context, "Foreign"));
    assert(group_count == 4);
    assert(strcmp(groups[1], groups[2]) != 0);
    strcpy(first_group, groups[0]);
    strcpy(lanes[0].title, "Renamed lane");
    render(&context, &layout, NULL, 0);
    assert(strcmp(first_group, groups[0]) == 0);

    /* List collapse hides cards immediately; lane collapse preserves child state. */
    render(&context, &layout, "Collapse", 1);
    assert(!has_label(&context, "Card"));
    assert(wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "id-0"));
    assert(card_action.actions == 0u && card_action.card_id[0] == '\0');
    render(&context, &layout, "Collapse", 0);
    assert(!has_label(&context, "List") && group_count == 2);
    assert(list_action.actions == 0u && list_action.list_id[0] == '\0');
    render(&context, &layout, "Uncollapse", 0);
    assert(has_label(&context, "List") && !has_label(&context, "Card"));
    render(&context, &layout, "Uncollapse", 0);
    assert(has_label(&context, "Card"));
    assert(memcmp(cards, original, sizeof(cards)) == 0);
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    /* Reordering and renaming do not retarget selection to an array position. */
    swapped = lists[0];
    lists[0] = lists[1];
    lists[1] = swapped;
    strcpy(lists[1].title, "Renamed list");
    render(&context, &layout, NULL, 0);
    assert(!has_label(&context, "Card"));
    assert(wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "id-0"));
    swapped = lists[0];
    lists[0] = lists[1];
    lists[1] = swapped;
    before = state;
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "missing", 1));
    assert(!wena_board_collapse_set(&state, &layout, (WenaBoardCollapseKind)99, "id-0", 1));
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "", 1));
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, NULL, 1));
    memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized) - 1] = '\0';
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, oversized, 1));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(!wena_board_is_collapsed(&state, "other", WENA_COLLAPSE_LIST, "id-0"));
    layout.board = &other;
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 0));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(wena_board_collapse_sync(&state, &layout));
    assert(state.list_count == 0 && strcmp(state.board_id, "other") == 0);
    layout.board = &board;
    assert(wena_board_collapse_sync(&state, &layout));
    assert(!wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "id-0"));

    /* Archived, deleted and cross-board children are pruned, including hidden ones. */
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    lists[0].archived = 1;
    assert(wena_board_collapse_sync(&state, &layout) && state.list_count == 0);
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    lists[0].archived = 0;
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    strcpy(lists[0].board_id, "other");
    assert(wena_board_collapse_sync(&state, &layout) && state.list_count == 0);
    strcpy(lists[0].board_id, "board");
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 1));
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_SWIMLANE, "id-0", 1));
    lanes[0].archived = 1;
    assert(wena_board_collapse_sync(&state, &layout));
    assert(state.list_count == 0 && state.swimlane_count == 0);
    lanes[0].archived = 0;
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-1", 1));
    layout.list_count = 1;
    assert(wena_board_collapse_sync(&state, &layout) && state.list_count == 0);

    /* Each independently bounded collection fails atomically and reuses freed slots. */
    layout.list_count = WENA_BOARD_COLLAPSE_CAPACITY + 1;
    layout.swimlane_count = WENA_BOARD_COLLAPSE_CAPACITY + 1;
    for (index = 0; index < WENA_BOARD_COLLAPSE_CAPACITY; ++index) {
        assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, lists[index].id, 1));
        assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_SWIMLANE, lanes[index].id, 1));
    }
    before = state;
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, lists[index].id, 1));
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_SWIMLANE, lanes[index].id, 1));
    assert(memcmp(&before, &state, sizeof(state)) == 0);
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, lists[0].id, 0));
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, lists[index].id, 1));
    assert(wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, lists[index].id));
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_SWIMLANE, lanes[0].id, 0));
    assert(wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_SWIMLANE, lanes[index].id, 1));
    before = state;
    layout.swimlanes = NULL;
    assert(!wena_board_collapse_sync(&state, &layout));
    assert(memcmp(&before, &state, sizeof(state)) == 0);
    layout.swimlanes = lanes;
    state.list_count = WENA_BOARD_COLLAPSE_CAPACITY + 1;
    assert(!wena_board_collapse_sync(&state, &layout));
    assert(!wena_board_is_collapsed(&state, "board", WENA_COLLAPSE_LIST, "id-0"));
    assert(!wena_board_collapse_set(&state, &layout, WENA_COLLAPSE_LIST, "id-0", 0));
    wena_board_collapse_init(NULL);
    assert(!wena_board_collapse_sync(NULL, &layout));
    assert(!wena_board_collapse_sync(&state, NULL));
    test_board_wide_lists();
    puts("collapse tests passed");
    return 0;
}
