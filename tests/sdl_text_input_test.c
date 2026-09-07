#include "../client/platform/sdl_nuklear.h"
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
    nk_sdl_shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    SDL_Quit();
    puts("complete bounded SDL UTF-8 text event tests passed");
    return 0;
}
