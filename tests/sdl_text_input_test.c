#include "../client/platform/sdl_nuklear.h"
#include "../client/components/forms/input_limits.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int input(struct nk_context *context, const char *text)
{
    SDL_Event event;
    assert(strlen(text) < sizeof(event.text.text));
    memset(&event, 0, sizeof(event));
    event.type = SDL_TEXTINPUT;
    strcpy(event.text.text, text);
    return wena_sdl_handle_event(context, &event);
}

static void rejected_paste(struct nk_text_edit *edit, char *memory,
                           size_t capacity, const char *text, size_t length)
{
    struct nk_text_edit before;
    char bytes[WENA_NATIVE_EDIT_CAPACITY(129)];
    assert(capacity <= sizeof(bytes));
    before = *edit; memcpy(bytes, memory, capacity);
    assert(!wena_sdl_paste_text(edit, text, length));
    assert(!memcmp(&before, edit, sizeof(before)));
    assert(!memcmp(bytes, memory, capacity));
}

/* Exercise the actual pinned SDL clipboard callback. Its whole-paste behavior
 * differs from SDL_TEXTINPUT; record that limitation instead of claiming that
 * scalar overflow slack provides atomic arbitrary-length paste replacement. */
static void clipboard_boundaries(struct nk_context *context)
{
    struct nk_text_edit edit;
    char memory[WENA_NATIVE_EDIT_CAPACITY(129)];
    char oversized[145], prefix[129];
    char supplementary[5] = {(char)240, (char)159, (char)152, (char)128, 0};
    const char *bad[] = {"valid\300\257", "valid\342\202", "valid\355\240\200",
        "valid\364\220\200\200", "valid\200", "valid\302\200",
        "valid\n", "valid\t", "valid\r", "valid\177"};
    size_t index;
    struct nk_text_edit before;
    assert(context->clip.paste);
    memset(prefix, 'a', 128); prefix[128] = 0;
    memset(oversized, 'q', sizeof(oversized) - 1); oversized[144] = 0;
    nk_textedit_init_fixed(&edit, memory, sizeof(memory));
    edit.mode = NK_TEXT_EDIT_MODE_INSERT;
    nk_textedit_text(&edit, prefix, 128);
    assert(edit.string.buffer.allocated == 128);
    assert(SDL_SetClipboardText("\360\237\230\200") == 0);
    context->clip.paste(context->clip.userdata, &edit);
    assert(edit.string.buffer.allocated == 132);
    assert(!memcmp(memory + 128, "\360\237\230\200", 4));
    assert(edit.cursor == 129 && edit.string.len == 129);
    nk_textedit_free(&edit);

    nk_textedit_init_fixed(&edit, memory, sizeof(memory));
    edit.mode = NK_TEXT_EDIT_MODE_INSERT;
    nk_textedit_text(&edit, "old", 3);
    assert(SDL_SetClipboardText(oversized) == 0);
    context->clip.paste(context->clip.userdata, &edit);
    assert(edit.string.buffer.allocated == 3 && !memcmp(memory, "old", 3));
    /* The native callback preflights before Nuklear can delete selection. */
    edit.select_start = 0; edit.select_end = 3; edit.cursor = 3;
    context->clip.paste(context->clip.userdata, &edit);
    assert(edit.string.buffer.allocated == 3 && !memcmp(memory, "old", 3));
    assert(edit.select_start == 0 && edit.select_end == 3 && edit.cursor == 3);
    before = edit;
    assert(wena_sdl_paste_text(&edit, NULL, 0));
    assert(!memcmp(&before, &edit, sizeof(edit)));
    for (index = 0; index < sizeof(bad) / sizeof(bad[0]); ++index)
        rejected_paste(&edit, memory, sizeof(memory), bad[index], strlen(bad[index]));
    rejected_paste(&edit, memory, sizeof(memory), "a\0b", 3);
    rejected_paste(&edit, memory, sizeof(memory), NULL, 1);
    edit.mode = NK_TEXT_EDIT_MODE_VIEW;
    rejected_paste(&edit, memory, sizeof(memory), "a", 1);
    edit.mode = NK_TEXT_EDIT_MODE_INSERT;
    edit.filter = nk_filter_decimal;
    rejected_paste(&edit, memory, sizeof(memory), "1a", 2);
    edit.filter = NULL;
    /* Exact-sized supplementary input catches the old byte/rune over-read
     * under ASan. Reverse selection and insertion cursor count actual runes. */
    edit.select_start = 3; edit.select_end = 0;
    assert(wena_sdl_paste_text(&edit, supplementary, 4));
    assert(edit.string.buffer.allocated == 4 && edit.string.len == 1 && edit.cursor == 1);
    assert(!memcmp(memory, supplementary, 4));
    assert(wena_sdl_paste_text(&edit, "x", 1));
    assert(edit.string.buffer.allocated == 5 && edit.string.len == 2 && edit.cursor == 2);
    assert(memory[4] == 'x');
    nk_textedit_undo(&edit);
    assert(edit.string.buffer.allocated == 4 && edit.string.len == 1);
    nk_textedit_redo(&edit);
    assert(edit.string.buffer.allocated == 5 && edit.string.len == 2);
    nk_textedit_free(&edit);

    /* Insertion in the middle preserves the untouched UTF-8 suffix. */
    nk_textedit_init_fixed(&edit, memory, sizeof(memory));
    edit.mode = NK_TEXT_EDIT_MODE_INSERT;
    assert(wena_sdl_paste_text(&edit, "\303\204Z", 3));
    edit.cursor = 1; edit.select_start = edit.select_end = 1;
    assert(wena_sdl_paste_text(&edit, supplementary, 4));
    assert(edit.string.buffer.allocated == 7 && edit.string.len == 3 && edit.cursor == 2);
    assert(!memcmp(memory, "\303\204\360\237\230\200Z", 7));
    nk_textedit_free(&edit);

    nk_textedit_init_fixed(&edit, memory, sizeof(memory));
    edit.mode = NK_TEXT_EDIT_MODE_INSERT; edit.single_line = 0;
    assert(SDL_SetClipboardText("A\r\n\t\360\237\230\200") == 0);
    context->clip.paste(context->clip.userdata, &edit);
    assert(edit.string.buffer.allocated == 8 && edit.string.len == 5 && edit.cursor == 5);
    assert(!memcmp(memory, "A\r\n\t\360\237\230\200", 8));
    rejected_paste(&edit, memory, sizeof(memory), "a\001", 2);
    rejected_paste(&edit, memory, sizeof(memory), "\302\205", 2);
    nk_textedit_free(&edit);

    /* Reserve the final NUL byte and reject a whole replacement before any
     * selection/undo mutation, even when no scalar slack remains. */
    nk_textedit_init_fixed(&edit, memory, 6);
    edit.mode = NK_TEXT_EDIT_MODE_INSERT;
    assert(wena_sdl_paste_text(&edit, supplementary, 4));
    assert(wena_sdl_paste_text(&edit, "x", 1));
    rejected_paste(&edit, memory, 6, "y", 1);
    edit.select_start = 0; edit.select_end = 2;
    rejected_paste(&edit, memory, 6, "123456", 6);
    assert(wena_sdl_paste_text(&edit, "12345", 5));
    assert(edit.string.buffer.allocated == 5 && edit.string.len == 5 && edit.cursor == 5);
    nk_textedit_free(&edit);
}

