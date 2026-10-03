#ifndef WENA_LIST_HEADER_H
#define WENA_LIST_HEADER_H

#include "../../../models/list.h"

struct nk_context;
struct nk_rect;

#define WENA_LIST_HEADER_NO_ACTION 0u
#define WENA_LIST_HEADER_ADD_CARD 1u
#define WENA_LIST_HEADER_OPEN_MENU 2u
#define WENA_LIST_HEADER_ADD_LIST 4u      /* WeKan's "Add List", after this one */
#define WENA_LIST_HEADER_EDIT_TITLE 8u    /* the title clicked */
#define WENA_LIST_HEADER_ADD_CARD_BOTTOM 16u /* with ADD_CARD: "+ Add Card" under the cards */
#define WENA_LIST_HEADER_HEIGHT 98.0f

/* active_count includes the entire list across lanes, before UI filtering. */
unsigned int wena_list_header_render(struct nk_context *context,
                                     const WenaList *list, size_t active_count);
/* WeKan's list header in a 98px row: the collapse caret at the left; Add
 * Card to Top of List, Add List and List Actions at the right; the title
 * under them. *collapse_clicked reports the caret; title_area is where a
 * drag of the list or a click to rename starts. */
unsigned int wena_list_header_render_wekan(struct nk_context *context,
                                           const WenaList *list, size_t active_count,
                                           int collapsed, int *collapse_clicked,
                                           struct nk_rect *title_area);

#endif
