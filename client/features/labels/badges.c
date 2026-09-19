#include "badges.h"
#include "component.h"
#include "../../components/cards/card_body.h"
#include <nuklear.h>

unsigned int wena_label_badges_render(struct nk_context *context,
    const WenaLabelBoardSnapshot *snapshot, const WenaCard *card)
{
    const unsigned char *assignments;
    size_t index;
    unsigned int action;
    if (context == NULL || card == NULL || card->archived || snapshot == NULL)
        return WENA_CARD_BODY_NO_ACTION;
    assignments = wena_label_board_assignments(snapshot, card->board_id, card->id);
    if (assignments == NULL) return WENA_CARD_BODY_NO_ACTION;
    action = WENA_CARD_BODY_NO_ACTION;
    for (index = 0; index < snapshot->catalogue.label_count; ++index) {
        if ((assignments[index / 8u] & (1u << (index % 8u))) != 0u) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            if (wena_label_badge_render(context,
                snapshot->catalogue.labels[index].name,
                snapshot->catalogue.labels[index].color))
                action |= WENA_CARD_BODY_OPEN_LABELS;
        }
    }
    return action;
}
