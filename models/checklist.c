#include "checklist.h"
#include "checklist_item.h"
#include <string.h>

int wena_checklist_valid(const WenaChecklist *checklist)
{
    return checklist && wena_model_identifier_valid(checklist->id) &&
        wena_model_identifier_valid(checklist->board_id) &&
        wena_model_identifier_valid(checklist->card_id) &&
        wena_model_title_string_valid(checklist->title,sizeof(checklist->title)) &&
        checklist->position <= WENA_CHECKLIST_POSITION_MAX &&
        (checklist->hide_checked_items==0 || checklist->hide_checked_items==1) &&
        (checklist->hide_all_items==0 || checklist->hide_all_items==1) &&
        (checklist->show_on_minicard==WENA_CHECKLIST_MINICARD_INHERIT ||
         checklist->show_on_minicard==WENA_CHECKLIST_MINICARD_HIDE ||
         checklist->show_on_minicard==WENA_CHECKLIST_MINICARD_SHOW);
}

int wena_checklist_init(WenaChecklist *checklist,const char *id,
    const char *board,const char *card,const char *title,unsigned long position)
{
    WenaChecklist candidate;
    if(!checklist)return 0;
    if(!wena_model_identifier_valid(id)||!wena_model_identifier_valid(board)||
        !wena_model_identifier_valid(card)||
        !wena_model_title_string_valid(title,WENA_CHECKLIST_TITLE_CAPACITY)||
        position>WENA_CHECKLIST_POSITION_MAX){memset(checklist,0,sizeof(*checklist));return 0;}
    memset(&candidate,0,sizeof(candidate));
    strcpy(candidate.id,id);strcpy(candidate.board_id,board);strcpy(candidate.card_id,card);
    strcpy(candidate.title,title);candidate.position=position;
    candidate.show_on_minicard=WENA_CHECKLIST_MINICARD_INHERIT;
    *checklist=candidate;return 1;
}

int wena_checklist_validate_parent(const WenaChecklist *checklist,const WenaCard *card)
{
    return wena_checklist_valid(checklist)&&card&&wena_model_identifier_valid(card->id)&&
        wena_model_identifier_valid(card->board_id)&&
        !strcmp(checklist->card_id,card->id)&&!strcmp(checklist->board_id,card->board_id);
}

int wena_checklist_compare(const void *first,const void *second)
{
    const WenaChecklist *a,*b;
    a=(const WenaChecklist*)first;b=(const WenaChecklist*)second;
    if(a->position<b->position)return -1;
    if(a->position>b->position)return 1;
    return strcmp(a->id,b->id);
}

int wena_checklist_progress(const WenaChecklist *checklist,
    const WenaChecklistItem *items,size_t count,WenaChecklistProgress *progress)
{
    WenaChecklistProgress candidate;
    size_t index,previous;
    if(!progress||!wena_checklist_valid(checklist)||count>WENA_CHECKLIST_MAX_ITEMS||
        (count&&!items))return 0;
    memset(&candidate,0,sizeof(candidate));candidate.total=count;
    for(index=0;index<count;++index){
        if(!wena_checklist_item_validate_parent(&items[index],checklist))return 0;
        for(previous=0;previous<index;++previous)
            if(!strcmp(items[previous].id,items[index].id))return 0;
        if(items[index].is_finished)++candidate.finished;
    }
    if(count)candidate.percent=(unsigned int)(((unsigned long)candidate.finished*100UL+
        (unsigned long)count/2UL)/(unsigned long)count);
    candidate.all_items_finished=count!=0&&candidate.finished==count;
    candidate.is_finished=checklist->hide_all_items||candidate.all_items_finished;
    *progress=candidate;return 1;
}

int wena_checklist_shown_at_minicard(const WenaChecklist *checklist,
    int board_default,int *shown)
{
    if(!shown||!wena_checklist_valid(checklist)||(board_default!=0&&board_default!=1))return 0;
    *shown=checklist->show_on_minicard==WENA_CHECKLIST_MINICARD_INHERIT?
        board_default:checklist->show_on_minicard==WENA_CHECKLIST_MINICARD_SHOW;
    return 1;
}
