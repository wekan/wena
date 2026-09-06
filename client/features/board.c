#include "board.h"
#include "../components/cards/card_body.h"

#include <nuklear.h>
#include <string.h>

static void wena_board_open_card_details(const WenaBoardLayout *layout,
                                         WenaCardDetailsState *card_details)
{
    size_t index;

    if (card_details == NULL || layout->card_interaction == NULL ||
        (layout->card_interaction->actions & WENA_CARD_BODY_OPEN_DETAILS) == 0u) {
        return;
    }
    for (index = 0; index < layout->card_count; ++index) {
        if (strcmp(layout->cards[index].id,
                   layout->card_interaction->card_id) == 0) {
            (void)wena_card_details_open(card_details, &layout->cards[index]);
            return;
        }
    }
}

int wena_board_feature_render(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              float width, float height)
{
    return wena_board_feature_render_with_state(context, layout, width, height,
                                                NULL);
}

int wena_board_feature_render_with_state(struct nk_context *context,
                                         const WenaBoardLayout *layout,
                                         float width, float height,
                                         WenaCardDetailsState *card_details)
{
    int rendered;

    if (context == NULL || layout == NULL || width <= 0.0f || height <= 0.0f) {
        return 0;
    }
    rendered = 0;
    if (nk_begin(context, "WeKan", nk_rect(0.0f, 0.0f, width, height),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        rendered = wena_board_layout_render(context, layout);
    }
    nk_end(context);
    if (rendered) {
        wena_board_open_card_details(layout, card_details);
    }
    return rendered;
}
