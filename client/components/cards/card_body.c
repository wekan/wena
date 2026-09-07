#include "card_body.h"
#include "../../../imports/ui/page_contract.h"

#include <nuklear.h>
#include <string.h>

unsigned int wena_card_body_render(struct nk_context *context,
                                   const WenaCard *card)
{
    unsigned int action;

    if (context == NULL || card == NULL || card->archived) {
        return WENA_CARD_BODY_NO_ACTION;
    }
    action = WENA_CARD_BODY_NO_ACTION;
    /* Titles use the full column; actions never consume their text width. */
    nk_layout_row_dynamic(context, strlen(card->title) > 32u ? 96.0f : 28.0f, 1);
    nk_label_wrap(context, card->title);
    nk_layout_row_dynamic(context, 28.0f, 2);
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_OPEN_CARD))) {
        action |= WENA_CARD_BODY_OPEN_DETAILS;
    }
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_CARD_MENU))) {
        action |= WENA_CARD_BODY_OPEN_MENU;
    }
    return action;
}
