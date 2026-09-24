#include "../client/features/boards/settings_panel.h"
#include "../client/features/boards/settings.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Adapter {
    WenaBoardSettingsMutation mutation;
    int saves;
    int fail_after_save;
    int fail_load;
} Adapter;
static void sql(sqlite3 *database, const char *query)
{
    char *error;
    int result;
    error = NULL;
    result = sqlite3_exec(database, query, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "%s: %s\n", query, error);
    sqlite3_free(error);
    assert(result == SQLITE_OK);
}
static sqlite3_int64 number(sqlite3 *database, const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(database, query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    result = sqlite3_column_int64(statement, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return result;
}
static void schema(sqlite3 *database, const char *path)
{
    FILE *file;
    long length;
    char *data;
    file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    length = ftell(file);
    assert(length > 0);
    rewind(file);
    data = (char *)malloc((size_t)length + 1);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    data[length] = 0;
    assert(!fclose(file));
    sql(database, data);
    free(data);
}
static int load(void *context, const char *board, WenaBoardSettingsSnapshot *snapshot)
{
    Adapter *adapter;
    adapter = (Adapter *)context;
    return !adapter->fail_load &&
        wena_board_settings_mutation_load(&adapter->mutation, board, snapshot);
}
static int save(void *context, const char *board, unsigned long version, int enabled, int contents)
{
    Adapter *adapter;
    adapter = (Adapter *)context;
    ++adapter->saves;
    if (!wena_board_settings_mutation_save_display(&adapter->mutation, board, version, enabled, contents)) return 0;
    if (adapter->fail_after_save) adapter->fail_load = 1;
    return 1;
}
static void frame(WenaBoardSettingsState *state, const char *button)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    assert(wena_board_settings_render(&context, state, "b", 800, 600));
}
int main(int argc, char **argv)
{
    sqlite3 *database, *second;
    Adapter adapter;
    WenaBoardSettingsMutation concurrent;
    WenaBoardSettingsState state;
    struct nk_context context;
    sqlite3_int64 keys, version;
    unsigned long request;
    int saves;
    assert(argc == 4);
    assert(sqlite3_open(argv[3], &database) == SQLITE_OK);
    sql(database, "PRAGMA foreign_keys=ON");
    schema(database, argv[1]);
    sql(database, "INSERT INTO actors VALUES('u','User',1);"
        "INSERT INTO boards VALUES('b','Board',1),('other','Other',1)");
    memset(&adapter, 0, sizeof(adapter));
    assert(wena_board_settings_mutation_init(&adapter.mutation, database, "u", "b"));
    wena_board_settings_init(&state, load, save, &adapter);
    assert(!wena_board_settings_open(&state, "b"));
    schema(database, argv[2]);
    assert(wena_board_settings_open(&state, "b"));
    assert(!state.show_checklist_count && state.show_checklists);
    frame(&state, "Save");
    assert(!state.error && number(database, "SELECT count(*) FROM board_settings") == 0 &&
        number(database, "SELECT count(*) FROM idempotency_keys") == 0 &&
        number(database, "SELECT version FROM boards WHERE id='b'") == 1);
    frame(&state, "Checklists");assert(!state.show_checklists);
    frame(&state, "Cancel");assert(wena_board_settings_open(&state, "b") && state.show_checklists);
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, NULL);
    assert(state.show_checklist_count && number(database, "SELECT count(*) FROM board_settings") == 0);
    frame(&state, "Cancel");
    assert(!state.visible && number(database, "SELECT count(*) FROM board_settings") == 0);
    assert(wena_board_settings_open(&state, "b"));
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Save");
    assert(!state.error && state.show_checklist_count && state.snapshot.board_version == 2 &&
        number(database, "SELECT show_checklist_count FROM board_settings WHERE board_id='b'") == 1);
    keys = number(database, "SELECT count(*) FROM idempotency_keys");
    frame(&state, "Save");
    assert(!state.error && state.snapshot.board_version == 2 &&
        number(database, "SELECT count(*) FROM idempotency_keys") == keys);
    /* A stale payload rejects even when another writer made the same choice. */
    frame(&state, "Checklist item count (0/0) on minicard");
    assert(sqlite3_open(argv[3], &second) == SQLITE_OK);
    assert(wena_board_settings_mutation_init(&concurrent, second, "u", "b"));
    assert(wena_board_settings_mutation_save(&concurrent, "b", state.snapshot.board_version, 0));
    assert(sqlite3_close(second) == SQLITE_OK);
    frame(&state, "Save");
    assert(state.error && !state.show_checklist_count && state.snapshot.show_checklist_count);
    frame(&state, "Cancel");
    assert(wena_board_settings_open(&state, "b"));
    frame(&state, "Checklist item count (0/0) on minicard");
    version = number(database, "SELECT version FROM boards WHERE id='b'");
    keys = number(database, "SELECT count(*) FROM idempotency_keys");
    sql(database, "CREATE TRIGGER rollback_setting BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'rollback setting'); END");
    frame(&state, "Save");
    assert(state.error && state.show_checklist_count && !state.snapshot.show_checklist_count &&
        number(database, "SELECT show_checklist_count FROM board_settings WHERE board_id='b'") == 0 &&
        number(database, "SELECT version FROM boards WHERE id='b'") == version &&
        number(database, "SELECT count(*) FROM idempotency_keys") == keys);
    sql(database, "DROP TRIGGER rollback_setting");
    adapter.fail_after_save = 1;
    frame(&state, "Save");
    assert(state.error && state.needs_refresh && !state.snapshot.show_checklist_count &&
        number(database, "SELECT show_checklist_count FROM board_settings WHERE board_id='b'") == 1);
    saves = adapter.saves;
    frame(&state, "Save");
    frame(&state, "Refresh");
    assert(state.needs_refresh && adapter.saves == saves);
    adapter.fail_after_save = 0;
    adapter.fail_load = 0;
    frame(&state, "Refresh");
    assert(!state.needs_refresh && !state.error && state.snapshot.show_checklist_count &&
        adapter.saves == saves);
    request = (unsigned long)number(database, "SELECT min(request_version) FROM idempotency_keys");
    assert(request && !wena_board_settings_mutation_save_display_request(&adapter.mutation,
        "b", state.snapshot.board_version, 0, 1, request));
    assert(!wena_board_settings_mutation_save(&adapter.mutation,
        "other", state.snapshot.board_version, 0));
    memset(&context, 0, sizeof(context));
    assert(!wena_board_settings_render(&context, &state, "other", 800, 600));
    assert(!state.visible && number(database, "SELECT version FROM boards WHERE id='other'") == 1);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[3], &database) == SQLITE_OK);
    assert(wena_board_settings_mutation_init(&adapter.mutation, database, "u", "b"));
    assert(wena_board_settings_open(&state, "b"));
    assert(state.show_checklist_count);
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Cancel");
    assert(number(database, "SELECT show_checklist_count FROM board_settings WHERE board_id='b'") == 1);
    assert(wena_board_settings_open(&state, "b"));
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Save");
    assert(!state.error && !state.show_checklist_count);
    wena_board_settings_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[3], &database) == SQLITE_OK);
    assert(wena_board_settings_mutation_init(&adapter.mutation, database, "u", "b"));
    assert(wena_board_settings_open(&state, "b"));
    state.show_checklists=0;frame(&state,"Save");assert(!state.error&&!state.snapshot.show_checklists);
    wena_board_settings_close(&state);assert(sqlite3_close(database)==SQLITE_OK);
    assert(sqlite3_open(argv[3],&database)==SQLITE_OK);
    assert(wena_board_settings_mutation_init(&adapter.mutation,database,"u","b"));
    assert(wena_board_settings_open(&state,"b"));assert(!state.show_checklists);
    wena_board_settings_close(&state);
    wena_board_settings_init(&state, load, NULL, &adapter);
    assert(wena_board_settings_open(&state, "b") && !state.show_checklist_count);
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Save");
    assert(!state.show_checklist_count &&
        number(database, "SELECT show_checklist_count FROM board_settings WHERE board_id='b'") == 0);
    wena_board_settings_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    puts("Board settings UI SQLite default, drafts, guards, rollback, refresh and reopen passed");
    return 0;
}
