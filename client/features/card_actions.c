#include "card_actions.h"
#include "../../imports/ui/page_contract.h"

#include <string.h>

size_t wena_card_actions_items(WenaWekanMenuItem *items, const WenaCardMoveState *move,
                               const WenaCardDetailsState *details)
{
    static const WenaUiTextId texts[WENA_CARD_ACTION_COUNT] = {
        WENA_UI_TEXT_MOVE_TO_TOP, WENA_UI_TEXT_MOVE_TO_BOTTOM,
        WENA_UI_TEXT_MOVE_CARD, WENA_UI_TEXT_ARCHIVE_CARD};
    static const WenaIcon icons[WENA_CARD_ACTION_COUNT] = {
        WENA_ICON_ARROW_UP, WENA_ICON_ARROW_DOWN, WENA_ICON_ARROW_RIGHT, WENA_ICON_ARCHIVE};
    /* WeKan's groups: Move to Top and Bottom | Move Card | Archive. */
    static const int separators[WENA_CARD_ACTION_COUNT] = {0, 0, 1, 1};
    size_t index;
    if (items == NULL) return 0;
    for (index = 0; index < WENA_CARD_ACTION_COUNT; ++index) {
        memset(&items[index], 0, sizeof(items[index]));
        items[index].icon = icons[index];
        items[index].text = wena_ui_text(texts[index]);
        items[index].separator_before = separators[index];
    }
    items[WENA_CARD_ACTION_MOVE_TO_TOP].enabled = move != NULL && move->reorder != NULL;
    items[WENA_CARD_ACTION_MOVE_TO_BOTTOM].enabled = move != NULL && move->reorder != NULL;
    items[WENA_CARD_ACTION_MOVE].enabled = move != NULL && move->apply != NULL;
    items[WENA_CARD_ACTION_ARCHIVE].enabled = details != NULL && details->archive_card != NULL &&
        details->load_title != NULL;
    return WENA_CARD_ACTION_COUNT;
}

/* Archive: the card's current version, then the details' archive adapter, as
 * the details panel's own Archive does. */
static int archive(const WenaCardDetailsState *details, const WenaBoardLayout *layout,
                   const char *card_id)
{
    char title[WENA_CARD_DETAILS_TITLE_CAPACITY];
    unsigned long version;
    if (details == NULL || details->archive_card == NULL || details->load_title == NULL ||
        layout == NULL || layout->board == NULL || card_id == NULL) return 0;
    version = 0ul;
    if (!details->load_title(details->title_context, layout->board->id, card_id, title,
                             sizeof(title), &version) || version == 0ul) return 0;
    return details->archive_card(details->title_context, layout->board->id, card_id, version);
}

int wena_card_actions_apply(WenaCardAction action, WenaCardMoveState *move,
                            const WenaCardDetailsState *details,
                            const WenaBoardLayout *layout, const char *card_id)
{
    switch (action) {
    case WENA_CARD_ACTION_MOVE_TO_TOP:
    case WENA_CARD_ACTION_MOVE_TO_BOTTOM:
        return wena_card_move_to_end(move, layout, card_id,
                                     action == WENA_CARD_ACTION_MOVE_TO_BOTTOM) ?
            WENA_CARD_ACTIONS_DONE : WENA_CARD_ACTIONS_FAILED;
    case WENA_CARD_ACTION_MOVE:
        return move != NULL && wena_card_move_open(move, layout, card_id) ?
            WENA_CARD_ACTIONS_OPEN_MOVE : WENA_CARD_ACTIONS_FAILED;
    case WENA_CARD_ACTION_ARCHIVE:
        return archive(details, layout, card_id) ? WENA_CARD_ACTIONS_DONE : WENA_CARD_ACTIONS_FAILED;
    default:
        return WENA_CARD_ACTIONS_FAILED;
    }
}
