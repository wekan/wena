#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/hierarchy_title.h"
#include "../client/features/hierarchy_mutation.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void execute(sqlite3 *database, const char *sql)
{
    assert(sqlite3_exec(database, sql, NULL, NULL, NULL) == SQLITE_OK);
}

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *context, WenaHierarchyTitleState *state,
                    WenaBoardLayout *layout)
{
    if (state->visible)
        assert(wena_hierarchy_title_render(context, state, layout, 640.0f, 480.0f));
    assert(context->current == NULL);
}

static void frame(struct nk_context *context, WenaHierarchyTitleState *state,
                   WenaBoardLayout *layout)
{
    nk_clear(context); nk_input_begin(context); nk_input_end(context);
    render(context, state, layout);
}

static struct nk_vec2 label_center(struct nk_context *context, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, context) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                            (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing control %s\n", label);
    assert(0);
    return nk_vec2(0.0f, 0.0f);
}

static void click_at(struct nk_context *context, WenaHierarchyTitleState *state,
                      WenaBoardLayout *layout, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(context); nk_input_begin(context);
        nk_input_motion(context, (int)point.x, (int)point.y);
        nk_input_button(context, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(context);
        render(context, state, layout);
    }
}

static void click(struct nk_context *context, WenaHierarchyTitleState *state,
                   WenaBoardLayout *layout, const char *label)
{
    click_at(context, state, layout, label_center(context, label));
}

