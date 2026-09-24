#include "card_selection.h"
#include <stdlib.h>
#include <string.h>

int wena_card_selection_init(WenaCardSelection *selection,const char *board)
{
    WenaId id;
    if(!selection||!wena_model_identifier_valid(board))return 0;
    strcpy(id,board);memset(selection,0,sizeof(*selection));strcpy(selection->board_id,id);return 1;
}
void wena_card_selection_clear(WenaCardSelection *selection)
{
    if(selection){memset(selection->ids,0,sizeof(selection->ids));selection->count=0;}
}
int wena_card_selection_contains(const WenaCardSelection *selection,const char *id)
{
    size_t i;
    if(!selection||selection->count>WENA_CARD_SELECTION_CAPACITY||!wena_model_identifier_valid(id))return 0;
    for(i=0;i<selection->count;++i)if(!strcmp(selection->ids[i],id))return 1;
    return 0;
}
static int valid(const WenaCardSelection *selection,const WenaCard *cards,size_t count)
{
    size_t i,j;
    if(!selection||!wena_model_identifier_valid(selection->board_id)||
        selection->count>WENA_CARD_SELECTION_CAPACITY||count>WENA_CARD_SELECTION_CAPACITY||(!cards&&count))return 0;
    for(i=0;i<selection->count;++i){
        if(!wena_model_identifier_valid(selection->ids[i]))return 0;
        for(j=0;j<i;++j)if(!strcmp(selection->ids[i],selection->ids[j]))return 0;
    }
    for(i=0;i<count;++i){
        if(!wena_model_identifier_valid(cards[i].id)||!wena_model_identifier_valid(cards[i].board_id)||
            !wena_model_identifier_valid(cards[i].list_id)||!wena_model_identifier_valid(cards[i].swimlane_id)||
            (cards[i].archived!=0&&cards[i].archived!=1))return 0;
        for(j=0;j<i;++j)if(!strcmp(cards[i].id,cards[j].id)&&!strcmp(cards[i].board_id,cards[j].board_id))return 0;
    }
    return 1;
}
static int active(const WenaCardSelection *selection,const WenaCard *cards,size_t count,const char *id)
{
    size_t i;
    for(i=0;i<count;++i)if(!cards[i].archived&&!strcmp(cards[i].board_id,selection->board_id)&&!strcmp(cards[i].id,id))return 1;
    return 0;
}
static int update(WenaCardSelection *selection,const WenaCard *cards,size_t count,
    const char *list,const char *lane,const char *toggle)
{
    WenaCardSelection *next;size_t i;int remove;
    if(!valid(selection,cards,count)||(toggle&&!active(selection,cards,count,toggle)))return 0;
    next=(WenaCardSelection*)calloc(1,sizeof(*next));if(!next)return 0;
    strcpy(next->board_id,selection->board_id);remove=toggle&&wena_card_selection_contains(selection,toggle);
    for(i=0;i<selection->count;++i)if(active(selection,cards,count,selection->ids[i])&&
        !(remove&&!strcmp(selection->ids[i],toggle)))strcpy(next->ids[next->count++],selection->ids[i]);
    if(toggle&&!remove)strcpy(next->ids[next->count++],toggle);
    if(list)for(i=0;i<count;++i)if(!cards[i].archived&&!strcmp(cards[i].board_id,next->board_id)&&
        !strcmp(cards[i].list_id,list)&&(!lane||!lane[0]||!strcmp(cards[i].swimlane_id,lane))&&
        !wena_card_selection_contains(next,cards[i].id))strcpy(next->ids[next->count++],cards[i].id);
    /* Every retained/added ID belongs to the validated cache, so count is
     * bounded by its unique active-card count and cannot exceed capacity. */
    memcpy(selection,next,sizeof(*next));free(next);return 1;
}
int wena_card_selection_add(WenaCardSelection *selection,const WenaCard *cards,
    size_t count,const char *list,const char *lane)
{
    if(!wena_model_identifier_valid(list)||(lane&&lane[0]&&!wena_model_identifier_valid(lane)))return 0;
    return update(selection,cards,count,list,lane,NULL);
}
int wena_card_selection_toggle(WenaCardSelection *selection,const WenaCard *cards,size_t count,const char *id)
{
    if(!wena_model_identifier_valid(id))return 0;
    return update(selection,cards,count,NULL,NULL,id);
}
int wena_card_selection_sync(WenaCardSelection *selection,const WenaCard *cards,size_t count)
{return update(selection,cards,count,NULL,NULL,NULL);}
