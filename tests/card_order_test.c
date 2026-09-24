#include "../models/card_order.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void insertion(void)
{
    WenaCardOrderSlot slots[4],*full;WenaId selected[3],*ordered,*prior,*many;
    size_t i,count,boundary,mask,n,j,k,at;
    memset(slots,0,sizeof(slots));
    for(i=0;i<4;++i){sprintf(slots[i].id,"c%lu",(unsigned long)i);slots[i].position=(double)(i*10);slots[i].model_index=3-i;}
    ordered=NULL;count=0;
    /* Every nonempty subset, reversed block order, every original boundary.
     * Test identities/order independently: block contiguous; untouched siblings
     * stay ordered, and only untouched predecessors appear before the block. */
    for(mask=1;mask<16;++mask){
        many=(WenaId*)calloc(4,sizeof(*many));assert(many);n=0;
        for(i=4;i>0;--i)if(mask&(1u<<(i-1)))strcpy(many[n++],slots[i-1].id);
        for(boundary=0;boundary<=4;++boundary){
            assert(wena_card_order_insert_selection(slots,4,(const WenaId*)many,n,boundary,&ordered,&count)&&count==4);
            at=0;for(i=0;i<boundary;++i)if(!(mask&(1u<<i)))++at;
            for(i=0;i<n;++i)assert(!strcmp(ordered[at+i],many[i]));
            k=0;for(i=0;i<4;++i)if(!(i>=at&&i<at+n)){
                while(mask&(1u<<k))++k;
                assert(!strcmp(ordered[i],slots[k++].id));
            }
            for(i=0;i<count;++i)for(j=0;j<i;++j)assert(strcmp(ordered[i],ordered[j]));
        }
        free(many);
    }
    strcpy(selected[0],"incoming");strcpy(selected[1],"c2");slots[1].archived=1;
    assert(wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,2,&ordered,&count)&&count==5);
    assert(!strcmp(ordered[0],"c0")&&!strcmp(ordered[1],"c1")&&!strcmp(ordered[2],"incoming")&&!strcmp(ordered[3],"c2")&&!strcmp(ordered[4],"c3"));
    prior=ordered;
    strcpy(selected[1],"c1");assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,0,&ordered,&count));
    strcpy(selected[1],"incoming");assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,0,&ordered,&count));
    strcpy(selected[1],"bad id");assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,0,&ordered,&count));
    strcpy(selected[1],"c2");slots[2].position=10;
    assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,0,&ordered,&count));slots[2].position=20;
    slots[2].position=20.5;assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,0,&ordered,&count));slots[2].position=20;
    assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,2,5,&ordered,&count));
    assert(!wena_card_order_insert_selection(NULL,4,(const WenaId*)selected,2,0,&ordered,&count));
    assert(!wena_card_order_insert_selection(slots,4,NULL,2,0,&ordered,&count));
    assert(!wena_card_order_insert_selection(slots,4,(const WenaId*)selected,0,0,&ordered,&count));
    assert(ordered==prior&&count==5&&!strcmp(ordered[2],"incoming"));
    /* Empty destination and input/output aliasing. */
    assert(wena_card_order_insert_selection(NULL,0,(const WenaId*)ordered,count,0,&ordered,&count)&&count==5);
    assert(!strcmp(ordered[2],"incoming"));
    full=(WenaCardOrderSlot*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*full));
    many=(WenaId*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*many));assert(full&&many);
    for(i=0;i<WENA_CARD_ORDER_CAPACITY;++i){sprintf(full[i].id,"f%lu",(unsigned long)i);full[i].position=(double)i;strcpy(many[WENA_CARD_ORDER_CAPACITY-i-1],full[i].id);}
    assert(wena_card_order_insert_selection(full,WENA_CARD_ORDER_CAPACITY,(const WenaId*)many,WENA_CARD_ORDER_CAPACITY,WENA_CARD_ORDER_CAPACITY,&ordered,&count));
    assert(count==WENA_CARD_ORDER_CAPACITY&&!strcmp(ordered[0],"f2047")&&!strcmp(ordered[2047],"f0"));
    prior=ordered;
    assert(!wena_card_order_insert_selection(full,WENA_CARD_ORDER_CAPACITY,(const WenaId*)selected,1,0,&ordered,&count));
    assert(!wena_card_order_insert_selection(full,WENA_CARD_ORDER_CAPACITY+1,(const WenaId*)many,1,0,&ordered,&count));
    assert(!wena_card_order_insert_selection(full,1,(const WenaId*)many,WENA_CARD_ORDER_CAPACITY+1,0,&ordered,&count));
    assert(ordered==prior&&count==WENA_CARD_ORDER_CAPACITY&&!strcmp(ordered[0],"f2047"));
    free(many);free(full);free(ordered);
}
int main(void)
{
    WenaCard cards[4];
    WenaCardOrderSlot *slots,*before;
    size_t count;
    insertion();
    assert(wena_card_init(&cards[0],"later","b","s","l","Same title",8,0));
    assert(wena_card_init(&cards[1],"first","b","s","l","Same title",0,0));
    assert(wena_card_init(&cards[2],"archived","b","s","l","Archived",3,1));
    assert(wena_card_init(&cards[3],"other","b","s","other","Other",1,0));
    slots=NULL;count=0;
    assert(wena_card_order_capture(cards,4,"b","l","s",&slots,&count));
    assert(count==3&&!strcmp(slots[0].id,"first")&&slots[0].model_index==1);
    assert(slots[1].archived&&!strcmp(slots[1].id,"archived"));
    assert(wena_card_order_current(slots,count,cards,4,"b","l","s"));
    strcpy(cards[0].title,"Renamed");cards[3].sort=123;
    assert(wena_card_order_current(slots,count,cards,4,"b","l","s"));
    cards[2].archived=0;
    assert(!wena_card_order_current(slots,count,cards,4,"b","l","s"));cards[2].archived=1;
    before=slots;cards[0].sort=3;
    assert(!wena_card_order_capture(cards,4,"b","l","s",&slots,&count)&&slots==before&&count==3);
    assert(!wena_card_order_current(slots,count,cards,4,"b","l","s"));
    cards[0].sort=8.5;assert(!wena_card_order_capture(cards,4,"b","l","s",&slots,&count));
    cards[0].sort=8;strcpy(cards[0].id,"first");
    assert(!wena_card_order_capture(cards,4,"b","l","s",&slots,&count)&&slots==before);
    strcpy(cards[0].id,"later");
    assert(!wena_card_order_capture(cards,2049,"b","l","s",&slots,&count));
    assert(!wena_card_order_current(slots,count,cards,2,"b","l","s"));
    assert(!wena_card_order_current(slots,count,cards,4,"foreign","l","s"));
    assert(wena_card_order_capture(cards,4,"b","other","s",&slots,&count));
    assert(count==1&&!strcmp(slots[0].id,"other"));
    assert(wena_card_order_capture(cards,4,"b","missing","s",&slots,&count));
    assert(!slots&&!count&&wena_card_order_current(slots,count,cards,4,"b","missing","s"));
    strcpy(cards[0].list_id,"missing");
    assert(!wena_card_order_current(slots,count,cards,4,"b","missing","s"));
    assert(wena_card_order_capture(NULL,0,"b","l","s",&slots,&count));
    assert(wena_card_order_current(slots,count,NULL,0,"b","l","s"));
    free(slots);
    puts("Shared card order: exact IDs, archived slots, scope, stale detection and atomic replacement passed");
    return 0;
}