static int scalar(sqlite3 *database, const char *sql)
{
    sqlite3_stmt *statement;
    int value;
    assert(sqlite3_prepare_v2(database, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    value = sqlite3_column_int(statement, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return value;
}

static void creation_tests(sqlite3 *database, WenaHierarchyMutation *adapter,
    WenaSqliteBoardSnapshot *snapshot, struct nk_context *context,
    WenaHierarchyTitleState *state, WenaBoardLayout *layout)
{
    size_t count;
    int keys;
    int index;
    char full_title[129];
    char title[129];
    unsigned long version;
    WenaHierarchyKind kind;
    const char *created_id;
    const char *count_sql;

    assert(!wena_hierarchy_mutation_create(adapter, "board", WENA_HIERARCHY_BOARD, "No board creation"));
    assert(!wena_hierarchy_mutation_create(adapter, "other", WENA_HIERARCHY_LIST, "Wrong scope"));
    for (index = 0; index < 2; ++index) {
        kind = index == 0 ? WENA_HIERARCHY_LIST : WENA_HIERARCHY_SWIMLANE;
        count_sql = index == 0 ? "SELECT count(*) FROM lists" : "SELECT count(*) FROM swimlanes";
        count = index == 0 ? snapshot->list_count : snapshot->swimlane_count;
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, ""));
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "Bad\n"));
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "\300\257"));
        assert(!wena_hierarchy_mutation_create_request(adapter, "board", kind, 0, "Invalid request"));
        assert(wena_hierarchy_mutation_create_request(adapter, "board", kind, 44, "Created & + % = \303\244"));
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 1);
        created_id = index == 0 ? snapshot->lists[count].id : snapshot->swimlanes[count].id;
        assert(created_id[0] && strcmp(created_id, "pending") != 0);
        assert(index == 0 ? snapshot->lists[count].sort == 1.0 : snapshot->swimlanes[count].sort == 1.0);
        if (index == 0) assert(snapshot->lists[count].swimlane_id[0] == '\0');
        assert(wena_hierarchy_mutation_load(adapter, "board", kind, created_id, title, sizeof(title), &version));
        assert(version == 1 && strcmp(title, "Created & + % = \303\244") == 0);
        assert(!wena_hierarchy_mutation_create_request(adapter, "board", kind, 44, "Replay"));
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 1);
        keys = scalar(database, "SELECT count(*) FROM idempotency_keys");
        execute(database, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'injected'); END;");
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "Rollback"));
        execute(database, "DROP TRIGGER reject_metadata;");
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 1);
        assert(scalar(database, count_sql) == (int)count + 2);
        assert(scalar(database, "SELECT count(*) FROM idempotency_keys") == keys);
        if (index == 0) snapshot->list_count = WENA_SQLITE_BOARD_MAX_LISTS;
        else snapshot->swimlane_count = WENA_SQLITE_BOARD_MAX_SWIMLANES;
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "Capacity"));
        assert(scalar(database, count_sql) == (int)count + 2);
        if (index == 0) snapshot->list_count = count + 1;
        else snapshot->swimlane_count = count + 1;
        memset(full_title, 'x', sizeof(full_title) - 1); full_title[128] = '\0';
        assert(wena_hierarchy_mutation_create(adapter, "board", kind, full_title));
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 2);
    }
    layout->list_count = snapshot->list_count;
    layout->swimlane_count = snapshot->swimlane_count;
    wena_hierarchy_title_set_create_adapter(state, wena_hierarchy_mutation_create);
    assert(!wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_BOARD));
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_LIST));
    assert(state->creating && state->visible && state->title_length == 0);
    count = snapshot->list_count;
    frame(context, state, layout);
    click(context, state, layout, "Save");
    assert(state->visible && state->error && snapshot->list_count == count);
    click(context, state, layout, "Cancel");
    assert(!state->visible && snapshot->list_count == count);
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_LIST));
    frame(context, state, layout);
    click_at(context, state, layout, nk_vec2(200.0f, 145.0f));
    nk_clear(context); nk_input_begin(context);
    nk_input_unicode(context, (nk_rune)'N'); nk_input_end(context);
    render(context, state, layout);
    assert(state->title_length == 1);
    click(context, state, layout, "Save");
    assert(!state->visible && snapshot->list_count == count + 1);
    assert(strcmp(snapshot->lists[count].title, "N") == 0);
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_SWIMLANE));
    count = snapshot->swimlane_count;
    frame(context, state, layout);
    click_at(context, state, layout, nk_vec2(200.0f, 145.0f));
    for (index = 0; index < 160; ++index) {
        nk_clear(context); nk_input_begin(context);
        nk_input_unicode(context, (nk_rune)'x'); nk_input_end(context);
        render(context, state, layout);
    }
    assert(state->title_length == WENA_CARD_DETAILS_TITLE_CAPACITY);
    click(context, state, layout, "Save");
    assert(state->visible && state->error && snapshot->swimlane_count == count);
    click(context, state, layout, "Cancel");
    assert(!state->visible && snapshot->swimlane_count == count);
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_LIST));
    strcpy(snapshot->board.id, "other");
    assert(!wena_hierarchy_title_render(context, state, layout, 640.0f, 480.0f));
    assert(!state->visible);
    strcpy(snapshot->board.id, "board");
}

