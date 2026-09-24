#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/boards/settings_panel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static WenaBoardSettingsSnapshot store;
static int calls, fail_load, fail_after_save;
static int load(void *context, const char *board, WenaBoardSettingsSnapshot *out)
{
    (void)context;
    assert(!strcmp(board, "b"));
    if (fail_load) return 0;
    *out = store;
    return 1;
}
static int save(void *context, const char *board, unsigned long version, int enabled, int contents)
{
    (void)context;
    assert(!strcmp(board, "b") && version == store.board_version);
    assert(enabled == 0 || enabled == 1);
    ++calls;
    if (enabled != store.show_checklist_count || contents != store.show_checklists) {
        store.show_checklist_count = enabled;
        store.show_checklists = contents;
        ++store.board_version;
    }
    if (fail_after_save) fail_load = 1;
    return 1;
}
static float width(nk_handle handle, float height, const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}
static void render(struct nk_context *context, WenaBoardSettingsState *state)
{
    const struct nk_command *command;
    if (state->visible) assert(wena_board_settings_render(context, state, "b", 800, 600));
    nk_foreach(command, context) { (void)command; }
}
static void key(struct nk_context *context, WenaBoardSettingsState *state,
    enum nk_keys code, int down)
{
    nk_clear(context);
    nk_input_begin(context);
    nk_input_key(context, code, down);
    nk_input_end(context);
    render(context, state);
}
static const struct nk_command_text *find(struct nk_context *context, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, context) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            !memcmp(text->string, label, (size_t)text->length)) return text;
    }
    return NULL;
}
static void click(struct nk_context *context, WenaBoardSettingsState *state,
    const char *label)
{
    const struct nk_command_text *text;
    int x, y, down;
    text = find(context, label);
    if (!text) fprintf(stderr, "Missing board setting control: %s\n", label);
    assert(text);
    x = (int)text->x + (int)text->w / 2;
    y = (int)text->y + (int)text->h / 2;
    for (down = 1; down >= 0; --down) {
        nk_clear(context);
        nk_input_begin(context);
        nk_input_motion(context, x, y);
        nk_input_button(context, NK_BUTTON_LEFT, x, y, down);
        nk_input_end(context);
        render(context, state);
    }
}
int main(void)
{
    struct nk_context context;
    struct nk_user_font font;
    WenaBoardSettingsState state;
    const char *option;
    unsigned long revision;
    option = "Checklist item count (0/0) on minicard";
    memset(&store, 0, sizeof(store));
    strcpy(store.board_id, "b");
    store.board_version = 1;
    store.show_checklists = 1;
    memset(&font, 0, sizeof(font));
    font.height = 13;
    font.width = width;
    assert(nk_init_default(&context, &font));
    wena_board_settings_init(&state, load, save, NULL);
    assert(wena_board_settings_open(&state, "b"));
    render(&context, &state);
    click(&context, &state, option);
    assert(state.show_checklist_count && !store.show_checklist_count && !calls);
    key(&context, &state, NK_KEY_ENTER, 1);
    assert(state.show_checklist_count && !store.show_checklist_count && !calls);
    key(&context, &state, NK_KEY_ENTER, 0);
    click(&context, &state, "Cancel");
    assert(!state.visible && !store.show_checklist_count && !calls);
    assert(wena_board_settings_open(&state, "b"));
    key(&context, &state, NK_KEY_ENTER, 0);
    assert(!state.show_checklist_count);
    click(&context, &state, option);
    click(&context, &state, "Save");
    assert(calls == 1 && store.show_checklist_count && !state.error && state.visible);
    revision = store.board_version;
    click(&context, &state, "Save");
    assert(calls == 2 && store.board_version == revision);
    click(&context, &state, option);
    key(&context, &state, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && store.show_checklist_count && calls == 2);
    key(&context, &state, NK_KEY_TEXT_RESET_MODE, 0);
    assert(wena_board_settings_open(&state, "b"));
    key(&context, &state, NK_KEY_ENTER, 0);
    click(&context, &state, option);
    fail_after_save = 1;
    click(&context, &state, "Save");
    assert(calls == 3 && state.needs_refresh && state.error && !store.show_checklist_count);
    key(&context, &state, NK_KEY_ENTER, 1);
    assert(!find(&context, option) && !find(&context, "Save") && calls == 3);
    key(&context, &state, NK_KEY_ENTER, 0);
    click(&context, &state, "Refresh");
    assert(state.needs_refresh && calls == 3);
    fail_after_save = 0;
    fail_load = 0;
    click(&context, &state, "Refresh");
    assert(!state.needs_refresh && !state.error && !state.show_checklist_count && calls == 3);
    key(&context, &state, NK_KEY_ENTER, 0);
    click(&context, &state, "Close details");
    assert(!state.visible);
    wena_board_settings_init(&state, load, NULL, NULL);
    assert(wena_board_settings_open(&state, "b"));
    key(&context, &state, NK_KEY_ENTER, 0);
    assert(find(&context, "[ ]") && find(&context, option) && !find(&context, "Save"));
    click(&context, &state, option);
    assert(!state.show_checklist_count && calls == 3);
    key(&context, &state, NK_KEY_ENTER, 1);
    assert(calls == 3);
    key(&context, &state, NK_KEY_ENTER, 0);
    key(&context, &state, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && calls == 3);
    wena_board_settings_close(&state);
    key(&context, &state, NK_KEY_TEXT_RESET_MODE, 0);
    wena_board_settings_init(&state, load, save, NULL);
    assert(wena_board_settings_open(&state, "b"));
    key(&context, &state, NK_KEY_ENTER, 0);
    assert(state.show_checklists && store.show_checklists);
    click(&context, &state, "Checklists");
    assert(!state.show_checklists && store.show_checklists && calls == 3);
    click(&context, &state, "Cancel");assert(store.show_checklists && calls == 3);
    assert(wena_board_settings_open(&state, "b"));key(&context, &state, NK_KEY_ENTER, 0);
    click(&context, &state, "Checklists");revision=store.board_version;
    click(&context, &state, "Save");
    assert(!state.error && !store.show_checklists && store.board_version==revision+1 && calls==4);
    wena_board_settings_close(&state);
    nk_free(&context);
    puts("Real Nuklear board setting drafts, explicit Save, keyboard and readonly passed");
    return 0;
}
