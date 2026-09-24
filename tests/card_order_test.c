#include "../models/card_order.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    WenaCard cards[4];
    WenaCardOrderSlot *slots,*before;
    size_t count;
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
