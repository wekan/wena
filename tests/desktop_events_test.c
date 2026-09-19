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

static int card_open_y(void)
{
    return getenv("WENA_TEST_CARD_BADGES") != NULL ? 428 : 400;
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
    int card_y;
    card_y = card_open_y();
    if (frame == 2u) {
        mouse_event(SDL_MOUSEMOTION, 350, card_y);
        mouse_event(SDL_MOUSEBUTTONDOWN, 350, card_y);
    }
    if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 350, card_y);
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
        {18,430,86}, {22,850,122}, {26,500,87}, {30,430,118},
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

static void label_badge_events(unsigned int frame)
{
    static const int clicks[][3] = {{2,350,368}, {6,900,55}, {10,900,55}};
    size_t index;
    SDL_Event event;
    for (index = 0; index < sizeof(clicks)/sizeof(clicks[0]); ++index) {
        if (frame == (unsigned int)clicks[index][0]) {
            mouse_event(SDL_MOUSEMOTION, clicks[index][1], clicks[index][2]);
            mouse_event(SDL_MOUSEBUTTONDOWN, clicks[index][1], clicks[index][2]);
        }
        if (frame == (unsigned int)clicks[index][0] + 1u)
            mouse_event(SDL_MOUSEBUTTONUP, clicks[index][1], clicks[index][2]);
    }
    if (frame == 12u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_TEXTINPUT;
        strcpy(event.text.text, " badge");
        (void)SDL_PushEvent(&event);
    }
    if (frame == 14u || frame == 15u) {
        memset(&event, 0, sizeof(event));
        event.type = frame == 14u ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.state = frame == 14u ? SDL_PRESSED : SDL_RELEASED;
        event.key.keysym.sym = SDLK_RETURN;
        event.key.keysym.scancode = SDL_SCANCODE_RETURN;
        (void)SDL_PushEvent(&event);
    }
    if (frame == 20u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}

static void label_events(unsigned int frame, const char *mode)
{
    static const int clicks[][3] = {
        {2,350,400}, {6,760,184}, {10,500,20}, {14,500,55}
    };
    size_t index;
    SDL_Event event;
    int inspect;
    int click_y;
    inspect = strcmp(mode, "read") == 0;
    for (index = 0; index < sizeof(clicks)/sizeof(clicks[0]); ++index) {
        if (inspect && index >= 2u) break;
        click_y = index == 0u ? card_open_y() : clicks[index][2];
        if (frame == (unsigned int)clicks[index][0]) {
            mouse_event(SDL_MOUSEMOTION, clicks[index][1], click_y);
            mouse_event(SDL_MOUSEBUTTONDOWN, clicks[index][1], click_y);
        }
        if (frame == (unsigned int)clicks[index][0] + 1u)
            mouse_event(SDL_MOUSEBUTTONUP, clicks[index][1], click_y);
    }
    if (!inspect && frame == 16u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_TEXTINPUT;
        strcpy(event.text.text, strcmp(mode, "cancel") == 0 ?
            "Discarded label" : "SDL label \303\204");
        (void)SDL_PushEvent(&event);
    }
    if (!inspect && (frame == 18u || frame == 19u)) {
        memset(&event, 0, sizeof(event));
        event.type = frame == 18u ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.state = frame == 18u ? SDL_PRESSED : SDL_RELEASED;
        event.key.keysym.sym = strcmp(mode, "cancel") == 0 ? SDLK_ESCAPE : SDLK_RETURN;
        event.key.keysym.scancode = strcmp(mode, "cancel") == 0 ?
            SDL_SCANCODE_ESCAPE : SDL_SCANCODE_RETURN;
        (void)SDL_PushEvent(&event);
    }
    if (strcmp(mode, "create") == 0) {
        if (frame == 22u) {
            mouse_event(SDL_MOUSEMOTION, 500, 55);
            mouse_event(SDL_MOUSEBUTTONDOWN, 500, 55);
        }
        if (frame == 23u) mouse_event(SDL_MOUSEBUTTONUP, 500, 55);
    }
    if (frame == 28u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}

static void board_settings_events(unsigned int frame, const char *mode)
{
    SDL_Event event;
    int button_x;
    button_x = strcmp(mode, "cancel") == 0 ? 850 : 430;
    if (frame == 2u || frame == 6u || frame == 10u) {
        int x;
        int y;
        /* Settings is the rightmost toolbar button (frame 2). Once the
         * deferred panel is open, frame 6 toggles its left-edge checkbox. */
        x = frame == 2u ? 900 : frame == 6u ? 330 : button_x;
        y = frame == 2u ? 86 : frame == 6u ? 20 : 55;
        mouse_event(SDL_MOUSEMOTION, x, y);
        mouse_event(SDL_MOUSEBUTTONDOWN, x, y);
    }
    if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 900, 86);
    if (frame == 7u) mouse_event(SDL_MOUSEBUTTONUP, 330, 20);
    if (frame == 11u) mouse_event(SDL_MOUSEBUTTONUP, button_x, 55);
    if (frame == 16u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_QUIT;
        (void)SDL_PushEvent(&event);
    }
}

static void checklist_summary_events(unsigned int frame, const char *mode)
{
    SDL_Event event;
    if (strcmp(mode, "disabled") == 0) {
        /* A disabled count has no card badge, so this must not reach a panel. */
        if (frame == 2u) {
            mouse_event(SDL_MOUSEMOTION, 550, 400);
            mouse_event(SDL_MOUSEBUTTONDOWN, 550, 400);
        }
        if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 550, 400);
    } else if (strcmp(mode, "open") == 0) {
        /* With no label rows in this fixture, the count badge occupies the
         * former card action row. Open it, then Escape without a write. */
        if (frame == 2u) {
            mouse_event(SDL_MOUSEMOTION, 550, 400);
            mouse_event(SDL_MOUSEBUTTONDOWN, 550, 400);
        }
        if (frame == 3u) mouse_event(SDL_MOUSEBUTTONUP, 550, 400);
        if (frame == 7u || frame == 8u) {
            memset(&event, 0, sizeof(event));
            event.type = frame == 7u ? SDL_KEYDOWN : SDL_KEYUP;
            event.key.state = frame == 7u ? SDL_PRESSED : SDL_RELEASED;
            event.key.keysym.sym = SDLK_ESCAPE;
            event.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
            (void)SDL_PushEvent(&event);
        }
    }
    if (strcmp(mode, "legacy") == 0 && frame == 12u) {
        memset(&event, 0, sizeof(event));
        event.type = SDL_TEXTINPUT;
        strcpy(event.text.text, "Counter item");
        (void)SDL_PushEvent(&event);
    }
    if (strcmp(mode, "legacy") == 0 && (frame == 18u || frame == 19u)) {
        memset(&event, 0, sizeof(event));
        event.type = frame == 18u ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.state = frame == 18u ? SDL_PRESSED : SDL_RELEASED;
        event.key.keysym.sym = SDLK_ESCAPE;
        event.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
        (void)SDL_PushEvent(&event);
    }
    if (frame == 15u) {
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
    if (getenv("WENA_TEST_BOARD_SETTINGS") != NULL) {
        board_settings_events(frame, getenv("WENA_TEST_BOARD_SETTINGS"));
        return;
    }
    if (getenv("WENA_TEST_CHECKLIST_SUMMARY") != NULL) {
        checklist_summary_events(frame, getenv("WENA_TEST_CHECKLIST_SUMMARY"));
        return;
    }
    if (getenv("WENA_TEST_LABEL_BADGE") != NULL) {
        label_badge_events(frame);
        return;
    }
    if (getenv("WENA_TEST_LABELS") != NULL) {
        label_events(frame, getenv("WENA_TEST_LABELS"));
        return;
    }
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
