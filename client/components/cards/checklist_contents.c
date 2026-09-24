#include "checklist_contents.h"
#include "card_body.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>

unsigned int wena_checklist_contents_render_editable(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card,
    int board_default, WenaChecklistCompletionIntent *intent, WenaCardSectionControl *sections, WenaChecklistInlineEdit *edit, WenaChecklistDrag *drag)
{
    const WenaChecklistCardSummary *summary;
    const WenaChecklistContents *list;
    size_t index, list_index;
    int shown, finished, editable;
    unsigned int action;
    char text[WENA_CHECKLIST_TITLE_CAPACITY + 4];
    char section_key[WENA_SECTION_KEY_CAPACITY];
    if (!context || !contents || !card || card->archived ||
        (board_default != 0 && board_default != 1) ||
        strcmp(contents->summary.board_id, card->board_id)) return WENA_CARD_BODY_NO_ACTION;
    summary = wena_checklist_summary_find(&contents->summary, card->id);
    if (!summary || summary->archived) return WENA_CARD_BODY_NO_ACTION;
    action = WENA_CARD_BODY_NO_ACTION;
    if (drag) wena_checklist_drag_destination(context, drag, contents, card, NULL);
    for (list = wena_checklist_contents_find(contents, card->id), list_index = 0; list; list = list->next, ++list_index) {
        if (!wena_checklist_shown_at_minicard(&list->checklist, board_default, &shown) || !shown) continue;
        if (edit && edit->action && !strcmp(edit->card_id, card->id) &&
            !strcmp(edit->checklist_id, list->checklist.id)) {
            nk_layout_row_dynamic(context, 28.0f, 1);
            nk_label_wrap(context, list->checklist.title);
            wena_checklist_inline_render(context, edit);
            continue;
        }
        if (drag) wena_checklist_drag_destination_at(context,drag,contents,card,NULL,list_index);
        editable = edit && !edit->action && (!intent || !intent->pending);
        nk_layout_row_dynamic(context, 28.0f, (sections ? 2 : 1) + (editable ? 1 : 0));
        if (nk_button_label(context, list->checklist.title)) action |= WENA_CARD_BODY_OPEN_CHECKLISTS;
        if (editable && nk_button_label(context, wena_ui_control_text(WENA_UI_RENAME_CHECKLIST)))
            (void)wena_checklist_inline_begin(edit, contents, card, list, 0, WENA_CHECKLIST_RENAME);
        if (sections && wena_card_section_checklist_key(list->checklist.id, section_key, sizeof(section_key)) &&
            wena_card_section_toggle(context, sections, card->board_id, card->id, section_key)) continue;
        if (drag) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            wena_checklist_drag_handle(context, drag, contents, card, list,
                list_index, WENA_CHECKLIST_REORDER, (!edit || !edit->action) && (!intent || !intent->pending));
        }
        if (drag) wena_checklist_drag_destination(context, drag, contents, card, list);
        if (list->checklist.hide_all_items) continue;
        nk_layout_row_dynamic(context, 28.0f, 1);
        for (index = 0; index < list->item_count; ++index) {
            if (list->checklist.hide_checked_items && list->items[index].is_finished) continue;
            if (drag) wena_checklist_drag_destination_at(context,drag,contents,card,list,index);
            nk_layout_row_dynamic(context, 28.0f, (editable ? 2 : 1) + (drag ? 1 : 0));
            if (intent && (!edit || !edit->action)) {
                finished = list->items[index].is_finished;
                if (nk_checkbox_label(context, list->items[index].title, &finished) && !intent->pending) {
                    strcpy(intent->board_id, card->board_id); strcpy(intent->card_id, card->id);
                    strcpy(intent->checklist_id, list->checklist.id);
                    strcpy(intent->item_id, list->items[index].id);
                    intent->card_version = summary->card_version;
                    intent->checklist_version = list->version;
                    intent->item_version = list->item_versions[index];
                    intent->is_finished = finished; intent->pending = 1;
                }
            } else {
                strcpy(text, list->items[index].is_finished ? "[x] " : "[ ] ");
                strcat(text, list->items[index].title);
                nk_label_wrap(context, text);
            }
            if (editable && nk_button_label(context, wena_ui_control_text(WENA_UI_RENAME_CHECKLIST_ITEM)))
                (void)wena_checklist_inline_begin(edit, contents, card, list, index, WENA_CHECKLIST_RENAME_ITEM);
            if (drag) wena_checklist_drag_handle(context, drag, contents, card, list,
                index, WENA_CHECKLIST_REORDER_ITEM, (!edit || !edit->action) && (!intent || !intent->pending));
        }
        if (editable) {
            nk_layout_row_dynamic(context, 28.0f, 1);
            if (nk_button_label(context, wena_ui_control_text(WENA_UI_ADD_CHECKLIST_ITEM)))
                (void)wena_checklist_inline_begin(edit, contents, card, list, 0, WENA_CHECKLIST_ADD_ITEM);
        }
    }
    return action;
}

unsigned int wena_checklist_contents_render(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card, int board_default)
{
    return wena_checklist_contents_render_actions(context, contents, card, board_default, NULL);
}

unsigned int wena_checklist_contents_render_actions(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card, int board_default,
    WenaChecklistCompletionIntent *intent)
{
    return wena_checklist_contents_render_controls(context, contents, card, board_default, intent, NULL);
}

unsigned int wena_checklist_contents_render_controls(struct nk_context *context,
    const WenaChecklistBoardContents *contents, const WenaCard *card, int board_default,
    WenaChecklistCompletionIntent *intent, WenaCardSectionControl *sections)
{
    return wena_checklist_contents_render_editable(context, contents, card,
        board_default, intent, sections, NULL, NULL);
}
