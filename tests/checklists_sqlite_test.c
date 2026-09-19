#include "../client/features/checklists.h"
#include "../client/features/checklist_mutation.h"
#include "../imports/ui/page_contract.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3*d,const char*q){assert(sqlite3_exec(d,q,NULL,NULL,NULL)==SQLITE_OK);}
static void schema(sqlite3*d,const char*p){FILE*f;long n;char*s;f=fopen(p,"rb");assert(f);assert(!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0);rewind(f);s=(char*)malloc((size_t)n+1);assert(s);assert(fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);sql(d,s);free(s);}
static void frame(WenaChecklistsState*s,WenaCard*c,const char*b,const char*t)
{struct nk_context n;memset(&n,0,sizeof(n));n.button_to_press=b;n.edit_text=t;assert(wena_checklists_render(&n,s,c,1,800,600));}
static void choose(WenaChecklistsState*s,WenaCard*c,const char*option)
{struct nk_context n;memset(&n,0,sizeof(n));n.combo_item_to_press=option;assert(wena_checklists_render(&n,s,c,1,800,600));}
int main(int argc,char**argv)
{
 sqlite3*d;WenaChecklistsState s;WenaChecklistMutation a;WenaCard c;WenaChecklistEdit e;
 assert(argc==5);assert(sqlite3_open(argv[4],&d)==SQLITE_OK);sql(d,"PRAGMA foreign_keys=ON");schema(d,argv[1]);schema(d,argv[2]);schema(d,argv[3]);sql(d,"PRAGMA user_version=3");
 sql(d,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1)");
 assert(wena_card_init(&c,"c","b","s","l","Card",0,0));assert(wena_checklist_mutation_init(&a,d,"u","b"));wena_checklists_init(&s,wena_checklist_mutation_load,wena_checklist_mutation_save,&a);
 assert(wena_checklists_open(&s,&c));frame(&s,&c,"Add Checklist",NULL);frame(&s,&c,"Save","Tasks");assert(!s.error&&s.snapshot->checklist_count==1&&s.snapshot->card_version==2);
 frame(&s,&c,"Add an item to checklist",NULL);frame(&s,&c,"Save","  First item  ");assert(!s.error&&s.snapshot->item_count==1&&!strcmp(s.snapshot->items[0].title,"First item"));
 frame(&s,&c,"Complete",NULL);frame(&s,&c,"Cancel",NULL);assert(!s.snapshot->items[0].is_finished);frame(&s,&c,"Complete",NULL);frame(&s,&c,"Save",NULL);assert(!s.error&&s.snapshot->items[0].is_finished);
 frame(&s,&c,"Checklist Actions",NULL);assert(s.action==WENA_CHECKLIST_SET_FLAGS);s.hide_checked_items=1;s.hide_all_items=1;frame(&s,&c,"Cancel",NULL);assert(!s.snapshot->checklists[0].hide_all_items);
 frame(&s,&c,"Checklist Actions",NULL);s.hide_checked_items=1;s.hide_all_items=1;frame(&s,&c,"Save",NULL);assert(!s.error&&s.snapshot->checklists[0].hide_all_items&&s.snapshot->checklists[0].show_on_minicard==WENA_CHECKLIST_MINICARD_INHERIT);
 {unsigned long version=s.snapshot->card_version;frame(&s,&c,"Checklist Actions",NULL);frame(&s,&c,"Save",NULL);assert(!s.error&&s.snapshot->card_version==version);}
 frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=0;s.hide_checked_items=0;frame(&s,&c,"Save",NULL);assert(!s.error&&!s.snapshot->checklists[0].hide_all_items);
 {int mode;const char *updates[]={"UPDATE checklists SET show_on_minicard=NULL","UPDATE checklists SET show_on_minicard=0","UPDATE checklists SET show_on_minicard=1"};for(mode=-1;mode<=1;++mode){wena_checklists_close(&s);sql(d,updates[mode+1]);assert(wena_checklists_open(&s,&c));frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=!s.hide_all_items;frame(&s,&c,"Save",NULL);assert(!s.error&&s.snapshot->checklists[0].show_on_minicard==mode);}}
 frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=0;s.hide_checked_items=0;frame(&s,&c,"Save",NULL);assert(!s.error);
 frame(&s,&c,"Rename",NULL);frame(&s,&c,"Save","Renamed checklist");assert(!s.error&&!strcmp(s.snapshot->checklists[0].title,"Renamed checklist"));
 frame(&s,&c,"Edit",NULL);sql(d,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'rollback'); END");frame(&s,&c,"Save","Retry item");assert(s.error&&s.action&&!strcmp(s.input,"Retry item")&&!strcmp(s.snapshot->items[0].title,"First item"));sql(d,"DROP TRIGGER reject_metadata");frame(&s,&c,"Save",NULL);assert(!s.error&&!strcmp(s.snapshot->items[0].title,"Retry item"));
 frame(&s,&c,"Checklist Actions",NULL);s.hide_all_items=1;sql(d,"UPDATE cards SET version=version+1");frame(&s,&c,"Save",NULL);assert(s.error&&s.action==WENA_CHECKLIST_SET_FLAGS&&s.hide_all_items&&!s.snapshot->checklists[0].hide_all_items);frame(&s,&c,"Close details",NULL);assert(wena_checklists_open(&s,&c));
 frame(&s,&c,"Edit",NULL);sql(d,"UPDATE cards SET version=version+1");frame(&s,&c,"Save","Stale");assert(s.error&&s.action);frame(&s,&c,"Close details",NULL);
 assert(wena_checklists_open(&s,&c));memset(&e,0,sizeof(e));e.action=WENA_CHECKLIST_CREATE;e.expected_card_version=s.snapshot->card_version;e.title="Replay";assert(!wena_checklist_mutation_save_request(&a,"b","c",&e,1));wena_checklists_close(&s);
 assert(sqlite3_close(d)==SQLITE_OK);assert(sqlite3_open(argv[4],&d)==SQLITE_OK);assert(wena_checklist_mutation_init(&a,d,"u","b"));assert(wena_checklists_open(&s,&c));assert(s.snapshot->item_count==1&&s.snapshot->items[0].is_finished&&!strcmp(s.snapshot->items[0].title,"Retry item"));
 frame(&s,&c,"Add an item to checklist",NULL);frame(&s,&c,"Save","Retry item");assert(!s.error&&s.snapshot->item_count==2);
 {WenaId keep;strcpy(keep,s.snapshot->items[1].id);frame(&s,&c,"Edit",NULL);frame(&s,&c,"Delete","Unsaved rename");assert(s.action==WENA_CHECKLIST_DELETE_ITEM&&!strcmp(s.input,"Retry item"));frame(&s,&c,"Cancel",NULL);assert(s.snapshot->item_count==2);
 frame(&s,&c,"Edit",NULL);frame(&s,&c,"Delete",NULL);sql(d,"CREATE TRIGGER reject_delete BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'rollback delete'); END");frame(&s,&c,"Delete",NULL);assert(s.error&&s.action==WENA_CHECKLIST_DELETE_ITEM&&s.snapshot->item_count==2);sql(d,"DROP TRIGGER reject_delete");frame(&s,&c,"Delete",NULL);assert(!s.error&&s.snapshot->item_count==1&&!strcmp(s.snapshot->items[0].id,keep));}
 frame(&s,&c,"Rename",NULL);frame(&s,&c,"Delete",NULL);sql(d,"UPDATE cards SET version=version+1");frame(&s,&c,"Delete",NULL);assert(s.error&&s.action==WENA_CHECKLIST_DELETE&&s.snapshot->checklist_count==1);frame(&s,&c,"Close details",NULL);assert(wena_checklists_open(&s,&c));
 frame(&s,&c,"Rename",NULL);frame(&s,&c,"Delete",NULL);frame(&s,&c,"Delete",NULL);assert(!s.error&&!s.snapshot->checklist_count&&!s.snapshot->item_count);wena_checklists_close(&s);assert(sqlite3_close(d)==SQLITE_OK);assert(sqlite3_open(argv[4],&d)==SQLITE_OK);assert(wena_checklist_mutation_init(&a,d,"u","b"));assert(wena_checklists_open(&s,&c));assert(!s.snapshot->checklist_count&&!s.snapshot->item_count);
 /* Batch and both ordinal selectors pass through the real typed SQLite callback. */
 frame(&s,&c,"Add Checklist",NULL);frame(&s,&c,"Save","Batch list");assert(!s.error);
 frame(&s,&c,"Add an item to checklist",NULL);
 frame(&s,&c,wena_ui_text(WENA_UI_TEXT_CHECKLIST_SPLIT_LINES),NULL);
 frame(&s,&c,"Save","One\nTwo\nThree");assert(!s.error&&s.snapshot->item_count==3);
 frame(&s,&c,"Edit",NULL);frame(&s,&c,wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION),NULL);
 choose(&s,&c,"3");frame(&s,&c,"Cancel",NULL);assert(!strcmp(s.snapshot->items[0].title,"One"));
 frame(&s,&c,"Edit",NULL);frame(&s,&c,wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION),NULL);
 choose(&s,&c,"3");frame(&s,&c,"Save",NULL);
 assert(!s.error&&!strcmp(s.snapshot->items[2].title,"One")&&!strcmp(s.snapshot->items[0].title,"Two"));
 frame(&s,&c,"Edit",NULL);frame(&s,&c,wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION),NULL);choose(&s,&c,"3");
 sql(d,"CREATE TRIGGER reject_order BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'rollback order'); END");
 frame(&s,&c,"Save",NULL);assert(s.error&&s.action==WENA_CHECKLIST_REORDER_ITEM&&!strcmp(s.snapshot->items[0].title,"Two"));
 sql(d,"DROP TRIGGER reject_order");frame(&s,&c,"Cancel",NULL);
 frame(&s,&c,"Add Checklist",NULL);frame(&s,&c,"Save","Second list");assert(!s.error);
 frame(&s,&c,"Rename",NULL);frame(&s,&c,wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION),NULL);choose(&s,&c,"2");
 frame(&s,&c,"Save",NULL);assert(!s.error&&!strcmp(s.snapshot->checklists[1].title,"Batch list"));
 wena_checklists_close(&s);assert(sqlite3_close(d)==SQLITE_OK);assert(sqlite3_open(argv[4],&d)==SQLITE_OK);
 assert(wena_checklist_mutation_init(&a,d,"u","b"));assert(wena_checklists_open(&s,&c));
 assert(s.snapshot->item_count==3&&s.snapshot->checklist_count==2&&!strcmp(s.snapshot->checklists[1].title,"Batch list")&&!strcmp(s.snapshot->items[2].title,"One"));
 wena_checklists_close(&s);assert(sqlite3_close(d)==SQLITE_OK);puts("Checklist UI SQLite create, rename, item, completion,batch,order,rollback,stale,replay and reopen passed");return 0;
}
