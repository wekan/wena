#include "checklist_item.h"
#include <string.h>

int wena_checklist_item_valid(const WenaChecklistItem *item)
{
    return item&&wena_model_identifier_valid(item->id)&&
        wena_model_identifier_valid(item->board_id)&&wena_model_identifier_valid(item->card_id)&&
        wena_model_identifier_valid(item->checklist_id)&&
        wena_model_title_string_valid(item->title,sizeof(item->title))&&
        item->position<=WENA_CHECKLIST_POSITION_MAX&&(item->is_finished==0||item->is_finished==1);
}

int wena_checklist_item_init(WenaChecklistItem *item,const char *id,
    const char *board,const char *card,const char *checklist,const char *title,
    unsigned long position,int is_finished)
{
    WenaChecklistItem candidate;
    if(!item)return 0;
    if(!wena_model_identifier_valid(id)||!wena_model_identifier_valid(board)||
        !wena_model_identifier_valid(card)||!wena_model_identifier_valid(checklist)||
        !wena_model_title_string_valid(title,WENA_CHECKLIST_TITLE_CAPACITY)||
        position>WENA_CHECKLIST_POSITION_MAX||(is_finished!=0&&is_finished!=1)){
        memset(item,0,sizeof(*item));return 0;
    }
    memset(&candidate,0,sizeof(candidate));strcpy(candidate.id,id);strcpy(candidate.board_id,board);
    strcpy(candidate.card_id,card);strcpy(candidate.checklist_id,checklist);strcpy(candidate.title,title);
    candidate.position=position;candidate.is_finished=is_finished;*item=candidate;return 1;
}

int wena_checklist_item_validate_parent(const WenaChecklistItem *item,
    const WenaChecklist *checklist)
{
    return wena_checklist_item_valid(item)&&wena_checklist_valid(checklist)&&
        !strcmp(item->board_id,checklist->board_id)&&!strcmp(item->card_id,checklist->card_id)&&
        !strcmp(item->checklist_id,checklist->id);
}

int wena_checklist_item_set_finished(WenaChecklistItem *item,
    const WenaChecklist *checklist,int is_finished)
{
    if(!wena_checklist_item_validate_parent(item,checklist)||(is_finished!=0&&is_finished!=1))return 0;
    item->is_finished=is_finished;return 1;
}

int wena_checklist_item_compare(const void *first,const void *second)
{
    const WenaChecklistItem *a,*b;
    a=(const WenaChecklistItem*)first;b=(const WenaChecklistItem*)second;
    if(a->position<b->position)return -1;
    if(a->position>b->position)return 1;
    return strcmp(a->id,b->id);
}
