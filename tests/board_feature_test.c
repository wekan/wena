#include <stdio.h>
#include "../client/features/board.h"
#include "../client/components/boards/board_header.h"
#include "../client/components/lists/list_header.h"
#include "../client/components/cards/card_body.h"
#include "../client/components/common/wekan_look.h"

#include <assert.h>
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>

typedef struct TitleStore {
    WenaCard *card;
    unsigned long version;
    int saves;
    int fail;
} TitleStore;

static int load_title(void *data, const char *board, const char *id,
    char *title, size_t capacity, unsigned long *version)
{
    TitleStore *store;
    store = (TitleStore *)data;
    if (strcmp(board, store->card->board_id) != 0 ||
        strcmp(id, store->card->id) != 0 || store->fail) return 0;
    *version = store->version;
    return wena_model_set_required(title, capacity, store->card->title);
}

static int save_title(void *data, const char *board, const char *id,
    unsigned long version, const char *title)
{
    TitleStore *store;
    store = (TitleStore *)data;
    ++store->saves;
    if (strcmp(board, store->card->board_id) != 0 ||
        strcmp(id, store->card->id) != 0 || store->fail ||
        version != store->version) return 0;
    assert(wena_model_set_required(store->card->title,
                                  sizeof(store->card->title), title));
    ++store->version;
    return 1;
}

/* WeKan opens a card by clicking the minicard itself, which the layout hands
 * to its drag-area callback: this one clicks the card the test names. */
static void click_card(struct nk_context *context, void *user_data, const WenaCard *card,
                       size_t ordinal, const struct nk_rect *area, int *clicked)
{
    (void)user_data; (void)ordinal; (void)area;
    if (context->button_to_press != NULL && strcmp(context->button_to_press, card->title) == 0) {
        *clicked = 1;
        context->button_to_press = NULL;
    }
}

static int has_control(const char *name)
{
    size_t count, index;
    const WenaUiControl *controls = wena_ui_controls(&count);
    for (index = 0; index < count; ++index)
        if (strcmp(controls[index].name, name) == 0) return 1;
    return 0;
}

#define SIDEBAR_TOGGLE "Open Sidebar or Close Sidebar"

static void editor_frame(WenaCardDetailsState *state, WenaCard *card,
    const char *button, const char *input)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    context.edit_text = input;
    assert(wena_card_details_render(&context, state, card, 1, 800, 600));
}

static void test_title_editor(void)
{
    WenaCard card;
    WenaCardDetailsState state;
    TitleStore store;
    char too_long[400];
    char exact[WENA_CARD_DETAILS_TITLE_CAPACITY];
    struct nk_context context;
    assert(wena_card_init(&card, "one", "board", "lane", "doing",
                          "Initial", 1, 0));
    memset(&store, 0, sizeof(store));
    store.card = &card;
    store.version = 1ul;
    wena_card_details_init(&state);
    wena_card_details_set_title_adapter(&state, load_title, save_title, &store);
    assert(wena_card_details_open(&state, &card));
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(state.editing_title && state.title_version == 1ul);
    editor_frame(&state, &card, "Save", "Updated & + title");
    assert(!state.editing_title && store.saves == 1);
    assert(strcmp(card.title, "Updated & + title") == 0);
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    editor_frame(&state, &card, "Cancel", "Cancelled");
    assert(!state.editing_title && store.saves == 1);
    assert(strcmp(card.title, "Updated & + title") == 0);
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    editor_frame(&state, &card, "Save", "");
    assert(state.title_error && state.editing_title && store.saves == 1);
    memset(too_long, 'x', sizeof(too_long));
    too_long[sizeof(too_long) - 1] = '\0';
    editor_frame(&state, &card, "Save", too_long);
    assert(state.title_error && state.title_length ==
        WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY) - 1 &&
        store.saves == 1);
    editor_frame(&state, &card, "Save", "Bad\nTitle");
    assert(state.title_error && store.saves == 1);
    editor_frame(&state, &card, "Save", "Bad\177Title");
    assert(state.title_error && store.saves == 1);
    editor_frame(&state, &card, "Save", "Bad\302\205Title");
    assert(state.title_error && store.saves == 1);
    editor_frame(&state, &card, "Save", "Bad\300\200");
    assert(state.title_error && store.saves == 1);
    editor_frame(&state, &card, "Save", "Bad\355\240\200");
    assert(state.title_error && store.saves == 1);
    assert(!wena_card_details_title_valid("a\0b", 3));
    assert(!wena_card_details_title_valid(NULL, 1));
    ++store.version;
    editor_frame(&state, &card, "Save", "Stale");
    assert(state.title_error && state.editing_title && store.saves == 2);
    assert(strcmp(card.title, "Updated & + title") == 0);
    editor_frame(&state, &card, "Cancel", NULL);
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    memset(exact, 'x', sizeof(exact));
    exact[sizeof(exact) - 1] = '\0';
    editor_frame(&state, &card, "Save", exact);
    assert(!state.editing_title && strlen(card.title) == 128);
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    store.fail = 1;
    editor_frame(&state, &card, "Save", "Failed");
    assert(state.title_error && strlen(card.title) == 128);
    store.fail = 0;
    editor_frame(&state, &card, "Close details", "Unsaved");
    assert(!state.visible && !state.editing_title && state.title_input[0] == '\0');
    assert(wena_card_details_open(&state, &card));
    store.fail = 1;
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(!state.editing_title && state.title_error);
    store.fail = 0;
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    assert(state.editing_title);
    strcpy(card.board_id, "wrong");
    memset(&context, 0, sizeof(context));
    assert(!wena_card_details_render(&context, &state, &card, 1, 800, 600));
    assert(!state.visible && !state.editing_title);
    strcpy(card.board_id, "board");
    assert(wena_card_details_open(&state, &card));
    editor_frame(&state, &card, card.title, NULL); /* the title edits itself */
    strcpy(card.id, "wrong");
    assert(!wena_card_details_render(&context, &state, &card, 1, 800, 600));
    assert(!state.visible);
}

