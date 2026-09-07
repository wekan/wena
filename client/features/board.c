#include "board.h"
#include "../components/cards/card_body.h"

#include <nuklear.h>
#include <string.h>

static int wena_board_open_card_details(const WenaBoardLayout *layout,
                                         WenaCardDetailsState *card_details)
{
    size_t index;

    if (card_details == NULL || layout->card_interaction == NULL ||
        (layout->card_interaction->actions & (WENA_CARD_BODY_OPEN_DETAILS |
                                              WENA_CARD_BODY_OPEN_MENU)) == 0u) {
        return 0;
    }
    for (index = 0; index < layout->card_count; ++index) {
        if (strcmp(layout->cards[index].id,
                   layout->card_interaction->card_id) == 0) {
            return wena_card_details_open(card_details, &layout->cards[index]);
        }
    }
    return 0;
}

static void wena_board_sidebar_window(struct nk_context *context,
                                       const WenaBoardLayout *layout,
                                       float width, float height)
{
    float panel_width;
    float content_height;
    float panel_top;
    float panel_height;

    if (!layout->sidebar_as_window || layout->sidebar == NULL ||
        !layout->sidebar->visible) return;
    panel_width = width < 360.0f ? width : 360.0f;
    /* Keep the opening header click outside the new menu's controls, so its
       release cannot accidentally select a section in the newly shown window. */
    panel_top = height > 44.0f ? 44.0f : 0.0f;
    panel_height = height - panel_top;
    content_height = panel_height > 24.0f ? panel_height - 24.0f : 1.0f;
    if (nk_begin(context, "Wena board menu",
        nk_rect(width - panel_width, panel_top, panel_width, panel_height),
        NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(context, content_height, 1);
        (void)wena_board_sidebar_render(context, layout->sidebar);
    }
    nk_end(context);
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
                 NK_WINDOW_BORDER)) {
        rendered = wena_board_layout_render(context, layout);
    }
    nk_end(context);
    if (rendered) {
        if (card_details != NULL && card_details->visible && layout->card_visible != NULL) {
            size_t index;
            for (index = 0; index < layout->card_count; ++index)
                if (!strcmp(layout->cards[index].id, card_details->card_id) &&
                    !layout->card_visible(layout->card_visible_context, &layout->cards[index])) {
                    wena_card_details_close(card_details); break;
                }
        }

        /* The opening click belongs to the board, never a new dialog action. */
        if (!wena_board_open_card_details(layout, card_details))
            (void)wena_card_details_render(context, card_details, layout->cards,
                                           layout->card_count, width, height);
        wena_board_sidebar_window(context, layout, width, height);
    }
    return rendered;
}
