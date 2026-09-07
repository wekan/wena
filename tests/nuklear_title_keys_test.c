#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_details.h"
#include "../client/features/card_create.h"
#include "../client/features/hierarchy_title.h"
#include "../client/components/lists/list_header.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Fixture {
    struct nk_context context;
    struct nk_user_font font;
    WenaBoard board;
    WenaSwimlane lane;
    WenaList list;
    WenaCard card;
    WenaBoardLayout layout;
    WenaCardDetailsState details;
    WenaCardCreateState create;
    WenaHierarchyTitleState hierarchy;
    char stored[129];
    unsigned long version;
    int attempts;
    int writes;
    int readonly;
    int mode;
    int other_window;
    int dropdown_open;
} Fixture;

static float text_width(nk_handle handle, float height, const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static int load(void *context, const char *board, const char *target,
                 char *title, size_t capacity, unsigned long *version)
{
    Fixture *f;
    f = (Fixture *)context;
    assert(strcmp(board, "board") == 0 && target && target[0]);
    assert(strlen(f->stored) < capacity);
    strcpy(title, f->stored); *version = f->version;
    return 1;
}

static int save(void *context, const char *board, const char *target,
                 unsigned long version, const char *title)
{
    Fixture *f;
    f = (Fixture *)context;
    assert(strcmp(board, "board") == 0 && target && target[0]);
    ++f->attempts;
    if (f->readonly || version != f->version) return 0;
    assert(wena_model_title_string_valid(title, sizeof(f->stored)));
    strcpy(f->stored, title); ++f->version; ++f->writes;
    return 1;
}

static int create_card(void *context, const char *board, const char *list,
                        const char *lane, const char *title)
{
    Fixture *f;
    f = (Fixture *)context;
    assert(strcmp(list, "list") == 0 && strcmp(lane, "lane") == 0);
    return save(context, board, list, f->version, title);
}

static int load_hierarchy(void *context, const char *board, WenaHierarchyKind kind,
                           const char *target, char *title, size_t capacity,
                           unsigned long *version)
{
    assert(kind == WENA_HIERARCHY_LIST);
    return load(context, board, target, title, capacity, version);
}

static int save_hierarchy(void *context, const char *board, WenaHierarchyKind kind,
                           const char *target, unsigned long version, const char *title)
{
    assert(kind == WENA_HIERARCHY_LIST);
    return save(context, board, target, version, title);
}

static int create_hierarchy(void *context, const char *board,
                             WenaHierarchyKind kind, const char *title)
{
    Fixture *f;
    f = (Fixture *)context;
    assert(kind == WENA_HIERARCHY_LIST);
    return save(context, board, "new-list", f->version, title);
}

static int editing(const Fixture *f)
{
    if (f->mode == 0) return f->details.editing_title;
    if (f->mode == 1) return f->create.visible;
    return f->hierarchy.visible;
}

static int error(const Fixture *f)
{
    if (f->mode == 0) return f->details.title_error;
    if (f->mode == 1) return f->create.error;
    return f->hierarchy.error;
}

static void render(Fixture *f)
{
    if (f->mode == 0 && f->details.visible)
        assert(wena_card_details_render(&f->context, &f->details, &f->card, 1, 640, 480));
    else if (f->mode == 1 && f->create.visible)
        assert(wena_card_create_render(&f->context, &f->create, &f->layout, 640, 480));
    else if (f->mode >= 2 && f->hierarchy.visible)
        assert(wena_hierarchy_title_render(&f->context, &f->hierarchy, &f->layout, 640, 480));
    if (f->other_window) {
        if (nk_begin(&f->context, "Unrelated", nk_rect(0, 0, 120, 90), NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(&f->context, 26, 1);
            f->dropdown_open = nk_combo_begin_label(&f->context, "Choice", nk_vec2(120, 90));
            if (f->dropdown_open) {
                nk_layout_row_dynamic(&f->context, 24, 1);
                (void)nk_combo_item_label(&f->context, "Item", NK_TEXT_LEFT);
                nk_combo_end(&f->context);
            }
        }
        nk_end(&f->context);
    }
    assert(f->context.current == NULL);
}

static void frame(Fixture *f)
{
    nk_clear(&f->context); nk_input_begin(&f->context); nk_input_end(&f->context);
    render(f);
}

static void key(Fixture *f, enum nk_keys value, int down)
{
    nk_clear(&f->context); nk_input_begin(&f->context);
    nk_input_key(&f->context, value, down); nk_input_end(&f->context);
    render(f);
}

static void character(Fixture *f, nk_rune value)
{
    nk_clear(&f->context); nk_input_begin(&f->context);
    nk_input_unicode(&f->context, value); nk_input_end(&f->context); render(f);
}

static struct nk_vec2 label_center(Fixture *f, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, &f->context) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                            (float)text->y + (float)text->h * 0.5f);
    }
    assert(0);
    return nk_vec2(0, 0);
}

