#include "card_body.h"
#include "../../../imports/ui/page_contract.h"

#include <nuklear.h>

unsigned int wena_card_body_render(struct nk_context *context,
                                   const WenaCard *card)
{
    unsigned int action;

    if (context == NULL || card == NULL || card->archived) {
        return WENA_CARD_BODY_NO_ACTION;
    }
    action = WENA_CARD_BODY_NO_ACTION;
    nk_layout_row_begin(context, NK_DYNAMIC, 28.0f, 3);
    nk_layout_row_push(context, 0.58f);
    nk_label(context, card->title, NK_TEXT_LEFT);
    nk_layout_row_push(context, 0.25f);
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_OPEN_CARD))) {
        action |= WENA_CARD_BODY_OPEN_DETAILS;
    }
    nk_layout_row_push(context, 0.17f);
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_CARD_MENU))) {
        action |= WENA_CARD_BODY_OPEN_MENU;
    }
    nk_layout_row_end(context);
    return action;
}