int main(int argc, char **argv)
{
    unsigned char migration[8192];
    size_t length;
    char hash[65];
    char path[4096];
    FILE *file;
    sqlite3 *database;
    WenaSqliteBoardSnapshot *snapshot;
    WenaHierarchyMutation adapter;
    WenaHierarchyTitleState state;
    WenaBoardLayout layout;
    WenaHierarchyKind kinds[3];
    const char *ids[3];
    const char *bad_titles[6];
    char title[129];
    char before[129];
    char long_title[130];
    unsigned long version;
    size_t index;
    size_t bad;
    struct nk_context context;
    struct nk_user_font font;
    int initial_length;
    int count;

    assert(argc == 3 && strlen(argv[2]) < 4000);
    file = fopen(argv[1], "rb"); assert(file);
    length = fread(migration, 1, sizeof(migration), file);
    assert(length > 0 && length < sizeof(migration));
    assert(fclose(file) == 0);
    wena_sha256_hex(migration, length, hash);
    sprintf(path, "%s/hierarchy.sqlite", argv[2]);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    execute(database, "INSERT INTO actors VALUES('actor','User',1);"
        "INSERT INTO boards VALUES('board','Board',1);"
        "INSERT INTO boards VALUES('other','Other',1);"
        "INSERT INTO swimlanes VALUES('lane','board','Lane',0,1);"
        "INSERT INTO swimlanes VALUES('foreign-lane','other','Foreign lane',0,1);"
        "INSERT INTO lists VALUES('list','board','List',0,1);"
        "INSERT INTO lists VALUES('foreign-list','other','Foreign list',0,1);");
    snapshot = (WenaSqliteBoardSnapshot *)malloc(sizeof(*snapshot));
    assert(snapshot && wena_sqlite_board_load(database, "board", snapshot));
    assert(!wena_hierarchy_mutation_init(&adapter, database, "actor", "other", snapshot));
    assert(!wena_hierarchy_mutation_init(&adapter, database, "bad/actor", "board", snapshot));
    assert(wena_hierarchy_mutation_init(&adapter, database, "actor", "board", snapshot));
    kinds[0] = WENA_HIERARCHY_BOARD; kinds[1] = WENA_HIERARCHY_LIST;
    kinds[2] = WENA_HIERARCHY_SWIMLANE;
    ids[0] = "board"; ids[1] = "list"; ids[2] = "lane";
    bad_titles[0] = ""; bad_titles[1] = "Bad\n"; bad_titles[2] = "\177";
    bad_titles[3] = "\300\257"; bad_titles[4] = "\302\205";
    bad_titles[5] = "\355\240\200";
    memset(long_title, 'x', sizeof(long_title)); long_title[129] = '\0';
    for (index = 0; index < 3; ++index) {
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index],
            title, sizeof(title), &version) && version == 1);
        for (bad = 0; bad < 6; ++bad)
            assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 1, bad_titles[bad]));
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 1, long_title));
        assert(!wena_hierarchy_mutation_save(&adapter, "other", kinds[index], ids[index], 1, "Wrong"));
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], "missing", 1, "Wrong"));
        assert(!wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 0, 1, "Wrong"));
        assert(!wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 1, ULONG_MAX, "Wrong"));
        assert(!wena_hierarchy_mutation_load(&adapter, "other", kinds[index], ids[index], title, sizeof(title), &version));
        assert(!wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, 2, &version));
        assert(wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 1, 40, "A&B + 100% = \303\244"));
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, sizeof(title), &version));
        assert(version == 2 && strcmp(title, "A&B + 100% = \303\244") == 0);
        assert(!wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 2, 40, "Replay"));
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 1, "Stale"));
        execute(database, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'injected'); END;");
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 2, "Rollback"));
        execute(database, "DROP TRIGGER reject_metadata;");
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, sizeof(title), &version));
        assert(version == 2 && strcmp(title, "A&B + 100% = \303\244") == 0);
    }
    assert(strcmp(snapshot->board.title, title) == 0);
    assert(strcmp(snapshot->lists[0].title, title) == 0);
    assert(strcmp(snapshot->swimlanes[0].title, title) == 0);
    assert(!wena_hierarchy_mutation_save(&adapter, "board", WENA_HIERARCHY_LIST, "foreign-list", 1, "Wrong"));
    assert(!wena_hierarchy_mutation_save(&adapter, "board", WENA_HIERARCHY_SWIMLANE, "foreign-lane", 1, "Wrong"));
    assert(!wena_hierarchy_mutation_save(&adapter, "board", (WenaHierarchyKind)99, "list", 2, "Wrong"));
    assert(wena_hierarchy_mutation_init(&adapter, database, "unknown", "board", snapshot));
    assert(!wena_hierarchy_mutation_load(&adapter, "board", WENA_HIERARCHY_LIST, "list", title, sizeof(title), &version));
    assert(!wena_hierarchy_mutation_save(&adapter, "board", WENA_HIERARCHY_LIST, "list", 2, "Forbidden"));
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    assert(wena_sqlite_integrity(database));
    assert(wena_sqlite_board_load(database, "board", snapshot));
    assert(wena_hierarchy_mutation_init(&adapter, database, "actor", "board", snapshot));
    for (index = 0; index < 3; ++index) {
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, sizeof(title), &version));
        assert(version == 2 && strcmp(title, "A&B + 100% = \303\244") == 0);
    }

    memset(&layout, 0, sizeof(layout));
    layout.board = &snapshot->board;
    layout.lists = snapshot->lists; layout.list_count = snapshot->list_count;
    layout.swimlanes = snapshot->swimlanes; layout.swimlane_count = snapshot->swimlane_count;
    memset(&font, 0, sizeof(font)); font.height = 13.0f; font.width = text_width;
    assert(nk_init_default(&context, &font));
    wena_hierarchy_title_init(&state);
    assert(!wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    wena_hierarchy_title_set_adapter(&state, wena_hierarchy_mutation_load,
        wena_hierarchy_mutation_save, &adapter);
    assert(!wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_BOARD, "other"));
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    strcpy(before, snapshot->lists[0].title);
    frame(&context, &state, &layout);
    click(&context, &state, &layout, "Cancel");
    assert(!state.visible && strcmp(before, snapshot->lists[0].title) == 0);
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    frame(&context, &state, &layout);
    click_at(&context, &state, &layout, nk_vec2(200.0f, 145.0f));
    initial_length = state.title_length;
    nk_clear(&context); nk_input_begin(&context);
    nk_input_unicode(&context, (nk_rune)'X'); nk_input_end(&context);
    render(&context, &state, &layout);
    assert(state.title_length == initial_length + 1);
    click(&context, &state, &layout, "Save");
    assert(!state.visible && strchr(snapshot->lists[0].title, 'X') != NULL);
    assert(wena_hierarchy_mutation_load(&adapter, "board", WENA_HIERARCHY_LIST, "list", title, sizeof(title), &version) && version == 3);
    strcpy(before, title);
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    frame(&context, &state, &layout);
    click_at(&context, &state, &layout, nk_vec2(200.0f, 145.0f));
    for (count = 0; count < 160; ++count) {
        nk_clear(&context); nk_input_begin(&context);
        nk_input_unicode(&context, (nk_rune)'a'); nk_input_end(&context);
        render(&context, &state, &layout);
    }
    assert(state.title_length == WENA_CARD_DETAILS_TITLE_CAPACITY);
    click(&context, &state, &layout, "Save");
    assert(state.visible && state.error && strcmp(before, snapshot->lists[0].title) == 0);
    click(&context, &state, &layout, "Cancel");
    assert(!state.visible);
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    state.title_input[0] = '\0'; state.title_length = 0;
    frame(&context, &state, &layout);
    click(&context, &state, &layout, "Save");
    assert(state.visible && state.error);
    click(&context, &state, &layout, "Cancel");
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    execute(database, "UPDATE lists SET version=version+1 WHERE id='list'");
    frame(&context, &state, &layout);
    click(&context, &state, &layout, "Save");
    assert(state.visible && state.error && strcmp(before, snapshot->lists[0].title) == 0);
    click(&context, &state, &layout, "Cancel");
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    snapshot->lists[0].archived = 1;
    assert(!wena_hierarchy_title_render(&context, &state, &layout, 640.0f, 480.0f));
    assert(!state.visible);
    snapshot->lists[0].archived = 0;
    creation_tests(database, &adapter, snapshot, &context, &state, &layout);
    nk_free(&context);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    assert(wena_sqlite_board_load(database, "board", snapshot));
    assert(strcmp(before, snapshot->lists[0].title) == 0);
    assert(snapshot->list_count == 4 && snapshot->swimlane_count == 3);
    assert(sqlite3_close(database) == SQLITE_OK);
    free(snapshot);
    puts("hierarchy SQLite title and real Nuklear editor tests passed");
    return 0;
}
