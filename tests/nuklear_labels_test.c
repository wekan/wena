#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/labels/panel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static WenaLabelSnapshot store;
static int writes;
static int load(void *context, const char *board, const char *card,
    WenaLabelSnapshot *out)
{
    (void)context;
    assert(!strcmp(board, "b") && card && !strcmp(card, "c"));
    *out = store;
    return 1;
}
static int save(void *context, const char *board, const char *card,
    const WenaLabelEdit *edit)
{
    (void)context;
    assert(!strcmp(board, "b") && !strcmp(card, "c"));
    assert(edit->expected_board_version == store.board_version);
    assert(edit->expected_card_version == store.card_version);
    if (edit->action == WENA_LABEL_CREATE) {
        assert(store.label_count == 0);
        assert(wena_label_init(&store.labels[0], "label", "b", edit->name,
            edit->color, 0));
        store.label_count = 1;
        store.label_versions[0] = 1;
    } else {
        assert(!strcmp(edit->label_id, "label"));
        assert(edit->expected_label_version == store.label_versions[0]);
        if (edit->action == WENA_LABEL_EDIT) {
            strcpy(store.labels[0].name, edit->name);
            strcpy(store.labels[0].color, edit->color);
            ++store.label_versions[0];
        } else if (edit->action == WENA_LABEL_ASSIGN || edit->action == WENA_LABEL_UNASSIGN) {
            store.assigned[0] = edit->action == WENA_LABEL_ASSIGN;
            store.assigned_card_counts[0] = (unsigned long)store.assigned[0];
            ++store.card_version;
        } else if (edit->action == WENA_LABEL_DELETE) {
            store.label_count = 0;
            ++store.card_version;
        }
    }
    ++store.board_version;
    ++writes;
    return 1;
}
static float text_width(nk_handle handle, float height, const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}
static void render(struct nk_context *context, WenaLabelsState *state, WenaCard *card)
{
    const struct nk_command *command;
    if (state->visible)
        assert(wena_labels_render(context, state, "b", card, 1, 800, 600));
    nk_foreach(command, context) { (void)command; }
}
static void character(struct nk_context *context, WenaLabelsState *state,
    WenaCard *card, nk_rune rune)
{
    nk_clear(context);
    nk_input_begin(context);
    if (rune) nk_input_unicode(context, rune);
    nk_input_end(context);
    render(context, state, card);
}
static void key(struct nk_context *context, WenaLabelsState *state, WenaCard *card,
    enum nk_keys keycode, int down)
{
    nk_clear(context);
    nk_input_begin(context);
    nk_input_key(context, keycode, down);
    nk_input_end(context);
    render(context, state, card);
}
static const struct nk_command_text *find_text(struct nk_context *context, const char *label)
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
static struct nk_vec2 center(struct nk_context *context, const char *label)
{
    const struct nk_command_text *text;
    text = find_text(context, label);
    if (!text) fprintf(stderr, "Missing labels control: %s\n", label);
    assert(text);
    return nk_vec2((float)text->x + (float)text->w * 0.5f,
        (float)text->y + (float)text->h * 0.5f);
}
static void click_at(struct nk_context *context, WenaLabelsState *state,
    WenaCard *card, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(context);
        nk_input_begin(context);
        nk_input_motion(context, (int)point.x, (int)point.y);
        nk_input_button(context, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(context);
        render(context, state, card);
    }
}
static void click(struct nk_context *context, WenaLabelsState *state,
    WenaCard *card, const char *label)
{
    click_at(context, state, card, center(context, label));
}
static void replace_text(struct nk_context *context, WenaLabelsState *state,
    WenaCard *card, const char *text)
{
    size_t i;
    key(context, state, card, NK_KEY_TEXT_SELECT_ALL, 1);
    key(context, state, card, NK_KEY_TEXT_SELECT_ALL, 0);
    for (i = 0; text[i]; ++i) character(context, state, card, (nk_rune)(unsigned char)text[i]);
}
static void assert_colors(struct nk_context *context, const char *name, const char *color)
{
    const struct nk_command_text *text;
    unsigned char rgb[3], foreground[3];
    text = find_text(context, name);
    assert(text && wena_color_rgb(color, rgb) && wena_color_foreground(color, foreground));
    assert(text->background.r == rgb[0] && text->background.g == rgb[1] &&
        text->background.b == rgb[2]);
    assert(text->foreground.r == foreground[0] && text->foreground.g == foreground[1] &&
        text->foreground.b == foreground[2]);
}
int main(void)
{
    struct nk_context context;
    struct nk_user_font font;
    struct nk_vec2 point;
    WenaLabelsState state;
    WenaCard card;
    int i;
    memset(&store, 0, sizeof(store));
    strcpy(store.board_id, "b");
    strcpy(store.card_id, "c");
    store.board_version = 1;
    store.card_version = 1;
    memset(&font, 0, sizeof(font));
    font.height = 13;
    font.width = text_width;
    assert(nk_init_default(&context, &font));
    assert(wena_card_init(&card, "c", "b", "s", "l", "Card", 0, 0));
    wena_labels_init(&state, load, save, NULL);
    assert(wena_labels_open(&state, "b", &card));
    render(&context, &state, &card);
    click(&context, &state, &card, "Create Label");
    character(&context, &state, &card, 0);
    assert(state.action == WENA_LABEL_CREATE && !strcmp(state.color, "white"));
    assert_colors(&context, "white", "white");
    assert_colors(&context, "black", "black");
    assert_colors(&context, "yellow", "yellow");
    click_at(&context, &state, &card, nk_vec2(350, 55));
    character(&context, &state, &card, 'A');
    assert(state.name_length == 1);
    key(&context, &state, &card, NK_KEY_ENTER, 1);
    assert(writes == 1 && !state.action && !strcmp(store.labels[0].name, "A"));
    key(&context, &state, &card, NK_KEY_ENTER, 1);
    assert(writes == 1);
    key(&context, &state, &card, NK_KEY_ENTER, 0);
    character(&context, &state, &card, 0);
    assert_colors(&context, "A", "white");
    click(&context, &state, &card, "A");
    assert(writes == 2 && state.snapshot->assigned[0]);
    character(&context, &state, &card, 0);
    assert(find_text(&context, "[x]"));
    click(&context, &state, &card, "Change Label");
    character(&context, &state, &card, 0);
    click_at(&context, &state, &card, nk_vec2(350, 55));
    for (i = 0; i < 127; ++i) character(&context, &state, &card, 'x');
    assert(state.name_length == 128);
    character(&context, &state, &card, 0x1f600u);
    assert(state.name_length == 132);
    click(&context, &state, &card, "Save");
    assert(state.error && writes == 2);
    click(&context, &state, &card, "Cancel");
    character(&context, &state, &card, 0);
    click(&context, &state, &card, "Change Label");
    character(&context, &state, &card, 0);
    point = center(&context, "Custom color");
    point.y += 28;
    click_at(&context, &state, &card, point);
    replace_text(&context, &state, &card, "#1234567");
    assert(state.use_custom_color && state.color_length == 8);
    key(&context, &state, &card, NK_KEY_ENTER, 1);
    assert(writes == 2 && state.action == WENA_LABEL_EDIT);
    key(&context, &state, &card, NK_KEY_ENTER, 0);
    click(&context, &state, &card, "Save");
    assert(state.error && writes == 2);
    click_at(&context, &state, &card, point);
    replace_text(&context, &state, &card, "#123AbC");
    click(&context, &state, &card, "Save");
    assert(writes == 3 && !state.action && !strcmp(store.labels[0].color, "#123AbC"));
    character(&context, &state, &card, 0);
    assert_colors(&context, "A", "#123AbC");
    click(&context, &state, &card, "Change Label");
    character(&context, &state, &card, 0);
    click(&context, &state, &card, "Delete");
    character(&context, &state, &card, 0);
    assert(state.action == WENA_LABEL_DELETE && find_text(&context, "Cards") &&
        find_text(&context, "1"));
    key(&context, &state, &card, NK_KEY_ENTER, 1);
    assert(writes == 3 && state.action == WENA_LABEL_DELETE);
    key(&context, &state, &card, NK_KEY_ENTER, 0);
    click(&context, &state, &card, "Cancel");
    assert(writes == 3 && store.label_count == 1);
    key(&context, &state, &card, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && !state.snapshot && writes == 3);
    key(&context, &state, &card, NK_KEY_TEXT_RESET_MODE, 0);
    wena_labels_init(&state, load, NULL, NULL);
    assert(wena_labels_open(&state, "b", &card));
    character(&context, &state, &card, 0);
    assert(!find_text(&context, "Change Label") && !find_text(&context, "Create Label"));
    click(&context, &state, &card, "A");
    assert(writes == 3 && !state.action);
    key(&context, &state, &card, NK_KEY_ENTER, 1);
    assert(writes == 3);
    key(&context, &state, &card, NK_KEY_ENTER, 0);
    key(&context, &state, &card, NK_KEY_TEXT_RESET_MODE, 1);
    assert(!state.visible && writes == 3);
    key(&context, &state, &card, NK_KEY_TEXT_RESET_MODE, 0);
    wena_labels_init(&state, load, save, NULL);
    assert(wena_labels_open(&state, "b", &card));
    character(&context, &state, &card, 0);
    click(&context, &state, &card, "Change Label");
    character(&context, &state, &card, 0);
    click(&context, &state, &card, "Delete");
    character(&context, &state, &card, 0);
    click(&context, &state, &card, "Delete");
    assert(writes == 4 && !state.action && !state.snapshot->label_count);
    wena_labels_close(&state);
    nk_free(&context);
    puts("Real Nuklear labels, contrast, bounded fields, keyboard, readonly and deletion passed");
    return 0;
}
