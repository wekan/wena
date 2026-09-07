#include "../models/wekan_models.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    WenaCard card,other_card;
    WenaChecklist checklist,other,ordered[3];
    WenaChecklistItem items[8],saved[8],invalid,*maximum;
    WenaChecklistProgress progress,before;
    char title[130],id[65];
    size_t index;int shown;
    assert(wena_card_init(&card,"card","board","lane","list","Card",0,0));
    assert(wena_card_init(&other_card,"card-other","board","lane","list","Other",0,0));
    assert(wena_checklist_init(&checklist,"check","board","card","Checklist \303\244",0));
    assert(wena_checklist_valid(&checklist)&&wena_checklist_validate_parent(&checklist,&card));
    assert(!wena_checklist_validate_parent(&checklist,&other_card));
    other_card=card;strcpy(other_card.board_id,"other");assert(!wena_checklist_validate_parent(&checklist,&other_card));
    card.archived=1;assert(wena_checklist_validate_parent(&checklist,&card));card.archived=0;
    assert(!wena_checklist_init(&other,"bad/id","board","card","Title",0));assert(!other.id[0]);
    assert(!wena_checklist_init(&other,"id","","card","Title",0));
    assert(!wena_checklist_init(&other,"id","board","","Title",0));
    assert(!wena_checklist_init(&other,"id","board","card","",0));
    assert(!wena_checklist_init(&other,"id","board","card","Bad\n",0));
    assert(!wena_checklist_init(&other,"id","board","card","\300\257",0));
    assert(!wena_checklist_init(&other,"id","board","card","\302\205",0));
    assert(!wena_checklist_init(&other,"id","board","card","Title",ULONG_MAX));
    memset(title,'x',sizeof(title));title[129]=0;assert(!wena_checklist_init(&other,"id","board","card",title,0));
    title[128]=0;assert(wena_checklist_init(&other,"id","board","card",title,WENA_CHECKLIST_POSITION_MAX));
    assert(strlen(other.title)==128);
    /* Re-initializing from the object's own strings must preserve them. */
    assert(wena_checklist_init(&other,other.id,other.board_id,other.card_id,other.title,1));assert(strlen(other.title)==128);
    assert(wena_checklist_shown_at_minicard(&checklist,1,&shown)&&shown==1);
    assert(wena_checklist_shown_at_minicard(&checklist,0,&shown)&&shown==0);
    checklist.show_on_minicard=WENA_CHECKLIST_MINICARD_HIDE;
    assert(wena_checklist_shown_at_minicard(&checklist,1,&shown)&&shown==0);
    checklist.show_on_minicard=WENA_CHECKLIST_MINICARD_SHOW;
    assert(wena_checklist_shown_at_minicard(&checklist,0,&shown)&&shown==1);
    checklist.show_on_minicard=(WenaChecklistMinicard)2;shown=99;
    assert(!wena_checklist_shown_at_minicard(&checklist,1,&shown)&&shown==99);
    checklist.show_on_minicard=WENA_CHECKLIST_MINICARD_INHERIT;
    assert(!wena_checklist_shown_at_minicard(&checklist,2,&shown));
    assert(wena_checklist_progress(&checklist,NULL,0,&progress));
    assert(progress.total==0&&progress.finished==0&&progress.percent==0&&!progress.all_items_finished&&!progress.is_finished);
    checklist.hide_all_items=1;assert(wena_checklist_progress(&checklist,NULL,0,&progress));
    assert(progress.is_finished&&!progress.all_items_finished&&progress.percent==0);checklist.hide_all_items=0;
    for(index=0;index<8;++index){sprintf(id,"item-%lu",(unsigned long)index);assert(wena_checklist_item_init(&items[index],id,"board","card","check","Item",(unsigned long)index,index==0));}
    assert(wena_checklist_item_validate_parent(&items[0],&checklist));
    assert(wena_checklist_progress(&checklist,items,8,&progress));
    assert(progress.total==8&&progress.finished==1&&progress.percent==13&&!progress.is_finished);
    memcpy(saved,items,sizeof(items));checklist.hide_checked_items=1;checklist.hide_all_items=1;
    assert(wena_checklist_progress(&checklist,items,8,&progress));
    assert(progress.finished==1&&progress.percent==13&&!progress.all_items_finished&&progress.is_finished);
    assert(!memcmp(saved,items,sizeof(items)));checklist.hide_all_items=0;
    for(index=0;index<8;++index)assert(wena_checklist_item_set_finished(&items[index],&checklist,1));
    assert(wena_checklist_progress(&checklist,items,8,&progress));assert(progress.percent==100&&progress.finished==8&&progress.all_items_finished&&progress.is_finished);
    assert(wena_checklist_item_set_finished(&items[0],&checklist,0));
    assert(wena_checklist_progress(&checklist,items,8,&progress));assert(progress.percent==88&&!progress.is_finished);
    assert(!wena_checklist_item_set_finished(&items[0],&checklist,2)&&items[0].is_finished==0);
    memset(&before,0xa5,sizeof(before));progress=before;
    invalid=items[0];strcpy(invalid.board_id,"wrong");assert(!wena_checklist_item_validate_parent(&invalid,&checklist));
    assert(!wena_checklist_progress(&checklist,&invalid,1,&progress)&&!memcmp(&progress,&before,sizeof(progress)));
    invalid=items[0];strcpy(invalid.card_id,"wrong");assert(!wena_checklist_item_validate_parent(&invalid,&checklist));
    invalid=items[0];strcpy(invalid.checklist_id,"wrong");assert(!wena_checklist_item_validate_parent(&invalid,&checklist));
    assert(!wena_checklist_item_set_finished(&invalid,&checklist,1)&&invalid.is_finished==0);
    invalid=items[0];invalid.is_finished=-1;assert(!wena_checklist_item_valid(&invalid));
    assert(!wena_checklist_item_init(&invalid,"item","board","card","check","Title",0,2));
    assert(!wena_checklist_item_init(&invalid,"item","board","card","","Title",0,0));
    assert(!wena_checklist_item_init(&invalid,"item","board","card","check","Bad\177",0,0));
    assert(!wena_checklist_item_init(&invalid,"item","board","card","check","Title",ULONG_MAX,0));
    memcpy(saved,items,sizeof(items));items[1]=items[0];
    assert(!wena_checklist_progress(&checklist,items,8,&progress)&&!memcmp(&progress,&before,sizeof(progress)));memcpy(items,saved,sizeof(items));
    checklist.hide_checked_items=2;assert(!wena_checklist_progress(&checklist,items,8,&progress));checklist.hide_checked_items=0;
    assert(!wena_checklist_progress(&checklist,items,WENA_CHECKLIST_MAX_ITEMS+1,&progress));
    assert(!wena_checklist_progress(&checklist,NULL,1,&progress));
    assert(wena_checklist_init(&ordered[0],"z","board","card","Last",2));
    assert(wena_checklist_init(&ordered[1],"b","board","card","Tie B",0));
    assert(wena_checklist_init(&ordered[2],"a","board","card","Tie A",0));
    qsort(ordered,3,sizeof(ordered[0]),wena_checklist_compare);assert(!strcmp(ordered[0].id,"a")&&!strcmp(ordered[1].id,"b")&&!strcmp(ordered[2].id,"z"));
    items[0].position=7;items[7].position=0;qsort(items,8,sizeof(items[0]),wena_checklist_item_compare);
    for(index=1;index<8;++index)assert(wena_checklist_item_compare(&items[index-1],&items[index])<=0);
    maximum=(WenaChecklistItem*)malloc(WENA_CHECKLIST_MAX_ITEMS*sizeof(*maximum));assert(maximum);
    for(index=0;index<WENA_CHECKLIST_MAX_ITEMS;++index){sprintf(id,"max-%lu",(unsigned long)index);assert(wena_checklist_item_init(&maximum[index],id,"board","card","check","Item",(unsigned long)index,(int)(index%2)));}
    assert(wena_checklist_progress(&checklist,maximum,WENA_CHECKLIST_MAX_ITEMS,&progress));assert(progress.total==1024&&progress.finished==512&&progress.percent==50);free(maximum);
    puts("checklist and checklist-item model tests passed");return 0;
}
