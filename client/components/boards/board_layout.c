#include "board_layout.h"
#include "board_header.h"

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

    for (index = 0; index < layout->card_count; ++index) {
        const WenaCard *card = &layout->cards[index];

        if (!card->archived && wena_same_id(card->board_id, layout->board->id) &&
            wena_same_id(card->list_id, list->id) &&
            wena_same_id(card->swimlane_id, swimlane->id)) {
            nk_layout_row_dynamic(context, 28.0f, 1);
            nk_label(context, card->title, NK_TEXT_LEFT);
        }
    }
}

static void wena_render_lists(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaSwimlane *swimlane)
{
    size_t index;

    for (index = 0; index < layout->list_count; ++index) {
        const WenaList *list = &layout->lists[index];

        if (!list->archived && wena_same_id(list->board_id, layout->board->id) &&
            wena_same_id(list->swimlane_id, swimlane->id) &&
            nk_group_begin(context, list->title, NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            nk_label(context, list->title, NK_TEXT_LEFT);
            wena_render_cards(context, layout, list, swimlane);
            nk_group_end(context);
        }
    }
}

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout)
{
    size_t index;

    if (context == NULL || layout == NULL || layout->board == NULL ||
        layout->board->archived ||
        (layout->swimlane_count != 0 && layout->swimlanes == NULL) ||
        (layout->list_count != 0 && layout->lists == NULL) ||
        (layout->card_count != 0 && layout->cards == NULL)) {
        return 0;
    }
    (void)wena_board_header_render(context, layout->board);
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
    return 1;
}