int main(void)
{
    WenaBoard board;
    WenaSwimlane swimlanes[2];
    WenaList lists[2];
    WenaCard cards[3];
    WenaBoardLayout layout;
    WenaBoardSidebar sidebar;
    WenaListInteraction list_interaction;
    WenaCardInteraction card_interaction;
    WenaCardDetailsState card_details;
    const char *activities[1];
    const char *members[2];
    const char *labels[1];
    const char *archives[1];
    struct nk_context context;

    test_title_editor();
    memset(&context, 0, sizeof(context));
    assert(wena_board_init(&board, "board", "Project", 0));
    assert(wena_swimlane_init(&swimlanes[0], "lane", "board", "Current", 1.0, 0));
    assert(wena_swimlane_init(&swimlanes[1], "hidden", "board", "Hidden", 2.0, 1));
    assert(wena_list_init(&lists[0], "doing", "board", "lane", "Doing", 1.0, 0));
    assert(wena_list_init(&lists[1], "old", "board", "lane", "Old", 2.0, 1));
    assert(wena_card_init(&cards[0], "one", "board", "lane", "doing", "One", 1.0, 0));
    assert(wena_card_init(&cards[1], "two", "board", "lane", "doing", "Two", 2.0, 0));
    assert(wena_card_init(&cards[2], "old", "board", "lane", "doing", "Old", 3.0, 1));

    memset(&layout, 0, sizeof(layout));
    layout.board = &board;
    layout.swimlanes = swimlanes;
    layout.swimlane_count = 2;
    layout.lists = lists;
    layout.list_count = 2;
    layout.cards = cards;
    layout.card_count = 3;

    wena_ui_controls_begin();
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(context.begin_count == 1);
    assert(context.end_count == 1);
    assert(context.group_depth == 0);
    /* WeKan's controls: the header's title, Filter and sidebar toggle; the
     * lane's caret; the list's caret, Add Card to Top of List, Add List and
     * List Actions; each card's Card Actions; and + Add Card. */
    assert(context.button_count == 11);
    assert(context.label_count == 4);
    assert(strcmp(context.labels[0], "Current") == 0);
    assert(strcmp(context.labels[1], "Doing") == 0);
    assert(strcmp(context.labels[2], "One") == 0);
    assert(strcmp(context.labels[3], "Two") == 0);
    assert(has_control("Project") && has_control("Filter") && has_control(SIDEBAR_TOGGLE));
    assert(has_control("Add Card to Top of List") && has_control("Add List") &&
           has_control("List Actions") && has_control("Card Actions") && has_control("Add Card"));
    /* Negative: the archived lane, list and card draw nothing. */
    assert(!has_control("Hidden") && !has_control("Old"));
    assert(!wena_board_feature_render(&context, &layout, 0.0f, 600.0f));
    board.archived = 1;
    assert(!wena_board_layout_render(&context, &layout));
    board.archived = 0;
    layout.cards = NULL;
    assert(!wena_board_layout_render(&context, &layout));
    layout.cards = cards;
    context.button_to_press = SIDEBAR_TOGGLE;
    assert(wena_board_header_render(&context, &board) ==
           WENA_BOARD_HEADER_OPEN_MENU);
    context.button_to_press = "Project";
    assert(wena_board_header_render(&context, &board) == WENA_BOARD_HEADER_RENAME);
    context.button_to_press = "Filter";
    assert(wena_board_header_render(&context, &board) == WENA_BOARD_HEADER_FILTER);
    assert(wena_board_header_render(NULL, &board) ==
           WENA_BOARD_HEADER_NO_ACTION);

    memset(&context, 0, sizeof(context));
    memset(&list_interaction, 0, sizeof(list_interaction));
    layout.list_interaction = &list_interaction;
    context.button_to_press = "Add Card to Top of List";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(list_interaction.actions == WENA_LIST_HEADER_ADD_CARD);
    assert(strcmp(list_interaction.list_id, "doing") == 0);
    /* "+ Add Card" under the cards asks for the composer at the bottom. */
    context.button_to_press = "Add Card";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(list_interaction.actions == (WENA_LIST_HEADER_ADD_CARD | WENA_LIST_HEADER_ADD_CARD_BOTTOM));
    context.button_to_press = "Add List";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(list_interaction.actions == WENA_LIST_HEADER_ADD_LIST);
    context.button_to_press = "List Actions";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(list_interaction.actions == WENA_LIST_HEADER_OPEN_MENU);
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(list_interaction.actions == WENA_LIST_HEADER_NO_ACTION);
    assert(list_interaction.list_id[0] == '\0');
    assert(wena_list_header_render(NULL, &lists[0], 0) ==
           WENA_LIST_HEADER_NO_ACTION);
    lists[0].archived = 1;
    assert(wena_list_header_render(&context, &lists[0], 0) ==
           WENA_LIST_HEADER_NO_ACTION);
    lists[0].archived = 0;
    layout.list_interaction = NULL;

    memset(&context, 0, sizeof(context));
    memset(&card_interaction, 0, sizeof(card_interaction));
    wena_card_details_init(&card_details);
    layout.card_interaction = &card_interaction;
    layout.card_drag_area = click_card;
    context.button_to_press = "One";
    assert(wena_board_feature_render_with_state(&context, &layout, 800.0f,
                                                600.0f, &card_details));
    assert(card_interaction.actions == WENA_CARD_BODY_OPEN_DETAILS);
    assert(strcmp(card_interaction.card_id, "one") == 0);
    assert(card_details.visible);
    assert(strcmp(card_details.card_id, "one") == 0);
    /* WeKan's details: the title itself edits it (the minicard's title opens
     * the details, so the details are drawn on their own here). */
    context.button_to_press = "One";
    assert(wena_card_details_render(&context, &card_details, cards, 3, 800.0f, 600.0f));
    assert(card_details.interaction.actions == WENA_CARD_DETAILS_EDIT_TITLE);
    assert(strcmp(card_details.interaction.card_id, "one") == 0);
    assert(has_control("Collapse") && has_control("Card Actions") && has_control("Maximize Card") &&
           has_control("Close Card") && has_control("Labels") && has_control("Description") &&
           has_control("Checklists"));
    /* Negative: the old button column is gone. */
    assert(!has_control("Edit title") && !has_control("Archive card") && !has_control("Move card to"));
    assert(wena_board_feature_render_with_state(&context, &layout, 800.0f,
                                                600.0f, &card_details));
    assert(card_details.interaction.actions == WENA_CARD_DETAILS_NO_ACTION);
    assert(card_details.visible);
    /* Card Actions in the details' header asks for WeKan's popup. */
    context.button_to_press = "Card Actions";
    assert(wena_card_details_render(&context, &card_details, cards, 3, 800.0f, 600.0f));
    assert(card_details.interaction.actions == WENA_CARD_DETAILS_OPEN_MENU);
    assert(strcmp(card_details.interaction.card_id, "one") == 0);
    /* Maximize, then Collapse: the window is the board's width, and only
     * the header is left. */
    context.button_to_press = "Maximize Card";
    assert(wena_card_details_render(&context, &card_details, cards, 3, 800.0f, 600.0f));
    assert(card_details.view.maximized && card_details.visible);
    wena_ui_controls_begin();
    context.button_to_press = "Collapse";
    assert(wena_card_details_render(&context, &card_details, cards, 3, 800.0f, 600.0f));
    assert(card_details.view.collapsed);
    wena_ui_controls_begin();
    assert(wena_card_details_render(&context, &card_details, cards, 3, 800.0f, 600.0f));
    assert(has_control("Minimize Card") && !has_control("Labels"));
    card_details.view.maximized = 0;
    wena_card_details_close(&card_details);
    /* The minicard's Card Actions asks for the popup, and does not open
     * the details (WeKan's minicard hamburger). */
    context.button_to_press = "Card Actions";
    assert(wena_board_feature_render_with_state(&context, &layout, 800.0f,
                                                600.0f, &card_details));
    assert(card_interaction.actions == WENA_CARD_BODY_OPEN_MENU);
    assert(strcmp(card_interaction.card_id, "one") == 0);
    assert(!card_details.visible);
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(card_interaction.actions == WENA_CARD_BODY_NO_ACTION);
    assert(card_interaction.card_id[0] == '\0');
    /* Opening the details again shows their header uncollapsed. */
    assert(wena_card_details_open(&card_details, &cards[0]) && !card_details.view.collapsed);
    context.button_to_press = "Close Card";
    assert(wena_card_details_render(&context, &card_details, cards, 3,
                                    800.0f, 600.0f));
    assert(!card_details.visible && card_details.card_id[0] == '\0');
    assert(card_details.interaction.actions == WENA_CARD_DETAILS_CLOSE);
    assert(strcmp(card_details.interaction.card_id, "one") == 0);
    assert(!wena_card_details_open(NULL, &cards[0]));
    assert(wena_card_details_canvas_render(NULL, &cards[0], &card_details.view) ==
           WENA_CARD_DETAILS_NO_ACTION);
    cards[0].archived = 1;
    assert(wena_card_body_render(&context, &cards[0]) ==
           WENA_CARD_BODY_NO_ACTION);
    assert(!wena_card_details_open(&card_details, &cards[0]));
    cards[0].archived = 0;
    assert(wena_card_details_open(&card_details, &cards[0]));
    cards[0].archived = 1;
    assert(!wena_card_details_render(&context, &card_details, cards, 3,
                                     800.0f, 600.0f));
    assert(!card_details.visible);
    cards[0].archived = 0;
    assert(wena_card_details_open(&card_details, &cards[0]));
    assert(!wena_card_details_render(&context, &card_details, NULL, 1,
                                     800.0f, 600.0f));
    wena_card_details_close(&card_details);
    layout.card_interaction = NULL;
    layout.card_drag_area = NULL;

    memset(&context, 0, sizeof(context));
    wena_board_sidebar_init(&sidebar);
    activities[0] = "Card moved";
    members[0] = "Ada";
    members[1] = "Linus";
    labels[0] = "Urgent";
    archives[0] = "Old card";
    sidebar.items.activities = activities;
    sidebar.items.activity_count = 1;
    sidebar.items.members = members;
    sidebar.items.member_count = 2;
    sidebar.items.labels = labels;
    sidebar.items.label_count = 1;
    sidebar.items.archives = archives;
    sidebar.items.archive_count = 1;
    layout.sidebar = &sidebar;
    context.button_to_press = SIDEBAR_TOGGLE;
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(sidebar.visible);
    assert(sidebar.section == WENA_SIDEBAR_ACTIVITIES);
    /* WeKan's sidebar home: Board Settings, then foldable Members, Labels
     * and Activities, and the Archive. */
    wena_ui_controls_begin();
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(has_control("Board Settings") && has_control("Members") && has_control("Labels") &&
           has_control("Activities") && has_control("Ada") && has_control("Close"));
    context.button_to_press = "Labels";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(sidebar.folded[WENA_SIDEBAR_FOLD_LABELS] && sidebar.section == WENA_SIDEBAR_ACTIVITIES);
    context.button_to_press = "Labels";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(!sidebar.folded[WENA_SIDEBAR_FOLD_LABELS]);
    /* Labels' "+" opens the board's labels. */
    context.button_to_press = "Add label";
    assert((wena_board_sidebar_render(&context, &sidebar) &
            (WENA_SIDEBAR_ADD_LABEL | WENA_SIDEBAR_SECTION_CHANGED)) ==
           (WENA_SIDEBAR_ADD_LABEL | WENA_SIDEBAR_SECTION_CHANGED));
    assert(sidebar.section == WENA_SIDEBAR_LABELS);
    context.button_to_press = "Board Settings";
    assert(wena_board_sidebar_render(&context, &sidebar) & WENA_SIDEBAR_SECTION_CHANGED);
    assert(sidebar.section == WENA_SIDEBAR_SETTINGS);
    /* Negative: a folded section draws neither its items nor its "+". */
    sidebar.folded[WENA_SIDEBAR_FOLD_MEMBERS] = 1;
    wena_ui_controls_begin();
    context.button_to_press = "Add member";
    assert(!(wena_board_sidebar_render(&context, &sidebar) & WENA_SIDEBAR_ADD_MEMBER));
    assert(!has_control("Ada") && !has_control("Add member"));
    sidebar.folded[WENA_SIDEBAR_FOLD_MEMBERS] = 0;
    sidebar.section = WENA_SIDEBAR_ACTIVITIES;
    context.button_to_press = "Refresh";
    assert((wena_board_sidebar_render(&context, &sidebar) &
            WENA_SIDEBAR_REFRESH_ACTIVITIES) != 0u);
    sidebar.section = WENA_SIDEBAR_MEMBERS;
    context.button_to_press = "Add member";
    assert((wena_board_sidebar_render(&context, &sidebar) &
            WENA_SIDEBAR_ADD_MEMBER) != 0u);
    sidebar.section = WENA_SIDEBAR_ARCHIVES;
    context.button_to_press = "Restore selected";
    assert((wena_board_sidebar_render(&context, &sidebar) &
            WENA_SIDEBAR_RESTORE_ARCHIVE) != 0u);
    {
        const char *many[21]; size_t item;
        for (item = 0; item < 21; ++item) many[item] = "Older activity";
        sidebar.items.activities = many; sidebar.items.activity_count = 21;
        sidebar.section = WENA_SIDEBAR_ACTIVITIES;
        memset(&context, 0, sizeof(context)); context.button_to_press = "Next Page";
        assert(wena_board_sidebar_render(&context, &sidebar) == WENA_SIDEBAR_NO_ACTION);
        assert(sidebar.table.page == 1);
        memset(&context, 0, sizeof(context)); context.button_to_press = "Next Page";
        assert(wena_board_sidebar_render(&context, &sidebar) == WENA_SIDEBAR_NO_ACTION);
        assert(sidebar.table.page == 2);
        memset(&context, 0, sizeof(context)); context.button_to_press = "Archives";
        assert(wena_board_sidebar_render(&context, &sidebar) & WENA_SIDEBAR_SECTION_CHANGED);
        assert(sidebar.table.page == 0 && sidebar.section == WENA_SIDEBAR_ARCHIVES);
        sidebar.items.activities = activities; sidebar.items.activity_count = 1;
    }
    context.button_to_press = "Close";
    assert(wena_board_feature_render(&context, &layout, 800.0f, 600.0f));
    assert(!sidebar.visible);
    assert(wena_board_sidebar_render(NULL, &sidebar) ==
           WENA_SIDEBAR_NO_ACTION);
    sidebar.visible = 1;
    sidebar.items.labels = NULL;
    assert(wena_board_sidebar_render(&context, &sidebar) ==
           WENA_SIDEBAR_INVALID_STATE);
    sidebar.items.label_count = 0;
    sidebar.section = (WenaSidebarSection)99;
    assert((wena_board_sidebar_render(&context, &sidebar) &
            WENA_SIDEBAR_INVALID_STATE) != 0u);
    return 0;
}
