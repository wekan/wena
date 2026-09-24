#include "card_people.h"
#include <stdlib.h>
#include <string.h>
int wena_board_members_valid(const WenaBoardMember *members,size_t count,const char *board)
{
    size_t i,j;
    if(!wena_model_identifier_valid(board)||count>WENA_BOARD_MEMBER_CAPACITY||(!members&&count))return 0;
    for(i=0;i<count;++i){
        if(!wena_model_identifier_valid(members[i].board_id)||strcmp(members[i].board_id,board)||
            !wena_model_identifier_valid(members[i].actor_id)||(members[i].active!=0&&members[i].active!=1))return 0;
        for(j=0;j<i;++j)if(!strcmp(members[i].actor_id,members[j].actor_id))return 0;
    }
    return 1;
}
int wena_card_people_init(WenaCardPeople *people,const char *board)
{
    WenaId id;
    if(!people||!wena_model_identifier_valid(board))return 0;
    strcpy(id,board);memset(people,0,sizeof(*people));strcpy(people->board_id,id);return 1;
}
int wena_card_people_valid(const WenaCardPeople *people)
{
    size_t i,j;
    if(!people||!wena_model_identifier_valid(people->board_id)||people->count>WENA_CARD_PEOPLE_CAPACITY)return 0;
    for(i=0;i<people->count;++i){
        if(!wena_model_identifier_valid(people->ids[i]))return 0;
        for(j=0;j<i;++j)if(!strcmp(people->ids[i],people->ids[j]))return 0;
    }
    return 1;
}
static int active(const WenaBoardMember *members,size_t count,const char *actor)
{
    size_t i;
    for(i=0;i<count;++i)if(!strcmp(members[i].actor_id,actor))return members[i].active;
    return 0;
}
int wena_card_people_set(const WenaCardPeople *people,const WenaBoardMember *members,
    size_t count,const char *actor,int enabled,WenaCardPeople *output,int *changed)
{
    WenaCardPeople *candidate;size_t i,index;int differs;
    if(!output||!changed||!wena_card_people_valid(people)||!wena_model_identifier_valid(actor)||
        (enabled!=0&&enabled!=1)||!wena_board_members_valid(members,count,people->board_id)||
        (enabled&&!active(members,count,actor)))return 0;
    for(index=0;index<people->count;++index)if(!strcmp(people->ids[index],actor))break;
    differs=enabled?(index==people->count):(index<people->count);
    if(enabled&&differs&&people->count==WENA_CARD_PEOPLE_CAPACITY)return 0;
    if(!differs){if(output!=people)*output=*people;*changed=0;return 1;}
    candidate=(WenaCardPeople*)malloc(sizeof(*candidate));if(!candidate)return 0;
    *candidate=*people;
    if(enabled){strcpy(candidate->ids[candidate->count],actor);++candidate->count;}
    else{
        for(i=index+1;i<candidate->count;++i)memcpy(candidate->ids[i-1],candidate->ids[i],sizeof(WenaId));
        --candidate->count;memset(candidate->ids[candidate->count],0,sizeof(WenaId));
    }
    *output=*candidate;*changed=1;free(candidate);return 1;
}
int wena_card_people_transfer(const WenaCardPeople *people,const WenaBoardMember *members,
    size_t count,const char *target_board,WenaCardPeople *output)
{
    WenaCardPeople *candidate;size_t i;
    if(!output||!wena_card_people_valid(people)||!wena_board_members_valid(members,count,target_board)||
        !strcmp(people->board_id,target_board))return 0;
    candidate=(WenaCardPeople*)malloc(sizeof(*candidate));if(!candidate)return 0;
    (void)wena_card_people_init(candidate,target_board);
    for(i=0;i<people->count;++i)if(active(members,count,people->ids[i])){
        strcpy(candidate->ids[candidate->count],people->ids[i]);++candidate->count;
    }
    *output=*candidate;free(candidate);return 1;
}
