/* WeKan's notifications drawer (client/components/sidebar/notifications_drawer.c). */
#include "../client/components/sidebar/notifications_drawer.h"
#include "../client/components/common/wekan_look.h"
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

static struct nk_context context;

static int control(const char *name)
{
    size_t count, i;
    const WenaUiControl *controls = wena_ui_controls(&count);
    for (i = 0; i < count; ++i) if (!strcmp(controls[i].name, name)) return 1;
    return 0;
}

int main(void)
{
    WenaWekanNotification items[2];
    char text[64];
    int index;
    memset(items, 0, sizeof(items));
    items[0].index = 2;
    strcpy(items[0].user, "Bob");
    strcpy(items[0].title, "Release");
    items[1].index = 0;
    strcpy(items[1].user, "Ada");
    items[1].read = 1;

    /* The line: who and the card or board; only who when there is no title
     * or no room for both; negative: no room at all. */
    wena_notifications_text(&items[0], text, sizeof(text));
    assert(!strcmp(text, "Bob - Release"));
    wena_notifications_text(&items[1], text, sizeof(text));
    assert(!strcmp(text, "Ada"));
    wena_notifications_text(&items[0], text, 4);
    assert(!strcmp(text, "Bob"));
    wena_notifications_text(&items[0], text, 3);
    assert(text[0] == '\0');
    assert(wena_notifications_unread(items, 2) == 1 && wena_notifications_unread(NULL, 2) == 0);

    /* The title with the unread count, Mark all as read, each line. */
    wena_ui_controls_begin();
    assert(wena_notifications_drawer_render(&context, items, 2, 1024.0f, 720.0f, &index) ==
           WENA_NOTIFICATIONS_NO_ACTION && index == -1);
    assert(control("Notifications (1)") && control("Mark all as read") && control("Bob - Release") && control("Ada"));
    /* A read checkbox gives its entry's place in profile.notifications. */
    context.button_to_press = "Bob - Release";
    assert(wena_notifications_drawer_render(&context, items, 2, 1024.0f, 720.0f, &index) ==
           WENA_NOTIFICATIONS_TOGGLE_READ && index == 2);
    context.button_to_press = "Mark all as read";
    assert(wena_notifications_drawer_render(&context, items, 2, 1024.0f, 720.0f, &index) ==
           WENA_NOTIFICATIONS_MARK_ALL_READ);
    context.button_to_press = "Close";
    assert(wena_notifications_drawer_render(&context, items, 2, 1024.0f, 720.0f, &index) == WENA_NOTIFICATIONS_CLOSE);
    /* All read: no count and no Mark all as read. Negatives: no context,
     * no room, items missing. */
    items[0].read = 1;
    wena_ui_controls_begin();
    (void)wena_notifications_drawer_render(&context, items, 2, 1024.0f, 720.0f, &index);
    assert(control("Notifications") && !control("Mark all as read"));
    assert(wena_notifications_drawer_render(NULL, items, 2, 1024.0f, 720.0f, &index) == WENA_NOTIFICATIONS_NO_ACTION);
    assert(wena_notifications_drawer_render(&context, items, 2, 0.0f, 720.0f, &index) == WENA_NOTIFICATIONS_NO_ACTION);
    assert(wena_notifications_drawer_render(&context, NULL, 2, 1024.0f, 720.0f, &index) == WENA_NOTIFICATIONS_NO_ACTION);
    return 0;
}