int main(void)
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    struct nk_context *context;
    struct nk_font_atlas *atlas;
    SDL_Event event;
    const char *bad[] = {"valid\300\257", "valid\342\202", "valid\355\240\200",
        "valid\364\220\200\200", "valid\200", "valid\302\200",
        "valid\n", "valid\t", "valid\177"};
    char maximum[32], original[NK_INPUT_MAX];
    size_t index;
    int previous;
    assert(NK_INPUT_MAX == 256);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    window = SDL_CreateWindow("Text event test", 0, 0, 160, 120, SDL_WINDOW_HIDDEN);
    assert(window);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    context = nk_sdl_init(window, renderer); assert(context);
    {
        nk_plugin_copy copy;
        copy = context->clip.copy;
        wena_sdl_install_clipboard(NULL); wena_sdl_install_clipboard(context);
        assert(context->clip.copy == copy);
    }
    nk_sdl_font_stash_begin(&atlas); assert(atlas);
    nk_sdl_font_stash_end();
    nk_input_begin(context);
    assert(input(context, "Several ASCII characters"));
    assert(context->input.keyboard.text_len == 24);
    assert(!memcmp(context->input.keyboard.text, "Several ASCII characters", 24));
    assert(input(context, " \303\204\303\244 \342\202\254 \360\237\230\200"));
    assert(context->input.keyboard.text_len == 38);
    assert(!memcmp(context->input.keyboard.text,
        "Several ASCII characters \303\204\303\244 \342\202\254 \360\237\230\200", 38));
    previous = context->input.keyboard.text_len;
    memcpy(original, context->input.keyboard.text, sizeof(original));
    for (index = 0; index < sizeof(bad) / sizeof(bad[0]); ++index) {
        assert(!input(context, bad[index]));
        assert(context->input.keyboard.text_len == previous);
        assert(!memcmp(original, context->input.keyboard.text, sizeof(original)));
    }
    memset(&event, 0, sizeof(event)); event.type = SDL_TEXTINPUT;
    memset(event.text.text, 'x', sizeof(event.text.text));
    assert(!wena_sdl_handle_event(context, &event));
    assert(context->input.keyboard.text_len == previous);
    assert(!memcmp(original, context->input.keyboard.text, sizeof(original)));
    assert(input(context, ""));
    assert(context->input.keyboard.text_len == previous);
    nk_input_end(context);
    nk_input_begin(context);
    memset(maximum, 'x', sizeof(maximum) - 1); maximum[31] = 0;
    for (index = 0; index < 8; ++index) assert(input(context, maximum));
    assert(context->input.keyboard.text_len == 248);
    memcpy(original, context->input.keyboard.text, sizeof(original));
    assert(!input(context, "12345678"));
    assert(context->input.keyboard.text_len == 248);
    assert(!memcmp(original, context->input.keyboard.text, sizeof(original)));
    assert(input(context, "1234567"));
    assert(context->input.keyboard.text_len == 255);
    assert(!input(context, "x"));
    assert(context->input.keyboard.text_len == 255);
    nk_input_end(context);
    nk_input_begin(context);
    /* Normal nontext input still reaches the pinned SDL backend. */
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_LEFT;
    assert(wena_sdl_handle_event(context, &event));
    assert(nk_input_is_key_down(&context->input, NK_KEY_LEFT));
    assert(!wena_sdl_handle_event(NULL, &event));
    assert(!wena_sdl_handle_event(context, NULL));
    assert(context->input.keyboard.text_len == 0);
    nk_input_end(context);
    clipboard_boundaries(context);
    nk_sdl_shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    SDL_Quit();
    puts("complete bounded SDL UTF-8 text event tests passed");
    return 0;
}
