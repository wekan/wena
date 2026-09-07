/* Linux-only test interposition: drive the unmodified desktop through SDL's
 * public event queue after fully rendered frames. No application test hooks. */
#include <SDL.h>
#include <string.h>

static void mouse_event(Uint32 type, int x, int y)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    if (type == SDL_MOUSEMOTION) {
        event.motion.x = x;
        event.motion.y = y;
    } else {
        event.button.button = SDL_BUTTON_LEFT;
        event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        event.button.x = x;
        event.button.y = y;
    }
    (void)SDL_PushEvent(&event);
}

void SDL_RenderPresent(SDL_Renderer *renderer)
{
    static unsigned int frame;
    SDL_Event event;
    (void)renderer;
    ++frame;
    /* Board menu, then the toolbar outside its right-hand overlay. */
    if (frame == 2u) {
        mouse_event(SDL_MOUSEMOTION, 900, 22);
        mouse_event(SDL_MOUSEBUTTONDOWN, 900, 22);
    }
    if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 900, 22);
    if (frame == 6u) {
        mouse_event(SDL_MOUSEMOTION, 150, 86);
        mouse_event(SDL_MOUSEBUTTONDOWN, 150, 86);
    }
    if (frame == 7u) mouse_event(SDL_MOUSEBUTTONUP, 150, 86);
    /* Focus the creation title, type, and save on separate rendered frames. */
    if (frame == 10u) {
        mouse_event(SDL_MOUSEMOTION, 300, 191);
        mouse_event(SDL_MOUSEBUTTONDOWN, 300, 191);
    }
    if (frame == 11u) mouse_event(SDL_MOUSEBUTTONUP, 300, 191);
    if (frame == 12u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_TEXTINPUT;
        /* SDL may deliver an entire UTF-8 phrase in one text event. */
        strcpy(event.text.text, "Sidebar \303\204\303\244 regression");
        (void)SDL_PushEvent(&event);
    }
    if (frame == 15u) {
        mouse_event(SDL_MOUSEMOTION, 300, 232);
        mouse_event(SDL_MOUSEBUTTONDOWN, 300, 232);
    }
    if (frame == 16u) mouse_event(SDL_MOUSEBUTTONUP, 300, 232);
    if (frame == 20u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}
