#include "drag.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>
void wena_checklist_drag_handle(struct nk_context *context,WenaChecklistDrag *state,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list,size_t position,WenaChecklistAction action,
    int enabled)
{
    const WenaChecklistCardSummary *summary;
    const char *id,*label;
    char scope[WENA_REORDER_SCOPE_CAPACITY];
    int item;
    if (!state || !contents || !card || !list || card->archived ||
        strcmp(contents->summary.board_id,card->board_id) ||
        strcmp(list->checklist.card_id,card->id)) return;
    item=action==WENA_CHECKLIST_REORDER_ITEM;
    if ((!item && action!=WENA_CHECKLIST_REORDER) || (item && position>=list->item_count)) return;
    summary=wena_checklist_summary_find(&contents->summary,card->id);
    if (!summary || summary->archived) return;
    if (item) sprintf(scope,"item:%s/%s/%s",card->board_id,card->id,list->checklist.id);
    else sprintf(scope,"list:%s/%s",card->board_id,card->id);
    id=item ? list->items[position].id : list->checklist.id;
    label=wena_ui_text(item ? WENA_UI_TEXT_MOVE_SELECTION : WENA_UI_TEXT_MOVE_CHECKLIST);
    if (wena_reorder_drag_handle(context,&state->gesture,scope,summary->card_version,
        id,position,label,enabled && !state->error)) {
        state->action=action;state->insert_at_position=0;
        strcpy(state->source.board_id,card->board_id);strcpy(state->source.card_id,card->id);
        strcpy(state->source.checklist_id,list->checklist.id);
        state->source.card_version=summary->card_version;
        state->source.checklist_version=list->version;
        state->source.item_id[0]=0;state->source.item_version=0;
        if (item) {
            strcpy(state->source.item_id,id);
            state->source.item_version=list->item_versions[position];
        }
    }
}

static void destination(struct nk_context *context,WenaChecklistDrag *state,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list,int inserting,size_t position)
{
    const WenaChecklistCardSummary *target;
    const char *id;
    char scope[WENA_REORDER_SCOPE_CAPACITY],label[256];
    int item;
    if (!context || !state || !state->gesture.active || state->gesture.pending || state->error ||
        !contents || !card || card->archived || strcmp(state->source.board_id,card->board_id) ||
        strcmp(contents->summary.board_id,card->board_id)) return;
    item=state->action==WENA_CHECKLIST_REORDER_ITEM;
    if (inserting && position>=(item?WENA_CARD_CHECKLIST_ITEM_CAPACITY:WENA_CARD_CHECKLIST_CAPACITY)) return;
    if (item) {
        if (!list || strcmp(list->checklist.card_id,card->id) ||
            !strcmp(list->checklist.id,state->source.checklist_id)) return;
        sprintf(scope,"item:%s/%s/%s",card->board_id,card->id,list->checklist.id);
        id=list->checklist.id;
    } else {
        if (list || state->action!=WENA_CHECKLIST_REORDER ||
            !strcmp(card->id,state->source.card_id)) return;
        sprintf(scope,"list:%s/%s",card->board_id,card->id);id=card->id;
    }
    target=wena_checklist_summary_find(&contents->summary,card->id);
    if (!target || target->archived) return;
    if (inserting) {
        const char *text;
        text=wena_ui_text(WENA_UI_TEXT_MOVE_DESTINATION);
        if (strlen(text)>sizeof(label)-24u) return;
        sprintf(label,"%s %lu",text,(unsigned long)position+1UL);
    } else {
        const char *text;
        text=wena_ui_text(WENA_UI_TEXT_MOVE_DESTINATION);
        if (strlen(text)>=sizeof(label)) return;
        strcpy(label,text);
    }
    nk_layout_row_dynamic(context,28,1);
    if (wena_reorder_drag_drop(context,&state->gesture,scope,id,
        label,1)) {
        state->insert_at_position=inserting;state->gesture.target_position=position;
        strcpy(state->target_card_id,card->id);
        state->target_card_version=target->card_version;
        state->target_checklist_id[0]=0;state->target_checklist_version=0;
        if (item) {
            strcpy(state->target_checklist_id,list->checklist.id);
            state->target_checklist_version=list->version;
        }
        state->action=item ? WENA_CHECKLIST_MOVE_ITEM : WENA_CHECKLIST_MOVE;
    }
}

void wena_checklist_drag_destination(struct nk_context *context,WenaChecklistDrag *state,
    const WenaChecklistBoardContents *contents,const WenaCard *card,const WenaChecklistContents *list)
{
    destination(context,state,contents,card,list,0,0);
}
void wena_checklist_drag_destination_at(struct nk_context *context,WenaChecklistDrag *state,
    const WenaChecklistBoardContents *contents,const WenaCard *card,
    const WenaChecklistContents *list,size_t position)
{
    destination(context,state,contents,card,list,1,position);
}
