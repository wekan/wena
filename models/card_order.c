#include "card_order.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int position_valid(double position)
{
    return position>=0&&position<(double)(LONG_MAX-2048)&&position<=9007199254740991.0&&
        (double)(long)position==position;
}
static int same_column(const WenaCard *card,const char *board,const char *list,
    const char *lane)
{
    return !strcmp(card->board_id,board) && !strcmp(card->list_id,list) &&
        !strcmp(card->swimlane_id,lane);
}
static int compare_slots(const void *left,const void *right)
{
    const WenaCardOrderSlot *a,*b;
    a=(const WenaCardOrderSlot *)left;b=(const WenaCardOrderSlot *)right;
    return a->position<b->position ? -1 : a->position>b->position ? 1 : 0;
}
int wena_card_order_capture(const WenaCard *cards,size_t card_count,
    const char *board,const char *list,const char *lane,
    WenaCardOrderSlot **slots,size_t *slot_count)
{
    WenaCardOrderSlot *candidate;
    size_t i,j,count;
    const WenaCard *card;
    if (!slots || !slot_count || (!cards && card_count) || card_count>WENA_CARD_ORDER_CAPACITY ||
        !wena_model_identifier_valid(board) || !wena_model_identifier_valid(list) ||
        !wena_model_identifier_valid(lane)) return 0;
    count=0;
    for (i=0;i<card_count;++i)
        if (same_column(&cards[i],board,list,lane)) ++count;
    if (!count) { free(*slots);*slots=NULL;*slot_count=0;return 1; }
    candidate=(WenaCardOrderSlot *)calloc(count,sizeof(*candidate));
    if (!candidate) return 0;
    count=0;
    for (i=0;i<card_count;++i) {
        card=&cards[i];
        if (!same_column(card,board,list,lane)) continue;
        if (!position_valid(card->sort) ||
            (card->archived!=0 && card->archived!=1) ||
            !wena_model_identifier_valid(card->id)) goto bad;
        strcpy(candidate[count].id,card->id);
        candidate[count].position=card->sort;
        candidate[count].model_index=i;candidate[count].archived=card->archived;
        ++count;
    }
    qsort(candidate,count,sizeof(*candidate),compare_slots);
    for (i=0;i<count;++i) {
        if (i && candidate[i-1].position==candidate[i].position) goto bad;
        for (j=0;j<i;++j)
            if (!strcmp(candidate[i].id,candidate[j].id)) goto bad;
    }
    free(*slots);*slots=candidate;*slot_count=count;
    return 1;
bad:
    free(candidate);return 0;
}
int wena_card_order_current(const WenaCardOrderSlot *slots,size_t slot_count,
    const WenaCard *cards,size_t card_count,const char *board,const char *list,
    const char *lane)
{
    unsigned char seen[WENA_CARD_ORDER_CAPACITY];
    size_t i,low,high,middle,count;
    const WenaCard *card;
    if ((!slots && slot_count) || (!slot_count && slots) || slot_count>WENA_CARD_ORDER_CAPACITY ||
        (!cards && card_count) || card_count>WENA_CARD_ORDER_CAPACITY || !wena_model_identifier_valid(board) ||
        !wena_model_identifier_valid(list) || !wena_model_identifier_valid(lane)) return 0;
    memset(seen,0,sizeof(seen));count=0;
    for (i=0;i<card_count;++i) {
        card=&cards[i];
        if (!same_column(card,board,list,lane)) continue;
        low=0;high=slot_count;
        while (low<high) {
            middle=low+(high-low)/2;
            if (slots[middle].position<card->sort) low=middle+1;
            else high=middle;
        }
        if (low==slot_count || seen[low] || slots[low].position!=card->sort ||
            slots[low].archived!=card->archived || strcmp(slots[low].id,card->id)) return 0;
        seen[low]=1;++count;
    }
    return count==slot_count;
}

int wena_card_order_insert_selection(const WenaCardOrderSlot *slots,size_t slot_count,
    const WenaId *selected,size_t selected_count,size_t before,
    WenaId **ordered,size_t *ordered_count)
{
    unsigned char moving[WENA_CARD_ORDER_CAPACITY];
    WenaId *candidate;size_t i,j,count,out;
    if(!ordered||!ordered_count||(!slots&&slot_count)||slot_count>WENA_CARD_ORDER_CAPACITY||
        !selected||!selected_count||selected_count>WENA_CARD_ORDER_CAPACITY||before>slot_count)return 0;
    for(i=0;i<selected_count;++i){
        if(!wena_model_identifier_valid(selected[i]))return 0;
        for(j=0;j<i;++j)if(!strcmp(selected[i],selected[j]))return 0;
    }
    memset(moving,0,sizeof(moving));count=selected_count;
    for(i=0;i<slot_count;++i){
        if(!wena_model_identifier_valid(slots[i].id)||!position_valid(slots[i].position)||
            (slots[i].archived!=0&&slots[i].archived!=1)||
            (i&&slots[i-1].position>=slots[i].position))return 0;
        for(j=0;j<i;++j)if(!strcmp(slots[i].id,slots[j].id))return 0;
        for(j=0;j<selected_count;++j)if(!strcmp(slots[i].id,selected[j])){moving[i]=1;break;}
        if(moving[i]){if(slots[i].archived)return 0;}
        else if(++count>WENA_CARD_ORDER_CAPACITY)return 0;
    }
    candidate=(WenaId*)calloc(count,sizeof(*candidate));if(!candidate)return 0;
    out=0;
    for(i=0;i<=slot_count;++i){
        if(i==before)for(j=0;j<selected_count;++j)strcpy(candidate[out++],selected[j]);
        if(i<slot_count&&!moving[i])strcpy(candidate[out++],slots[i].id);
    }
    free(*ordered);*ordered=candidate;*ordered_count=count;return 1;
}