static void click(Fixture *f, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(&f->context); nk_input_begin(&f->context);
        nk_input_motion(&f->context, (int)point.x, (int)point.y);
        nk_input_button(&f->context, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(&f->context); render(f);
    }
}

static void focus_field(Fixture *f)
{
    click(f, f->mode == 0 ? nk_vec2(420, 20) :
        f->mode == 1 ? nk_vec2(420, 55) : nk_vec2(200, 145));
}

static void initialize(Fixture *f, int mode)
{
    WenaListInteraction intent;
    memset(f, 0, sizeof(*f));
    f->font.height = 14; f->font.width = text_width;
    assert(nk_init_default(&f->context, &f->font));
    f->mode = mode; f->version = 1; strcpy(f->stored, "Original");
    assert(wena_board_init(&f->board, "board", "Board", 0));
    assert(wena_swimlane_init(&f->lane, "lane", "board", "Lane", 0, 0));
    assert(wena_list_init(&f->list, "list", "board", "", "List", 0, 0));
    assert(wena_card_init(&f->card, "card", "board", "lane", "list", "Original", 0, 0));
    f->layout.board = &f->board; f->layout.swimlanes = &f->lane;
    f->layout.swimlane_count = 1; f->layout.lists = &f->list; f->layout.list_count = 1;
    f->layout.cards = &f->card; f->layout.card_count = 1;
    if (mode == 0) {
        wena_card_details_init(&f->details);
        wena_card_details_set_title_adapter(&f->details, load, save, f);
        assert(wena_card_details_open(&f->details, &f->card));
        frame(f); click(f, label_center(f, "Edit title")); frame(f);
        assert(f->details.editing_title);
    } else if (mode == 1) {
        wena_card_create_init(&f->create, create_card, f);
        memset(&intent, 0, sizeof(intent)); intent.actions = WENA_LIST_HEADER_ADD_CARD;
        strcpy(intent.board_id, "board"); strcpy(intent.list_id, "list");
        strcpy(intent.swimlane_id, "lane");
        assert(wena_card_create_open(&f->create, &f->layout, &intent));
        frame(f);
    } else {
        wena_hierarchy_title_init(&f->hierarchy);
        wena_hierarchy_title_set_adapter(&f->hierarchy, load_hierarchy, save_hierarchy, f);
        wena_hierarchy_title_set_create_adapter(&f->hierarchy, create_hierarchy);
        if (mode == 2)
            assert(wena_hierarchy_title_open(&f->hierarchy, &f->layout, WENA_HIERARCHY_LIST, "list"));
        else assert(wena_hierarchy_title_open_create(&f->hierarchy, &f->layout, WENA_HIERARCHY_LIST));
        frame(f);
    }
}

