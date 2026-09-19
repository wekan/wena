#include "../client/features/checklists.h"
#include <nuklear.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static WenaChecklistSnapshot store;
static int writes,fail_load,fail_save;
static WenaChecklistEdit last;
static char last_title[129];
static int load(void*c,const char*b,const char*id,WenaChecklistSnapshot*out)
{(void)c;assert(!strcmp(b,"b")&&!strcmp(id,"c"));if(fail_load)return 0;*out=store;return 1;}
static int save(void*c,const char*b,const char*id,const WenaChecklistEdit*e)
{(void)c;assert(!strcmp(b,"b")&&!strcmp(id,"c"));++writes;last=*e;if(e->title)strcpy(last_title,e->title);if(fail_save||e->expected_card_version!=store.card_version)return 0;++store.card_version;if(e->action==WENA_CHECKLIST_RENAME)strcpy(store.checklists[0].title,e->title);if(e->action==WENA_CHECKLIST_SET_FINISHED)store.items[0].is_finished=e->is_finished;if(e->action==WENA_CHECKLIST_SET_FLAGS){store.checklists[0].hide_checked_items=e->hide_checked_items;store.checklists[0].hide_all_items=e->hide_all_items;store.checklists[0].show_on_minicard=e->show_on_minicard;}if(e->action==WENA_CHECKLIST_DELETE_ITEM){size_t i;for(i=0;i<store.item_count;++i)if(!strcmp(store.items[i].id,e->item_id)){if(i+1<store.item_count){memmove(&store.items[i],&store.items[i+1],(store.item_count-i-1)*sizeof(store.items[0]));memmove(&store.item_versions[i],&store.item_versions[i+1],(store.item_count-i-1)*sizeof(store.item_versions[0]));}--store.item_count;break;}}if(e->action==WENA_CHECKLIST_DELETE){store.checklist_count=0;store.item_count=0;}return 1;}
static void frame(WenaChecklistsState*s,WenaCard*c,const char*b,const char*t)
{struct nk_context n;memset(&n,0,sizeof(n));n.button_to_press=b;n.edit_text=t;assert(wena_checklists_render(&n,s,c,1,800,600));assert(n.begin_count==n.end_count);}
int main(void)
{
 WenaChecklistsState s;WenaCard c;char big[140];struct nk_context ctx;
 memset(&store,0,sizeof(store));strcpy(store.board_id,"b");strcpy(store.card_id,"c");store.card_version=1;store.checklist_count=1;store.item_count=1;store.checklist_versions[0]=3;store.item_versions[0]=4;
 assert(wena_checklist_init(&store.checklists[0],"cl","b","c","Checklist",0));assert(wena_checklist_item_init(&store.items[0],"it","b","c","cl","Item",0,0));assert(wena_card_init(&c,"c","b","s","l","Card",0,0));
 wena_checklists_init(&s,load,save,NULL);assert(wena_checklists_open(&s,&c));
 frame(&s,&c,"Add Checklist",NULL);assert(s.action==WENA_CHECKLIST_CREATE);frame(&s,&c,"Cancel",NULL);assert(!s.action&&!writes);
 frame(&s,&c,"Add Checklist",NULL);frame(&s,&c,"Save","");assert(s.error&&!writes);
 memset(big,'x',sizeof(big));big[sizeof(big)-1]=0;frame(&s,&c,"Save",big);assert(s.length==132&&!writes);frame(&s,&c,"Save","Bad\n");assert(!writes);frame(&s,&c,"Save","Valid");assert(writes==1&&!s.action&&last.action==WENA_CHECKLIST_CREATE);
 frame(&s,&c,"Rename",NULL);assert(s.action==WENA_CHECKLIST_RENAME);frame(&s,&c,"Save","Renamed");assert(!strcmp(s.snapshot->checklists[0].title,"Renamed")&&last.expected_checklist_version==3);
 frame(&s,&c,"Add an item to checklist",NULL);frame(&s,&c,"Save","  Item trim  ");assert(last.action==WENA_CHECKLIST_ADD_ITEM&&!strcmp(last_title,"Item trim"));
 frame(&s,&c,"Edit",NULL);assert(s.action==WENA_CHECKLIST_RENAME_ITEM);fail_save=1;frame(&s,&c,"Save","Retained");assert(s.error&&s.action&&!strcmp(s.input,"Retained"));fail_save=0;frame(&s,&c,"Cancel",NULL);
 frame(&s,&c,"Complete",NULL);assert(s.action==WENA_CHECKLIST_SET_FINISHED&&s.is_finished);frame(&s,&c,"Save",NULL);assert(s.snapshot->items[0].is_finished&&last.expected_item_version==4);
 frame(&s,&c,"Add Checklist",NULL);fail_load=1;frame(&s,&c,"Save","Committed");assert(s.needs_refresh&&s.error&&!s.action);{int n=writes;frame(&s,&c,"Save",NULL);frame(&s,&c,"Refresh",NULL);assert(writes==n&&s.needs_refresh);fail_load=0;frame(&s,&c,"Refresh",NULL);assert(writes==n&&!s.needs_refresh);}
 frame(&s,&c,"Checklist Actions",NULL);assert(s.action==WENA_CHECKLIST_SET_FLAGS);s.hide_all_items=1;s.hide_checked_items=1;frame(&s,&c,"Cancel",NULL);assert(!s.snapshot->checklists[0].hide_all_items);
 frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=1;fail_save=1;frame(&s,&c,"Save",NULL);assert(s.error&&s.action&&s.hide_all_items&&!s.snapshot->checklists[0].hide_all_items);fail_save=0;frame(&s,&c,"Save",NULL);assert(!s.error&&s.snapshot->checklists[0].hide_all_items&&last.show_on_minicard==WENA_CHECKLIST_MINICARD_INHERIT);
 frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=3;{int n=writes;frame(&s,&c,"Save",NULL);assert(s.error&&writes==n);}frame(&s,&c,"Cancel",NULL);
 frame(&s,&c,"Close details",NULL);assert(!s.visible&&!s.snapshot);wena_checklists_close(&s);
 {int mode;for(mode=-1;mode<=1;++mode){store.checklists[0].show_on_minicard=(WenaChecklistMinicard)mode;assert(wena_checklists_open(&s,&c));frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=!s.hide_all_items;frame(&s,&c,"Save",NULL);assert(!s.error&&last.show_on_minicard==mode&&s.snapshot->checklists[0].show_on_minicard==mode);wena_checklists_close(&s);}}
 store.checklists[0].hide_all_items=0;store.checklists[0].hide_checked_items=0;store.item_count=2;store.item_versions[1]=4;assert(wena_checklist_item_init(&store.items[1],"it2","b","c","cl",store.items[0].title,18,0));
 assert(wena_checklists_open(&s,&c));frame(&s,&c,"Edit",NULL);frame(&s,&c,"Delete","Invalid\001 draft");assert(s.action==WENA_CHECKLIST_DELETE_ITEM&&!strcmp(s.input,store.items[0].title));{int n=writes;frame(&s,&c,"Cancel",NULL);assert(writes==n&&store.item_count==2);}
 frame(&s,&c,"Edit",NULL);frame(&s,&c,"Delete",NULL);fail_save=1;frame(&s,&c,"Delete",NULL);assert(s.error&&s.action==WENA_CHECKLIST_DELETE_ITEM&&store.item_count==2);fail_save=0;frame(&s,&c,"Delete",NULL);assert(!s.action&&store.item_count==1&&!strcmp(store.items[0].id,"it2"));
 frame(&s,&c,"Rename",NULL);frame(&s,&c,"Delete","Discard this rename");assert(s.action==WENA_CHECKLIST_DELETE&&!strcmp(s.input,store.checklists[0].title));{int n=writes;frame(&s,&c,"Cancel",NULL);assert(writes==n&&store.checklist_count==1);}
 frame(&s,&c,"Rename",NULL);frame(&s,&c,"Delete",NULL);fail_load=1;frame(&s,&c,"Delete",NULL);assert(s.needs_refresh&&!s.action&&!store.checklist_count&&!store.item_count);{int n=writes;frame(&s,&c,"Delete",NULL);assert(writes==n);fail_load=0;frame(&s,&c,"Refresh",NULL);assert(writes==n&&!s.error&&!s.needs_refresh&&!s.snapshot->checklist_count);}wena_checklists_close(&s);
 store.checklist_count=1;store.item_count=1;assert(wena_checklist_item_init(&store.items[0],"it","b","c","cl","Item",0,0));
 wena_checklists_init(&s,load,NULL,NULL);assert(wena_checklists_open(&s,&c));frame(&s,&c,"Add Checklist",NULL);assert(!s.action);frame(&s,&c,"Complete",NULL);assert(!s.action);strcpy(c.board_id,"wrong");memset(&ctx,0,sizeof(ctx));assert(!wena_checklists_render(&ctx,&s,&c,1,800,600)&&!s.visible);strcpy(c.board_id,"b");
 memset(store.board_id,'x',sizeof(store.board_id));assert(!wena_checklists_open(&s,&c));strcpy(store.board_id,"b");
 store.card_version=(unsigned long)LONG_MAX;assert(!wena_checklists_open(&s,&c));store.card_version=8;
 store.checklist_versions[0]=(unsigned long)LONG_MAX;assert(!wena_checklists_open(&s,&c));store.checklist_versions[0]=3;
 store.item_versions[0]=(unsigned long)LONG_MAX;assert(!wena_checklists_open(&s,&c));store.item_versions[0]=4;
 store.checklists[0].position=20;store.items[0].position=17;assert(wena_checklists_open(&s,&c));wena_checklists_close(&s);
 store.item_count=2;store.items[1]=store.items[0];store.item_versions[1]=4;assert(!wena_checklists_open(&s,&c));
 store.item_count=1025;assert(!wena_checklists_open(&s,&c));store.item_count=1;strcpy(store.items[0].checklist_id,"wrong");assert(!wena_checklists_open(&s,&c));return 0;
}
