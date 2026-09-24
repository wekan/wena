#include "../models/card_selection.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    WenaCardSelection *s,*before;WenaCard *cards;size_t i;char id[32];WenaId order[3];
    s=(WenaCardSelection*)malloc(sizeof(*s));before=(WenaCardSelection*)malloc(sizeof(*before));
    cards=(WenaCard*)calloc(WENA_CARD_SELECTION_CAPACITY,sizeof(*cards));assert(s&&before&&cards);
    assert(wena_card_selection_init(s,"b"));
    assert(wena_card_init(&cards[0],"a","b","lane","list","Repeated",0,0));
    assert(wena_card_init(&cards[1],"b","b","other","list","Repeated",1,0));
    assert(wena_card_init(&cards[2],"c","b","lane","list","Archived",2,1));
    assert(wena_card_init(&cards[3],"d","foreign","lane","list","Foreign",3,0));
    assert(wena_card_init(&cards[4],"e","b","lane","elsewhere","Elsewhere",4,0));
    assert(wena_card_selection_add(s,cards,5,"list","lane")&&s->count==1&&!strcmp(s->ids[0],"a"));
    assert(wena_card_selection_add(s,cards,5,"list",NULL)&&s->count==2&&!strcmp(s->ids[1],"b"));
    assert(wena_card_selection_add(s,cards,5,"list","")&&s->count==2);
    assert(wena_card_selection_toggle(s,cards,5,"a")&&s->count==1&&!strcmp(s->ids[0],"b"));
    assert(wena_card_selection_toggle(s,cards,5,"e")&&s->count==2);
    strcpy(order[0],"e");strcpy(order[1],"a");strcpy(order[2],"b");
    assert(wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"e","b")&&s->count==3);
    assert(!strcmp(s->ids[0],"e")&&!strcmp(s->ids[1],"a")&&!strcmp(s->ids[2],"b"));
    assert(wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"b","e")&&s->count==3);
    assert(wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"a","a")&&s->count==1&&!strcmp(s->ids[0],"a"));
    memcpy(before,s,sizeof(*s));
    assert(!wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"missing","b")&&!memcmp(s,before,sizeof(*s)));
    assert(!wena_card_selection_range(s,cards,5,(const WenaId*)order,0,"a","b")&&!memcmp(s,before,sizeof(*s)));
    assert(!wena_card_selection_range(s,cards,5,(const WenaId*)order,6,"a","b")&&!memcmp(s,before,sizeof(*s)));
    strcpy(order[0],"c");assert(!wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"a","b")&&!memcmp(s,before,sizeof(*s)));
    strcpy(order[0],"d");assert(!wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"a","b")&&!memcmp(s,before,sizeof(*s)));
    strcpy(order[0],"a");assert(!wena_card_selection_range(s,cards,5,(const WenaId*)order,3,"a","b")&&!memcmp(s,before,sizeof(*s)));
    /* A filtered display omits a middle card even if its model is active. */
    strcpy(order[0],"e");strcpy(order[1],"b");
    assert(wena_card_selection_range(s,cards,5,(const WenaId*)order,2,"e","b")&&s->count==2&&!wena_card_selection_contains(s,"a"));
    assert(wena_card_selection_range(s,cards,5,(const WenaId*)s->ids,s->count,s->ids[1],s->ids[0])&&s->count==2);
    /* Restore the preceding toggle fixture for stale-state checks. */
    wena_card_selection_clear(s);
    assert(wena_card_selection_toggle(s,cards,5,"b")&&wena_card_selection_toggle(s,cards,5,"e"));

    memcpy(before,s,sizeof(*s));
    assert(!wena_card_selection_toggle(s,cards,5,"c")&&!memcmp(before,s,sizeof(*s)));
    assert(!wena_card_selection_toggle(s,cards,5,"d")&&!memcmp(before,s,sizeof(*s)));
    assert(!wena_card_selection_toggle(s,cards,5,"missing")&&!memcmp(before,s,sizeof(*s)));
    assert(!wena_card_selection_add(s,cards,5,"list","bad/id")&&!memcmp(before,s,sizeof(*s)));
    assert(!wena_card_selection_sync(s,NULL,1)&&!memcmp(before,s,sizeof(*s)));
    assert(!wena_card_selection_sync(s,cards,WENA_CARD_SELECTION_CAPACITY+1)&&!memcmp(before,s,sizeof(*s)));
    cards[0].archived=2;assert(!wena_card_selection_sync(s,cards,5)&&!memcmp(before,s,sizeof(*s)));cards[0].archived=0;
    strcpy(cards[0].id,"b");assert(!wena_card_selection_sync(s,cards,5)&&!memcmp(before,s,sizeof(*s)));strcpy(cards[0].id,"a");
    strcpy(s->ids[1],s->ids[0]);memcpy(before,s,sizeof(*s));
    assert(!wena_card_selection_sync(s,cards,5)&&!memcmp(before,s,sizeof(*s)));
    strcpy(s->ids[1],"e");s->count=WENA_CARD_SELECTION_CAPACITY+1;memcpy(before,s,sizeof(*s));
    assert(!wena_card_selection_sync(s,cards,5)&&!memcmp(before,s,sizeof(*s)));
    s->count=2;
    cards[1].archived=1;assert(wena_card_selection_sync(s,cards,5)&&s->count==1&&!strcmp(s->ids[0],"e"));
    assert(wena_card_selection_sync(s,cards,4)&&s->count==0);
    assert(wena_card_selection_init(s,"foreign"));
    assert(wena_card_selection_add(s,cards,5,"list",NULL)&&s->count==1&&!strcmp(s->ids[0],"d"));
    memcpy(before,s,sizeof(*s));assert(!wena_card_selection_init(s,"bad/id")&&!memcmp(before,s,sizeof(*s)));
    assert(wena_card_selection_init(s,s->board_id)&&!s->count&&!strcmp(s->board_id,"foreign"));
    assert(wena_card_selection_init(s,"b"));
    for(i=0;i<WENA_CARD_SELECTION_CAPACITY;++i){
        sprintf(id,"card%lu",(unsigned long)i);
        assert(wena_card_init(&cards[i],id,"b","lane","list","Repeated",(double)i,0));
    }
    assert(wena_card_selection_add(s,cards,WENA_CARD_SELECTION_CAPACITY,"list",NULL)&&s->count==WENA_CARD_SELECTION_CAPACITY);
    assert(wena_card_selection_add(s,cards,WENA_CARD_SELECTION_CAPACITY,"list","lane")&&s->count==WENA_CARD_SELECTION_CAPACITY);
    assert(wena_card_selection_toggle(s,cards,WENA_CARD_SELECTION_CAPACITY,"card0")&&s->count==WENA_CARD_SELECTION_CAPACITY-1);
    assert(wena_card_selection_toggle(s,cards,WENA_CARD_SELECTION_CAPACITY,"card0")&&s->count==WENA_CARD_SELECTION_CAPACITY);
    assert(wena_card_selection_range(s,cards,WENA_CARD_SELECTION_CAPACITY,(const WenaId*)s->ids,s->count,
        s->ids[s->count-1],s->ids[0])&&s->count==WENA_CARD_SELECTION_CAPACITY);
    assert(wena_card_selection_sync(s,NULL,0)&&!s->count);
    wena_card_selection_clear(s);assert(!s->count&&!strcmp(s->board_id,"b"));
    free(cards);free(before);free(s);puts("Scoped card selection, union, toggle, pruning and capacity passed");return 0;
}
