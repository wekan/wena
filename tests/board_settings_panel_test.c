#include "../client/features/boards/settings_panel.h"
#include <nuklear.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static WenaBoardSettingsSnapshot store;
static int calls, commits, fail_load, fail_save, fail_after_commit;
static int load(void *context, const char *board, WenaBoardSettingsSnapshot *out)
{
    (void)context;
    assert(!strcmp(board, "b"));
    if (fail_load) return 0;
    *out = store;
    return 1;
}
static int save(void *context, const char *board, unsigned long version, int enabled)
{
    (void)context;
    assert(!strcmp(board, "b"));
    assert(enabled == 0 || enabled == 1);
    ++calls;
    if (fail_save || version != store.board_version) return 0;
    if (enabled == store.show_checklist_count) return 1;
    store.show_checklist_count = enabled;
    ++store.board_version;
    ++commits;
    if (fail_after_commit) fail_load = 1;
    return 1;
}
static void frame(WenaBoardSettingsState *state, const char *button)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    assert(wena_board_settings_render(&context, state, "b", 800, 600));
    assert(context.begin_count == context.end_count);
}
int main(void)
{
    WenaBoardSettingsState state;
    struct nk_context context;
    int before;
    unsigned long revision;
    memset(&store, 0, sizeof(store));
    strcpy(store.board_id, "b");
    store.board_version = 1;
    wena_board_settings_init(&state, load, save, NULL);
    assert(wena_board_settings_open(&state, "b"));
    assert(!state.show_checklist_count);
    frame(&state, NULL);
    assert(!calls && !commits);
    frame(&state, "Checklist item count (0/0) on minicard");
    assert(state.show_checklist_count && !store.show_checklist_count && !calls);
    frame(&state, "Cancel");
    assert(!state.visible && !store.show_checklist_count && !calls);
    assert(wena_board_settings_open(&state, "b"));
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Save");
    assert(!state.error && state.visible && state.show_checklist_count &&
        state.snapshot.show_checklist_count && store.show_checklist_count && commits == 1);
    revision = store.board_version;
    frame(&state, "Save");
    assert(!state.error && commits == 1 && store.board_version == revision);
    frame(&state, "Checklist item count (0/0) on minicard");
    assert(!state.show_checklist_count && store.show_checklist_count);
    fail_save = 1;
    frame(&state, "Save");
    assert(state.error && !state.show_checklist_count && store.show_checklist_count);
    fail_save = 0;
    frame(&state, "Save");
    assert(!state.error && !store.show_checklist_count && commits == 2);
    frame(&state, "Checklist item count (0/0) on minicard");
    ++store.board_version;
    frame(&state, "Save");
    assert(state.error && state.show_checklist_count && !store.show_checklist_count);
    frame(&state, "Cancel");
    assert(!state.visible);
    assert(wena_board_settings_open(&state, "b"));
    state.show_checklist_count = 2;
    before = calls;
    frame(&state, "Save");
    assert(state.error && calls == before);
    state.show_checklist_count = 1;
    fail_after_commit = 1;
    frame(&state, "Save");
    assert(state.error && state.needs_refresh && store.show_checklist_count &&
        !state.snapshot.show_checklist_count && commits == 3);
    before = calls;
    frame(&state, "Save");
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Refresh");
    assert(state.needs_refresh && calls == before);
    fail_load = 0;
    fail_after_commit = 0;
    frame(&state, "Refresh");
    assert(!state.error && !state.needs_refresh && calls == before &&
        state.snapshot.show_checklist_count && state.show_checklist_count);
    frame(&state, "Close details");
    assert(!state.visible);
    wena_board_settings_close(&state);
    wena_board_settings_init(&state, load, NULL, NULL);
    assert(wena_board_settings_open(&state, "b"));
    frame(&state, "Checklist item count (0/0) on minicard");
    frame(&state, "Save");
    assert(state.show_checklist_count && calls == before);
    memset(&context, 0, sizeof(context));
    assert(!wena_board_settings_render(&context, &state, "other", 800, 600));
    assert(!state.visible && calls == before);
    assert(wena_board_settings_open(&state, "b"));
    context.input.pressed_keys = 1u << NK_KEY_TEXT_RESET_MODE;
    assert(wena_board_settings_render(&context, &state, "b", 800, 600));
    assert(!state.visible && calls == before);
    store.show_checklist_count = -1;
    assert(!wena_board_settings_open(&state, "b"));
    store.show_checklist_count = 1;
    store.board_version = 0;
    assert(!wena_board_settings_open(&state, "b"));
    store.board_version = (unsigned long)LONG_MAX;
    assert(!wena_board_settings_open(&state, "b"));
    store.board_version = 3;
    strcpy(store.board_id, "wrong");
    assert(!wena_board_settings_open(&state, "b"));
    memset(store.board_id, 'x', sizeof(store.board_id));
    assert(!wena_board_settings_open(&state, "b"));
    wena_board_settings_close(&state);
    puts("Board settings UI drafts, cancel, no-op, stale, readonly and refresh guard passed");
    return 0;
}
