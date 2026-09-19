#include "../client/features/card_description.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

typedef struct Store { char text[1026]; unsigned long version; int calls; int fail; } Store;
static int load(void *data,const char *board,const char *card,char *text,size_t capacity,unsigned long *version)
{
    Store *s=(Store*)data;assert(!strcmp(board,"board")&&!strcmp(card,"card"));
    if(s->fail||strlen(s->text)>=capacity)return 0;
    strcpy(text,s->text);*version=s->version;return 1;
}
static int save(void *data,const char *board,const char *card,unsigned long version,const char *text)
{
    Store *s=(Store*)data;++s->calls;assert(!strcmp(board,"board")&&!strcmp(card,"card"));
    if(s->fail||version!=s->version)return 0;
    strcpy(s->text,text);++s->version;return 1;
}
static void frame(WenaCardDescriptionState *state,WenaCard *card,const char *button,const char *input)
{
    struct nk_context c;memset(&c,0,sizeof(c));c.button_to_press=button;c.edit_text=input;
    assert(wena_card_description_render(&c,state,card,1,800,600));assert(c.begin_count==c.end_count);
}
int main(void)
{
    WenaCard card, duplicates[2];WenaCardDescriptionState state;Store store;char oversized[1100];struct nk_context context;
    assert(wena_card_init(&card,"card","board","lane","list","Card",0,0));
    memset(&store,0,sizeof(store));memset(&context,0,sizeof(context));store.version=1;
    strcpy(store.text,"  # Heading\n\nLine\tvalue\r\n  ");
    wena_card_description_init(&state,load,save,&store);
    assert(wena_card_description_open(&state,&card));assert(!strcmp(state.input,store.text));
    frame(&state,&card,"Cancel","Discard");assert(!state.visible&&store.calls==0);
    assert(wena_card_description_open(&state,&card));frame(&state,&card,"Save","  **plain markdown**\nSecond\tline\r\n");
    assert(!state.visible&&store.calls==1&&!strcmp(store.text,"  **plain markdown**\nSecond\tline\r\n"));
    assert(wena_card_description_open(&state,&card));frame(&state,&card,"Save","");
    assert(!state.visible&&store.calls==2&&store.text[0]==0);
    assert(wena_card_description_open(&state,&card));memset(oversized,'x',sizeof(oversized));oversized[sizeof(oversized)-1]=0;
    frame(&state,&card,"Save",oversized);assert(state.error&&state.length==
        WENA_NATIVE_EDIT_CAPACITY(WENA_DESCRIPTION_CAPACITY) - 1&&store.calls==2);
    frame(&state,&card,"Save","Bad\001");assert(state.error&&store.calls==2);
    frame(&state,&card,"Save","Bad\177");assert(state.error&&store.calls==2);
    frame(&state,&card,"Save","Bad\302\205");assert(state.error&&store.calls==2);
    frame(&state,&card,"Save","Bad\300\257");assert(state.error&&store.calls==2);
    frame(&state,&card,"Save","Bad\355\240\200");assert(state.error&&store.calls==2);
    memcpy(state.input,"a\0b",3);state.length=3;frame(&state,&card,"Save",NULL);assert(store.calls==2);
    ++store.version;frame(&state,&card,"Save","Stale\n");assert(state.visible&&state.error&&store.calls==3&&store.text[0]==0);
    frame(&state,&card,"Cancel",NULL);assert(wena_card_description_open(&state,&card));
    store.fail=1;frame(&state,&card,"Save","Retained\n");assert(state.visible&&state.error&&!strcmp(state.input,"Retained\n"));
    store.fail=0;oversized[1024]=0;frame(&state,&card,"Save",oversized);assert(!state.visible&&strlen(store.text)==1024);
    assert(wena_card_description_open(&state,&card));frame(&state,&card,"Close details",NULL);assert(!state.visible);
    store.fail=1;assert(!wena_card_description_open(&state,&card));store.fail=0;
    wena_card_description_init(&state,load,NULL,&store);assert(wena_card_description_open(&state,&card));
    frame(&state,&card,NULL,"Readonly change");assert(state.length==1024);
    frame(&state,&card,"Cancel",NULL);assert(!state.visible&&strlen(store.text)==1024);
    assert(wena_card_description_open(&state,&card));strcpy(card.board_id,"wrong");
    assert(!wena_card_description_render(&context,&state,&card,1,800,600));assert(!state.visible);
    strcpy(card.board_id,"board");assert(wena_card_description_open(&state,&card));card.archived=1;
    assert(!wena_card_description_render(&context,&state,&card,1,800,600));assert(!state.visible);
    assert(!wena_card_description_open(&state,&card));card.archived=0;
    assert(wena_card_description_open(&state,&card));duplicates[0]=card;duplicates[1]=card;
    assert(!wena_card_description_render(&context,&state,duplicates,2,800,600)&&!state.visible);
    context.button_to_press="Description";assert(wena_card_details_canvas_render(&context,&card)==WENA_CARD_DETAILS_DESCRIPTION);
    return 0;
}
