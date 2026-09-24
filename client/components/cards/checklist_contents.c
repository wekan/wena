#include "checklist_contents.h"
#include "card_body.h"
#include <nuklear.h>
#include <string.h>

unsigned int wena_checklist_contents_render(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card,
    int board_default)
{
    const WenaChecklistCardSummary *summary;
    const WenaChecklistContents *list;
    size_t index;
    int shown;
    unsigned int action;
    char text[WENA_CHECKLIST_TITLE_CAPACITY + 4];
    if (!context || !contents || !card || card->archived ||
        (board_default != 0 && board_default != 1) ||
        strcmp(contents->summary.board_id, card->board_id)) return WENA_CARD_BODY_NO_ACTION;
    summary = wena_checklist_summary_find(&contents->summary, card->id);
    if (!summary || summary->archived) return WENA_CARD_BODY_NO_ACTION;
    action = WENA_CARD_BODY_NO_ACTION;
    for (list = wena_checklist_contents_find(contents, card->id); list; list = list->next) {
        if (!wena_checklist_shown_at_minicard(&list->checklist, board_default, &shown) || !shown) continue;
        nk_layout_row_dynamic(context, 28.0f, 1);
        if (nk_button_label(context, list->checklist.title)) action |= WENA_CARD_BODY_OPEN_CHECKLISTS;
        if (list->checklist.hide_all_items) continue;
        nk_layout_row_dynamic(context, 28.0f, 1);
        for (index = 0; index < list->item_count; ++index) {
            if (list->checklist.hide_checked_items && list->items[index].is_finished) continue;
            strcpy(text, list->items[index].is_finished ? "[x] " : "[ ] ");
            strcat(text, list->items[index].title);
            nk_label_wrap(context, text);
        }
    }
    return action;
}
