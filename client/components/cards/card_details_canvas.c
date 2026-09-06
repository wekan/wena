#include "card_details_canvas.h"

#include <nuklear.h>

unsigned int wena_card_details_canvas_render(struct nk_context *context,
                                             const WenaCard *card)
{
    unsigned int action;

    if (context == NULL || card == NULL || card->archived) {
        return WENA_CARD_DETAILS_NO_ACTION;
    }
    action = WENA_CARD_DETAILS_NO_ACTION;
    nk_layout_row_dynamic(context, 32.0f, 1);
    nk_label(context, card->title, NK_TEXT_LEFT);
    nk_layout_row_dynamic(context, 28.0f, 2);
    if (nk_button_label(context, "Edit title")) {
        action |= WENA_CARD_DETAILS_EDIT_TITLE;
    }
    if (nk_button_label(context, "Archive card")) {
        action |= WENA_CARD_DETAILS_ARCHIVE;
    }
    nk_layout_row_dynamic(context, 28.0f, 1);
    if (nk_button_label(context, "Close details")) {
        action |= WENA_CARD_DETAILS_CLOSE;
    }
    return action;
}
