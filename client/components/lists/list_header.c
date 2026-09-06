#include "list_header.h"

#include <nuklear.h>

unsigned int wena_list_header_render(struct nk_context *context,
                                     const WenaList *list)
{
    unsigned int action;

    if (context == NULL || list == NULL || list->archived) {
        return WENA_LIST_HEADER_NO_ACTION;
    }
    action = WENA_LIST_HEADER_NO_ACTION;
    nk_layout_row_begin(context, NK_DYNAMIC, 24.0f, 3);
    nk_layout_row_push(context, 0.58f);
    nk_label(context, list->title, NK_TEXT_LEFT);
    nk_layout_row_push(context, 0.25f);
    if (nk_button_label(context, "Add card")) {
        action |= WENA_LIST_HEADER_ADD_CARD;
    }
    nk_layout_row_push(context, 0.17f);
    if (nk_button_label(context, "List menu")) {
        action |= WENA_LIST_HEADER_OPEN_MENU;
    }
    nk_layout_row_end(context);
    return action;
}
