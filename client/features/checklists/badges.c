#include "badges.h"
#include "../../components/cards/card_body.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

unsigned int wena_checklist_badges_render(struct nk_context *context,
    const WenaChecklistBoardSummary *summary, const WenaCard *card)
{
    const WenaChecklistCardSummary *counts;
    char text[48];
    if (context == NULL || summary == NULL || card == NULL || card->archived ||
        !summary->enabled || strcmp(summary->board_id, card->board_id))
        return WENA_CARD_BODY_NO_ACTION;
    counts = wena_checklist_summary_find(summary, card->id);
    if (counts == NULL || counts->archived || counts->checklist_count == 0)
        return WENA_CARD_BODY_NO_ACTION;
    sprintf(text, "%lu/%lu", (unsigned long)counts->progress.finished,
        (unsigned long)counts->progress.total);
    nk_layout_row_dynamic(context, 28.0f, 2);
    nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_CHECKLISTS));
    return nk_button_label(context, text) ? WENA_CARD_BODY_OPEN_CHECKLISTS :
                                          WENA_CARD_BODY_NO_ACTION;
}
