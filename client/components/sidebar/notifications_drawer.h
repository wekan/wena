#ifndef WENA_NOTIFICATIONS_DRAWER_H
#define WENA_NOTIFICATIONS_DRAWER_H
/* WeKan's notifications drawer (client/components/notifications): under the
 * header at the right, "Notifications" with the unread count and the close
 * cross, then each notification newest first - its read checkbox, who did
 * what on which card or board, and when. */
#include <stddef.h>
#include "../../../server/wekan_sync.h"

struct nk_context;

#define WENA_NOTIFICATIONS_NO_ACTION 0u
#define WENA_NOTIFICATIONS_TOGGLE_READ 1u  /* its index in `index` */
#define WENA_NOTIFICATIONS_CLOSE 2u
#define WENA_NOTIFICATIONS_MARK_ALL_READ 4u

/* How many are not read: WeKan's count in the drawer's title. */
size_t wena_notifications_unread(const WenaWekanNotification *items, size_t count);
/* "who - card", the line a notification shows. */
void wena_notifications_text(const WenaWekanNotification *item, char *out, size_t capacity);
unsigned int wena_notifications_drawer_render(struct nk_context *context, const WenaWekanNotification *items,
                                              size_t count, float width, float height, int *index);

#endif
