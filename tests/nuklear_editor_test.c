#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/card_details.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct TestStore {
    char title[129];
    unsigned long version;
    int writes;
} TestStore;

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static int load(void *context, const char *board, const char *card,
                char *title, size_t capacity, unsigned long *version)
{
    TestStore *store = (TestStore *)context;
    assert(strcmp(board, "board") == 0 && strcmp(card, "card") == 0);
    assert(strlen(store->title) < capacity);
    strcpy(title, store->title);
    *version = store->version;
    return 1;
}

static int save(void *context, const char *board, const char *card,
                unsigned long version, const char *title)
{
    TestStore *store = (TestStore *)context;
    assert(strcmp(board, "board") == 0 && strcmp(card, "card") == 0);
    if (version != store->version) return 0;
    strcpy(store->title, title);
    ++store->version;
    ++store->writes;
    return 1;
}

static void render(struct nk_context *ctx, WenaCardDetailsState *state,
                    WenaCard *card)
{
    assert(wena_card_details_render(ctx, state, card, 1, 640.0f, 480.0f));
    assert(ctx->current == NULL);
}

/* Locate rendered text, avoiding assumptions about font-specific button widths. */
static struct nk_vec2 label_center(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, ctx) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                           (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing rendered control: %s\n", label);
    assert(0);
    return nk_vec2(0.0f, 0.0f);
}

static void click_at(struct nk_context *ctx, WenaCardDetailsState *state,
                     WenaCard *card, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(ctx);
        nk_input_begin(ctx);
        nk_input_motion(ctx, (int)point.x, (int)point.y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(ctx);
        render(ctx, state, card);
    }
}

static void click(struct nk_context *ctx, WenaCardDetailsState *state,
                  WenaCard *card, const char *label)
{
    click_at(ctx, state, card, label_center(ctx, label));
}

static void character(struct nk_context *ctx, WenaCardDetailsState *state,
                      WenaCard *card, nk_rune rune)
{
    nk_clear(ctx);
    nk_input_begin(ctx);
    nk_input_unicode(ctx, rune);
    nk_input_end(ctx);
    render(ctx, state, card);
}

int main(void)
{
    struct nk_context ctx;
    struct nk_user_font font;
    WenaCardDetailsState state;
    WenaCard card;
    TestStore store;
    int index;

    memset(&font, 0, sizeof(font));
    font.height = 13.0f;
    font.width = text_width;
    memset(&store, 0, sizeof(store));
    strcpy(store.title, "Initial");
    store.version = 1ul;
    assert(nk_init_default(&ctx, &font));
    assert(wena_card_init(&card, "card", "board", "lane", "list",
                          "Initial", 0.0, 0));
    wena_card_details_init(&state);
    wena_card_details_set_title_adapter(&state, load, save, &store);
    assert(wena_card_details_open(&state, &card));
    render(&ctx, &state, &card);
    click(&ctx, &state, &card, "Edit title");
    assert(state.editing_title && state.title_version == 1ul);
    /* A following frame lays out the newly opened editor. */
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx, &state, &card);
    click_at(&ctx, &state, &card, nk_vec2(420.0f, 20.0f));
    character(&ctx, &state, &card, (nk_rune)'X');
    assert(state.title_length == 8);
    click(&ctx, &state, &card, "Save");
    assert(!state.editing_title && store.writes == 1);
    assert(strchr(store.title, 'X') != NULL);

    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx, &state, &card);
    click(&ctx, &state, &card, "Edit title");
    nk_clear(&ctx); nk_input_begin(&ctx); nk_input_end(&ctx);
    render(&ctx, &state, &card);
    click_at(&ctx, &state, &card, nk_vec2(420.0f, 20.0f));
    for (index = 0; index < 160; ++index)
        character(&ctx, &state, &card, (nk_rune)'a');
    assert(state.title_length == WENA_CARD_DETAILS_TITLE_CAPACITY);
    click(&ctx, &state, &card, "Save");
    assert(state.editing_title && state.title_error && store.writes == 1);
    click(&ctx, &state, &card, "Cancel");
    assert(!state.editing_title && store.writes == 1);
    nk_free(&ctx);
    puts("Real Nuklear mouse, text input, save, bounds and cancel passed");
    return 0;
}
