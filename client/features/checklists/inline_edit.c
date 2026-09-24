#include "inline_edit.h"
#include "../../components/forms/text_form.h"
#include <string.h>
void wena_checklist_inline_cancel(WenaChecklistInlineEdit *edit)
{if(edit)memset(edit,0,sizeof(*edit));}
int wena_checklist_inline_begin(WenaChecklistInlineEdit *edit,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list,size_t item,WenaChecklistAction action)
{
    const WenaChecklistCardSummary *summary;const char *title;
    if (!edit || !contents || !card || !list || card->archived ||
        strcmp(card->board_id,contents->summary.board_id) ||
        strcmp(list->checklist.card_id,card->id) ||
        (action!=WENA_CHECKLIST_RENAME && action!=WENA_CHECKLIST_RENAME_ITEM && action!=WENA_CHECKLIST_ADD_ITEM)) return 0;
    summary=wena_checklist_summary_find(&contents->summary,card->id);
    if (!summary || summary->archived || (action==WENA_CHECKLIST_RENAME_ITEM && item>=list->item_count)) return 0;
    title=action==WENA_CHECKLIST_ADD_ITEM ? "" : (action==WENA_CHECKLIST_RENAME ? list->checklist.title : list->items[item].title);
    wena_checklist_inline_cancel(edit);edit->action=action;
    strcpy(edit->board_id,card->board_id);strcpy(edit->card_id,card->id);strcpy(edit->checklist_id,list->checklist.id);
    edit->card_version=summary->card_version;edit->checklist_version=list->version;
    if(action==WENA_CHECKLIST_RENAME_ITEM){strcpy(edit->item_id,list->items[item].id);edit->item_version=list->item_versions[item];}
    strcpy(edit->input,title);edit->length=(int)strlen(title);return 1;
}
void wena_checklist_inline_render(struct nk_context *context,WenaChecklistInlineEdit *edit)
{
    unsigned int action;
    if (!edit || !edit->action) return;
    action=wena_text_form_render(context,edit->input,&edit->length,(int)sizeof(edit->input),edit->error);
    if(action & WENA_TEXT_FORM_CANCEL)wena_checklist_inline_cancel(edit);
    else if(action & WENA_TEXT_FORM_SAVE)edit->pending=1;
}
void wena_checklist_inline_sync(WenaChecklistInlineEdit *edit,const WenaChecklistBoardContents *contents, int board_default)
{
    const WenaChecklistContents *list;const WenaChecklistCardSummary *card;size_t index;int shown;
    if (!edit || !edit->action) return;
    if (!contents || strcmp(edit->board_id,contents->summary.board_id)) {wena_checklist_inline_cancel(edit);return;}
    card=wena_checklist_summary_find(&contents->summary,edit->card_id);
    if (!card || card->archived) {wena_checklist_inline_cancel(edit);return;}
    list=wena_checklist_contents_find(contents,edit->card_id);
    while(list && strcmp(list->checklist.id,edit->checklist_id))list=list->next;
    if (!list || !wena_checklist_shown_at_minicard(&list->checklist,board_default,&shown) || !shown) {wena_checklist_inline_cancel(edit);return;}
    if (edit->action==WENA_CHECKLIST_RENAME_ITEM) {
        for(index=0;index<list->item_count;++index)if(!strcmp(list->items[index].id,edit->item_id))return;
        wena_checklist_inline_cancel(edit);
    }
}
