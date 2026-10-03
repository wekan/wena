#include "checklist_contents.h"
#include "card_body.h"
#include "../../../imports/ui/page_contract.h"
#include "../common/wekan_look.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
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
    char count[48];
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
        /* WeKan's checklist title row: the title in bold, its finished/total
         * count in gray, then the pencil that renames it. */
        for (index = 0, finished = 0; index < list->item_count; ++index)
            finished += list->items[index].is_finished != 0;
        sprintf(count, "%d/%lu", finished, (unsigned long)list->item_count);
        nk_layout_row_begin(context, NK_DYNAMIC, 26.0f, (sections ? 3 : 2) + (editable ? 1 : 0));
        nk_layout_row_push(context, editable || sections ? 0.6f : 0.75f);
        if (wena_wekan_text_button(context, list->checklist.title, WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT))
            action |= WENA_CARD_BODY_OPEN_CHECKLISTS;
        nk_layout_row_push(context, 0.15f);
        wena_wekan_text(context, count, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON, NK_TEXT_LEFT);
        if (editable) {
            nk_layout_row_push(context, 0.1f);
            if (wena_wekan_icon_button(context, WENA_ICON_PENCIL, wena_ui_control_text(WENA_UI_RENAME_CHECKLIST),
                                       12.0f, WENA_WEKAN_ICON))
                (void)wena_checklist_inline_begin(edit, contents, card, list, 0, WENA_CHECKLIST_RENAME);
        }
        if (sections) nk_layout_row_push(context, 0.15f);
        if (sections && wena_card_section_checklist_key(list->checklist.id, section_key, sizeof(section_key)) &&
            wena_card_section_toggle_caret(context, sections, card->board_id, card->id, section_key)) {
            nk_layout_row_end(context);
            continue;
        }
        nk_layout_row_end(context);
        if (drag) {
            nk_layout_row_dynamic(context, 24.0f, 1);
            wena_checklist_drag_handle(context, drag, contents, card, list,
                list_index, WENA_CHECKLIST_REORDER, (!edit || !edit->action) && (!intent || !intent->pending));
        }
        if (drag) wena_checklist_drag_destination(context, drag, contents, card, list);
        if (list->checklist.hide_all_items) continue;
        for (index = 0; index < list->item_count; ++index) {
            if (list->checklist.hide_checked_items && list->items[index].is_finished) continue;
            if (drag) wena_checklist_drag_destination_at(context,drag,contents,card,list,index);
            if (editable || drag) {
                nk_layout_row_begin(context, NK_DYNAMIC, 24.0f, 1 + (editable ? 1 : 0) + (drag ? 1 : 0));
                nk_layout_row_push(context, 0.88f - (editable ? 0.1f : 0.0f) - (drag ? 0.25f : 0.0f));
            } else nk_layout_row_dynamic(context, 24.0f, 1);
            if (intent && (!edit || !edit->action)) {
                finished = list->items[index].is_finished;
                if (wena_wekan_checkbox(context, finished, list->items[index].title, 1) && !intent->pending) {
                    finished = !finished;
                    strcpy(intent->board_id, card->board_id); strcpy(intent->card_id, card->id);
                    strcpy(intent->checklist_id, list->checklist.id);
                    strcpy(intent->item_id, list->items[index].id);
                    intent->card_version = summary->card_version;
                    intent->checklist_version = list->version;
                    intent->item_version = list->item_versions[index];
                    intent->is_finished = finished; intent->pending = 1;
                }
            } else (void)wena_wekan_checkbox(context, list->items[index].is_finished,
                                             list->items[index].title, 0);
            if (editable) {
                nk_layout_row_push(context, 0.1f);
                if (wena_wekan_icon_button(context, WENA_ICON_PENCIL,
                        wena_ui_control_text(WENA_UI_RENAME_CHECKLIST_ITEM), 12.0f, WENA_WEKAN_ICON))
                    (void)wena_checklist_inline_begin(edit, contents, card, list, index, WENA_CHECKLIST_RENAME_ITEM);
            }
            if (drag) {
                nk_layout_row_push(context, 0.25f);
                wena_checklist_drag_handle(context, drag, contents, card, list,
                    index, WENA_CHECKLIST_REORDER_ITEM, (!edit || !edit->action) && (!intent || !intent->pending));
            }
            if (editable || drag) nk_layout_row_end(context);
        }
        if (editable) {
            nk_layout_row_dynamic(context, 26.0f, 1);
            if (wena_wekan_link(context, WENA_ICON_PLUS, wena_ui_control_text(WENA_UI_ADD_CHECKLIST_ITEM),
                                WENA_WEKAN_FONT_LINK, WENA_WEKAN_ADD_CARD))
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