int main(void)
{
    Fixture f;
    int mode;
    int count;
    for (mode = 0; mode < 4; ++mode) {
        /* Enter outside the edit field cannot submit an unfocused form. */
        initialize(&f, mode);
        key(&f, NK_KEY_ENTER, 1); key(&f, NK_KEY_ENTER, 0);
        assert(f.writes == 0 && f.attempts == 0 && editing(&f));
        focus_field(&f); character(&f, (nk_rune)'X');
        key(&f, NK_KEY_ENTER, 1);
        assert(f.writes == 1 && f.attempts == 1 && !editing(&f));
        assert(strchr(f.stored, 'X') != NULL);
        for (count = 0; count < 4; ++count) frame(&f);
        assert(f.writes == 1 && f.attempts == 1);
        key(&f, NK_KEY_ENTER, 0);
        assert(f.writes == 1 && f.attempts == 1);
        nk_free(&f.context);

        initialize(&f, mode); focus_field(&f);
        key(&f, NK_KEY_TEXT_SELECT_ALL, 1); key(&f, NK_KEY_TEXT_SELECT_ALL, 0);
        key(&f, NK_KEY_BACKSPACE, 1); key(&f, NK_KEY_BACKSPACE, 0);
        key(&f, NK_KEY_ENTER, 1);
        assert(editing(&f) && error(&f) && f.attempts == 0 && f.writes == 0);
        for (count = 0; count < 4; ++count) frame(&f);
        assert(editing(&f) && error(&f) && f.attempts == 0);
        key(&f, NK_KEY_ENTER, 0);
        key(&f, NK_KEY_TEXT_RESET_MODE, 1);
        assert(!editing(&f) && f.writes == 0 && strcmp(f.stored, "Original") == 0);
        nk_free(&f.context);

        initialize(&f, mode); focus_field(&f);
        for (count = 0; count < 150; ++count) character(&f, (nk_rune)'a');
        key(&f, NK_KEY_ENTER, 1);
        assert(editing(&f) && error(&f) && f.attempts == 0 && f.writes == 0);
        key(&f, NK_KEY_ENTER, 0); key(&f, NK_KEY_TEXT_RESET_MODE, 1);
        assert(!editing(&f) && f.writes == 0);
        nk_free(&f.context);

        /* Escape wins even if Enter arrives in the same input batch. */
        initialize(&f, mode); focus_field(&f); character(&f, (nk_rune)'X');
        nk_clear(&f.context); nk_input_begin(&f.context);
        nk_input_key(&f.context, NK_KEY_ENTER, 1);
        nk_input_key(&f.context, NK_KEY_TEXT_RESET_MODE, 1);
        nk_input_end(&f.context); render(&f);
        assert(!editing(&f) && f.attempts == 0 && f.writes == 0);
        assert(strcmp(f.stored, "Original") == 0);
        nk_free(&f.context);

        /* A rejected/readonly adapter is attempted once, not every held frame. */
        initialize(&f, mode); f.readonly = 1;
        focus_field(&f); character(&f, (nk_rune)'X');
        key(&f, NK_KEY_ENTER, 1);
        assert(editing(&f) && error(&f) && f.attempts == 1 && f.writes == 0);
        for (count = 0; count < 4; ++count) frame(&f);
        assert(f.attempts == 1 && f.writes == 0);
        key(&f, NK_KEY_ENTER, 0); key(&f, NK_KEY_TEXT_RESET_MODE, 1);
        assert(!editing(&f) && strcmp(f.stored, "Original") == 0);
        nk_free(&f.context);

        /* A separate active dropdown does not route keys to this editor. */
        initialize(&f, mode); focus_field(&f); character(&f, (nk_rune)'X');
        f.other_window = 1; frame(&f); click(&f, label_center(&f, "Choice"));
        assert(f.dropdown_open);
        key(&f, NK_KEY_ENTER, 1); key(&f, NK_KEY_ENTER, 0);
        key(&f, NK_KEY_TEXT_RESET_MODE, 1);
        assert(editing(&f) && f.attempts == 0 && f.writes == 0);
        nk_free(&f.context);
    }
    /* A details view with no mutation adapter keeps Enter inert. */
    initialize(&f, 0);
    wena_card_details_set_title_adapter(&f.details, NULL, NULL, NULL);
    assert(wena_card_details_open(&f.details, &f.card));
    frame(&f); key(&f, NK_KEY_ENTER, 1); key(&f, NK_KEY_ENTER, 0);
    assert(f.details.visible && f.writes == 0 && f.attempts == 0);
    key(&f, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!f.details.visible && f.writes == 0);
    nk_free(&f.context);
    puts("real Nuklear focused Enter/Escape title keyboard tests passed");
    return 0;
}
