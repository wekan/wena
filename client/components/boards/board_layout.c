#include "board_layout.h"
#include "board_header.h"
#include "../lists/list_header.h"
#include "../cards/card_body.h"

#include <nuklear.h>
#include <string.h>

static int wena_same_id(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static void wena_render_cards(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaList *list,
                              const WenaSwimlane *swimlane)
{
    size_t index;
    unsigned int card_action;

    for (index = 0; index < layout->card_count; ++index) {
        const WenaCard *card = &layout->cards[index];

        if (!card->archived && wena_same_id(card->board_id, layout->board->id) &&
            wena_same_id(card->list_id, list->id) &&
            wena_same_id(card->swimlane_id, swimlane->id)) {
            card_action = wena_card_body_render(context, card);
            if (layout->card_interaction != NULL &&
                card_action != WENA_CARD_BODY_NO_ACTION) {
                layout->card_interaction->actions = card_action;
                (void)wena_model_set_required(layout->card_interaction->card_id,
                    sizeof(layout->card_interaction->card_id), card->id);
            }
        }
    }
}

static void wena_render_lists(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaSwimlane *swimlane)
{
    size_t index;
    unsigned int list_action;

    for (index = 0; index < layout->list_count; ++index) {
        const WenaList *list = &layout->lists[index];

        if (!list->archived && wena_same_id(list->board_id, layout->board->id) &&
            wena_same_id(list->swimlane_id, swimlane->id) &&
            nk_group_begin(context, list->title, NK_WINDOW_BORDER)) {
            list_action = wena_list_header_render(context, list);
            if (layout->list_interaction != NULL &&
                list_action != WENA_LIST_HEADER_NO_ACTION) {
                layout->list_interaction->actions = list_action;
                (void)wena_model_set_required(layout->list_interaction->list_id,
                    sizeof(layout->list_interaction->list_id), list->id);
            }
            wena_render_cards(context, layout, list, swimlane);
            nk_group_end(context);
        }
    }
}

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout)
{
    size_t index;
    unsigned int header_action;

    if (context == NULL || layout == NULL || layout->board == NULL ||
        layout->board->archived ||
        (layout->swimlane_count != 0 && layout->swimlanes == NULL) ||
        (layout->list_count != 0 && layout->lists == NULL) ||
        (layout->card_count != 0 && layout->cards == NULL)) {
        return 0;
    }
    if (layout->list_interaction != NULL) {
        layout->list_interaction->actions = WENA_LIST_HEADER_NO_ACTION;
        layout->list_interaction->list_id[0] = '\0';
    }
    if (layout->card_interaction != NULL) {
        layout->card_interaction->actions = WENA_CARD_BODY_NO_ACTION;
        layout->card_interaction->card_id[0] = '\0';
    }
    header_action = wena_board_header_render(context, layout->board);
    if (layout->sidebar != NULL &&
        (header_action & WENA_BOARD_HEADER_OPEN_MENU) != 0u) {
        layout->sidebar->visible = 1;
    }
    for (index = 0; index < layout->swimlane_count; ++index) {
        const WenaSwimlane *swimlane = &layout->swimlanes[index];

        if (!swimlane->archived &&
            wena_same_id(swimlane->board_id, layout->board->id) &&
            nk_group_begin(context, swimlane->title, NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(context, 26.0f, 1);
            nk_label(context, swimlane->title, NK_TEXT_LEFT);
            wena_render_lists(context, layout, swimlane);
            nk_group_end(context);
        }
    }
    (void)wena_board_sidebar_render(context, layout->sidebar);
    return 1;
}
