/* Linux-only test interposition: drive the unmodified desktop through SDL's
 * public event queue after fully rendered frames. No application test hooks. */
#include <SDL.h>
#include <string.h>
#include <stdlib.h>

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

static void hierarchy_move_events(unsigned int frame)
{
    SDL_Event event;
    /* Open the first list's title editor and transfer focus to Move list. */
    if (frame == 2u) {
        mouse_event(SDL_MOUSEMOTION, 200, 301);
        mouse_event(SDL_MOUSEBUTTONDOWN, 200, 301);
    }
    if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 200, 301);
    if (frame == 6u) {
        mouse_event(SDL_MOUSEMOTION, 400, 260);
        mouse_event(SDL_MOUSEBUTTONDOWN, 400, 260);
    }
    if (frame == 7u) mouse_event(SDL_MOUSEBUTTONUP, 400, 260);
    if (frame == 10u) {
        mouse_event(SDL_MOUSEMOTION, 400, 196);
        mouse_event(SDL_MOUSEBUTTONDOWN, 400, 196);
    }
    if (frame == 11u) mouse_event(SDL_MOUSEBUTTONUP, 400, 196);
    if (frame == 14u) {
        mouse_event(SDL_MOUSEMOTION, 400, 260);
        mouse_event(SDL_MOUSEBUTTONDOWN, 400, 260);
    }
    if (frame == 15u) mouse_event(SDL_MOUSEBUTTONUP, 400, 260);
    if (frame == 18u) {
        mouse_event(SDL_MOUSEMOTION, 300, 232);
        mouse_event(SDL_MOUSEBUTTONDOWN, 300, 232);
    }
    if (frame == 19u) mouse_event(SDL_MOUSEBUTTONUP, 300, 232);
    if (frame == 23u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}

static void description_events(unsigned int frame, const char *mode)
{
    SDL_Event event;
    int button_x;
    if (frame == 2u) {
        mouse_event(SDL_MOUSEMOTION, 350, 400);
        mouse_event(SDL_MOUSEBUTTONDOWN, 350, 400);
    }
    if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 350, 400);
    if (frame == 6u) {
        mouse_event(SDL_MOUSEMOTION, 760, 120);
        mouse_event(SDL_MOUSEBUTTONDOWN, 760, 120);
    }
    if (frame == 7u) mouse_event(SDL_MOUSEBUTTONUP, 760, 120);
    if (frame == 10u) {
        mouse_event(SDL_MOUSEMOTION, 400, 70);
        mouse_event(SDL_MOUSEBUTTONDOWN, 400, 70);
    }
    if (frame == 11u) mouse_event(SDL_MOUSEBUTTONUP, 400, 70);
    if (frame == 12u || frame == 15u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_TEXTINPUT;
        strcpy(event.text.text, frame == 12u ? "First \303\204\303\244" : "Second \316\251");
        (void)SDL_PushEvent(&event);
    }
    if (frame == 13u || frame == 14u) {
        memset(&event, 0, sizeof(event));
        event.type = frame == 13u ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.state = frame == 13u ? SDL_PRESSED : SDL_RELEASED;
        event.key.keysym.sym = SDLK_RETURN;
        event.key.keysym.scancode = SDL_SCANCODE_RETURN;
        (void)SDL_PushEvent(&event);
    }
    button_x = strcmp(mode, "cancel") == 0 ? 660 : 430;
    if (frame == 18u) {
        mouse_event(SDL_MOUSEMOTION, button_x, 620);
        mouse_event(SDL_MOUSEBUTTONDOWN, button_x, 620);
    }
    if (frame == 19u) mouse_event(SDL_MOUSEBUTTONUP, button_x, 620);
    if (frame == 23u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}

static void checklist_events(unsigned int frame)
{
    static const int clicks[][3] = {
        {2,350,400}, {6,760,152}, {10,500,20}, {14,500,55},
        {18,430,86}, {22,850,122}, {26,500,55}, {30,430,86},
        {34,330,230}, {38,430,86}
    };
    size_t i;
    SDL_Event event;
    for (i = 0; i < sizeof(clicks)/sizeof(clicks[0]); ++i) {
        if (frame == (unsigned int)clicks[i][0]) {
            mouse_event(SDL_MOUSEMOTION, clicks[i][1], clicks[i][2]);
            mouse_event(SDL_MOUSEBUTTONDOWN, clicks[i][1], clicks[i][2]);
        }
        if (frame == (unsigned int)clicks[i][0]+1u)
            mouse_event(SDL_MOUSEBUTTONUP, clicks[i][1], clicks[i][2]);
    }
    if (frame == 16u || frame == 28u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_TEXTINPUT;
        strcpy(event.text.text, frame == 16u ? "SDL checklist" : "SDL item \303\204");
        (void)SDL_PushEvent(&event);
    }
    if (frame == 43u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}

void SDL_RenderPresent(SDL_Renderer *renderer)
{
    static unsigned int frame;
    SDL_Event event;
    (void)renderer;
    ++frame;
    if (getenv("WENA_TEST_COLLAPSE") != NULL) {
        if (frame == 2u) {
            mouse_event(SDL_MOUSEMOTION, 400, 333);
            mouse_event(SDL_MOUSEBUTTONDOWN, 400, 333);
        }
        if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 400, 333);
        if (frame == 8u) {
            memset(&event, 0, sizeof(event)); event.type = SDL_QUIT;
            (void)SDL_PushEvent(&event);
        }
        return;
    }
    if (getenv("WENA_TEST_CHECKLIST") != NULL) {
        checklist_events(frame);
        return;
    }
    if (getenv("WENA_TEST_DESCRIPTION") != NULL) {
        description_events(frame, getenv("WENA_TEST_DESCRIPTION"));
        return;
    }
    if (getenv("WENA_TEST_HIERARCHY_MOVE") != NULL) {
        hierarchy_move_events(frame);
        return;
    }
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
