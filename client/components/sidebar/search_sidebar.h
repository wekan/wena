#ifndef WENA_SEARCH_SIDEBAR_H
#define WENA_SEARCH_SIDEBAR_H
/* WeKan's board Search (client/components/sidebar/sidebarSearches.jade):
 * the sidebar with the term's field - Enter searches - then the lists and
 * the cards found, a card opening when clicked. */
#include <stddef.h>
#include "../../../models/model.h"
#include "../../../models/list.h"
#include "../../../models/card.h"

struct nk_context;

#define WENA_SEARCH_RESULTS 64
typedef struct WenaSearchSidebar {
    int visible;
    int focus;                         /* put the cursor in the field */
    char term[129];
    int length;
    int searched;                      /* results are for `term` */
    WenaId lists[WENA_SEARCH_RESULTS];
    size_t list_count;
    WenaId cards[WENA_SEARCH_RESULTS];
    size_t card_count;
} WenaSearchSidebar;

#define WENA_SEARCH_NO_ACTION 0u
#define WENA_SEARCH_SUBMIT 1u          /* Enter: search for `term` */
#define WENA_SEARCH_OPEN_CARD 2u       /* a found card: its id in `card` */
#define WENA_SEARCH_CLOSE 4u

void wena_search_sidebar_open(WenaSearchSidebar *search);
/* The sidebar at the right under the header, as WeKan's; lists and cards
 * give the found ids their titles (an id not there is left out). */
unsigned int wena_search_sidebar_render(struct nk_context *context, WenaSearchSidebar *search,
                                        const WenaList *lists, size_t list_count,
                                        const WenaCard *cards, size_t card_count,
                                        float width, float height, char *card, size_t capacity);

#endif
