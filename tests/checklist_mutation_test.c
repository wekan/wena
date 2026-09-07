#include "../client/features/checklist_mutation.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3*d,const char*q){char*e=NULL;int rc;rc=sqlite3_exec(d,q,NULL,NULL,&e);if(rc!=SQLITE_OK){fprintf(stderr,"SQL: %s: %s\n",q,e);sqlite3_free(e);}assert(rc==SQLITE_OK);}
static int number(sqlite3*d,const char*q){sqlite3_stmt*s;int n;assert(sqlite3_prepare_v2(d,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
static void schema(sqlite3*d,const char*p){FILE*f;long n;char*s;f=fopen(p,"rb");assert(f);assert(!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0);rewind(f);s=(char*)malloc((size_t)n+1);assert(s);assert(fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);sql(d,s);free(s);}
static void edit(WenaChecklistEdit*e,WenaChecklistSnapshot*s,WenaChecklistAction action,const char*title)
{memset(e,0,sizeof(*e));e->action=action;e->title=title;e->expected_card_version=s->card_version;if(s->checklist_count){e->checklist_id=s->checklists[0].id;e->expected_checklist_version=s->checklist_versions[0];}if(s->item_count){e->item_id=s->items[0].id;e->expected_item_version=s->item_versions[0];}}
int main(int argc,char**argv)
{
    sqlite3*d,*second;
    WenaChecklistMutation a,other;
    WenaChecklistSnapshot *s,*before;
    WenaChecklistEdit e,stale;
    WenaDomainCommand c;
    WenaRegionResponse r;
    WenaChecklistProgress progress;
    unsigned long cv,lv,iv;
    int keys,hc,ha,mini;
    char q[512],longtitle[130],listid[65],itemid[65];
    assert(argc==5);assert(sqlite3_open(argv[4],&d)==SQLITE_OK);sql(d,"PRAGMA foreign_keys=ON");schema(d,argv[1]);schema(d,argv[2]);
    sql(d,"INSERT INTO actors VALUES('u','User',1);INSERT INTO actors VALUES('v','Other',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);INSERT INTO cards VALUES('c2','b','s','l','Other',1,0,1)");
    s=wena_checklist_snapshot_create();before=wena_checklist_snapshot_create();assert(s&&before);
    assert(wena_checklist_mutation_init(&a,d,"u","b"));memset(s,37,sizeof(*s));*before=*s;
    assert(!wena_checklist_mutation_load(&a,"b","c",s));assert(!memcmp(s,before,sizeof(*s)));
    memset(&e,0,sizeof(e));e.action=WENA_CHECKLIST_CREATE;e.title="Before schema";e.expected_card_version=1;
    assert(!wena_checklist_mutation_save(&a,"b","c",&e));assert(number(d,"SELECT version FROM cards WHERE id='c'")==1);
    schema(d,argv[3]);
    assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->card_version==1&&!s->checklist_count&&!s->item_count);
    edit(&e,s,WENA_CHECKLIST_CREATE,"");assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    e.title="\n";assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.title="\300\257";assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    memset(longtitle,'x',129);longtitle[129]=0;e.title=longtitle;assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    e.title=" Pack & + \303\244 ";assert(!wena_checklist_mutation_save(&a,"other","c",&e));assert(!wena_checklist_mutation_save(&a,"b","missing",&e));
    assert(wena_checklist_mutation_init(&other,d,"unknown","b"));assert(!wena_checklist_mutation_save(&other,"b","c",&e));assert(!wena_checklist_mutation_load(&other,"b","c",s));
    assert(wena_checklist_mutation_save_request(&a,"b","c",&e,20));assert(wena_checklist_mutation_load(&a,"b","c",s));
    assert(s->card_version==2&&s->checklist_count==1&&s->checklist_versions[0]==1&&!strcmp(s->checklists[0].title,e.title));strcpy(listid,s->checklists[0].id);
    assert(number(d,"SELECT created_at>=0 AND updated_at>=created_at FROM checklists")==1);
    e.expected_card_version=2;assert(!wena_checklist_mutation_save_request(&a,"b","c",&e,20));
    edit(&e,s,WENA_CHECKLIST_ADD_ITEM,"First");stale=e;assert(wena_checklist_mutation_save(&a,"b","c",&e));
    assert(!wena_checklist_mutation_save(&a,"b","c",&stale));assert(wena_checklist_mutation_load(&a,"b","c",s));
    assert(s->card_version==3&&s->checklist_versions[0]==2&&s->item_count==1&&s->item_versions[0]==1);strcpy(itemid,s->items[0].id);
    edit(&e,s,WENA_CHECKLIST_SET_FINISHED,NULL);e.is_finished=1;assert(wena_checklist_mutation_save(&a,"b","c",&e));
    assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->card_version==4&&s->checklist_versions[0]==2&&s->item_versions[0]==2&&s->items[0].is_finished);
    assert(wena_checklist_progress(&s->checklists[0],s->items,s->item_count,&progress)&&progress.percent==100);
    edit(&e,s,WENA_CHECKLIST_SET_FINISHED,NULL);e.is_finished=1;keys=number(d,"SELECT count(*) FROM idempotency_keys");
    assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys&&number(d,"SELECT version FROM cards WHERE id='c'")==4);
    e.expected_item_version=1;assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.expected_item_version=2;e.is_finished=2;assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    edit(&e,s,WENA_CHECKLIST_RENAME_ITEM,"Renamed item");e.checklist_id="missing";assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.checklist_id=listid;e.item_id="missing";assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.item_id=itemid;
    assert(!wena_checklist_mutation_save(&a,"b","c2",&e));
    sql(d,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END");
    assert(!wena_checklist_mutation_save(&a,"b","c",&e));sql(d,"DROP TRIGGER reject_metadata");
    assert(number(d,"SELECT version FROM cards WHERE id='c'")==4&&number(d,"SELECT version FROM checklist_items")==2);
    assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->card_version==5&&s->item_versions[0]==3&&s->checklist_versions[0]==2);
    edit(&e,s,WENA_CHECKLIST_RENAME,"Renamed checklist");assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->card_version==6&&s->checklist_versions[0]==3);
    /* Identical titles are fully guarded no-ops, with no metadata writes. */
    edit(&e,s,WENA_CHECKLIST_RENAME,s->checklists[0].title);keys=number(d,"SELECT count(*) FROM idempotency_keys");
    assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);
    edit(&e,s,WENA_CHECKLIST_RENAME_ITEM,s->items[0].title);assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);
    /* Future imported/local timestamps cannot go backwards on an edit. */
    sql(d,"UPDATE checklist_items SET updated_at=8000000000000");
    edit(&e,s,WENA_CHECKLIST_SET_FINISHED,NULL);e.is_finished=0;assert(wena_checklist_mutation_save(&a,"b","c",&e));
    assert(number(d,"SELECT updated_at=8000000000000 FROM checklist_items")==1);
    assert(wena_checklist_mutation_load(&a,"b","c",s)&&!s->items[0].is_finished);
    /* Failure after child insert/parent version update rolls everything back. */
    sql(d,"CREATE TRIGGER reject_card BEFORE UPDATE ON cards BEGIN SELECT RAISE(ABORT,'card'); END");
    edit(&e,s,WENA_CHECKLIST_ADD_ITEM,"Rollback child");assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    sql(d,"DROP TRIGGER reject_card");assert(number(d,"SELECT count(*) FROM checklist_items")==1);
    /* A second connection changes the aggregate through another card field. */
    assert(sqlite3_open(argv[4],&second)==SQLITE_OK);sql(second,"UPDATE cards SET title='Concurrent',version=version+1 WHERE id='c'");assert(sqlite3_close(second)==SQLITE_OK);
    edit(&e,s,WENA_CHECKLIST_ADD_ITEM,"Stale card");assert(!wena_checklist_mutation_save(&a,"b","c",&e));assert(wena_checklist_mutation_load(&a,"b","c",s));
    edit(&e,s,WENA_CHECKLIST_ADD_ITEM,"Second");assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->item_count==2&&s->items[1].position==1);
    /* Actor identity separates durable IDs, even same request/operation/board. */
    assert(wena_checklist_mutation_init(&other,d,"v","b"));edit(&e,s,WENA_CHECKLIST_CREATE,"Actor two");assert(wena_checklist_mutation_save_request(&other,"b","c",&e,20));assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->checklist_count==2&&strcmp(s->checklists[0].id,s->checklists[1].id));
    /* Display flags preserve actual item completion and derived counts. */
    edit(&e,s,WENA_CHECKLIST_SET_FLAGS,NULL);e.show_on_minicard=WENA_CHECKLIST_MINICARD_INHERIT;
    keys=number(d,"SELECT count(*) FROM idempotency_keys");cv=s->card_version;
    assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);
    e.expected_checklist_version--;assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.expected_checklist_version++;
    e.hide_checked_items=2;assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.hide_checked_items=0;
    e.hide_all_items=-1;assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.hide_all_items=0;
    e.show_on_minicard=(WenaChecklistMinicard)2;assert(!wena_checklist_mutation_save(&a,"b","c",&e));e.show_on_minicard=WENA_CHECKLIST_MINICARD_HIDE;
    assert(!wena_checklist_mutation_save(&a,"other","c",&e));assert(!wena_checklist_mutation_save(&a,"b","c2",&e));
    sql(d,"CREATE TRIGGER reject_flags BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'flags'); END");
    assert(!wena_checklist_mutation_save_request(&a,"b","c",&e,500));sql(d,"DROP TRIGGER reject_flags");
    assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);
    assert(wena_checklist_mutation_load(&a,"b","c",s)&&s->card_version==cv&&s->checklists[0].show_on_minicard==WENA_CHECKLIST_MINICARD_INHERIT);
    assert(wena_checklist_mutation_save_request(&a,"b","c",&e,500));
    assert(wena_checklist_mutation_load(&a,"b","c",s));
    edit(&e,s,WENA_CHECKLIST_SET_FLAGS,NULL);e.show_on_minicard=WENA_CHECKLIST_MINICARD_SHOW;
    assert(!wena_checklist_mutation_save_request(&a,"b","c",&e,500));
    sql(d,"UPDATE checklists SET updated_at=8000000000000");
    for(hc=0;hc<=1;++hc)for(ha=0;ha<=1;++ha)for(mini=-1;mini<=1;++mini){
        iv=s->item_versions[0];lv=s->checklist_versions[1];
        edit(&e,s,WENA_CHECKLIST_SET_FLAGS,NULL);e.hide_checked_items=hc;e.hide_all_items=ha;e.show_on_minicard=(WenaChecklistMinicard)mini;
        assert(wena_checklist_mutation_save(&a,"b","c",&e));assert(wena_checklist_mutation_load(&a,"b","c",s));
        assert(s->checklists[0].hide_checked_items==hc&&s->checklists[0].hide_all_items==ha&&s->checklists[0].show_on_minicard==(WenaChecklistMinicard)mini);
        assert(s->item_versions[0]==iv&&s->checklist_versions[1]==lv&&!s->items[0].is_finished&&!s->items[1].is_finished);
        assert(wena_checklist_progress(&s->checklists[0],s->items,2,&progress)&&progress.finished==0&&progress.percent==0&&progress.is_finished==ha);
        assert(number(d,"SELECT min(updated_at)=8000000000000 FROM checklists")==1);
    }
    /* Native-only raw form parsing rejects malformed and duplicate booleans. */
    memset(&c,0,sizeof(c));c.operation=WENA_DOMAIN_SET_CHECKLIST_FLAGS;c.request_version=700;strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");
    sprintf(c.form_body,"cardId=c&expectedVersion=%lu&checklistId=%s&expectedChecklistVersion=%lu&hideChecked=1&hideAll=0&showOnMinicard=2",s->card_version,listid,s->checklist_versions[0]);c.form_body_length=strlen(c.form_body);
    assert(!wena_sqlite_persistence_apply(&a.persistence,&c,&r));
    sprintf(c.form_body,"cardId=c&expectedVersion=%lu&checklistId=%s&expectedChecklistVersion=%lu&hideChecked=1&hideChecked=0&hideAll=0&showOnMinicard=-1",s->card_version,listid,s->checklist_versions[0]);c.form_body_length=strlen(c.form_body);
    assert(!wena_sqlite_persistence_apply(&a.persistence,&c,&r));
    cv=s->card_version;lv=s->checklist_versions[0];iv=s->item_versions[0];*before=*s;
    sql(d,"UPDATE cards SET archived=1 WHERE id='c'");edit(&e,s,WENA_CHECKLIST_RENAME,"Archived");assert(!wena_checklist_mutation_save(&a,"b","c",&e));assert(!wena_checklist_mutation_load(&a,"b","c",s));assert(!memcmp(s,before,sizeof(*s)));sql(d,"UPDATE cards SET archived=0 WHERE id='c'");
    assert(sqlite3_close(d)==SQLITE_OK);assert(sqlite3_open(argv[4],&d)==SQLITE_OK);sql(d,"PRAGMA foreign_keys=ON");assert(wena_checklist_mutation_init(&a,d,"u","b"));assert(wena_checklist_mutation_load(&a,"b","c",s));assert(s->card_version==cv&&s->checklist_versions[0]==lv&&s->item_versions[0]==iv);
    assert(s->checklists[0].hide_checked_items==1&&s->checklists[0].hide_all_items==1&&s->checklists[0].show_on_minicard==WENA_CHECKLIST_MINICARD_SHOW);
    edit(&e,s,WENA_CHECKLIST_SET_FLAGS,NULL);e.show_on_minicard=WENA_CHECKLIST_MINICARD_INHERIT;
    assert(!wena_checklist_mutation_save_request(&a,"b","c",&e,500));
    edit(&e,s,WENA_CHECKLIST_CREATE,"Replay reopened");assert(!wena_checklist_mutation_save_request(&a,"b","c",&e,20));
    /* Raw typed commands must enforce title validation too. */
    memset(&c,0,sizeof(c));c.operation=WENA_DOMAIN_CREATE_CHECKLIST;c.request_version=200;strcpy(c.user_id,"u");strcpy(c.route,"/b/b/native");sprintf(c.form_body,"cardId=c&expectedVersion=%lu&title=%%C2%%80",cv);c.form_body_length=strlen(c.form_body);assert(!wena_sqlite_persistence_apply(&a.persistence,&c,&r));
    /* Malformed persisted payloads never partially overwrite a snapshot. */
    *before=*s;sql(d,"PRAGMA ignore_check_constraints=ON;UPDATE checklist_items SET title=char(1) WHERE position=0");assert(!wena_checklist_mutation_load(&a,"b","c",s));assert(!memcmp(s,before,sizeof(*s)));sql(d,"UPDATE checklist_items SET title='Recovered' WHERE position=0;PRAGMA ignore_check_constraints=OFF");
    /* Scope-corrupt rows must not disappear from an apparently complete load. */
    sql(d,"PRAGMA foreign_keys=OFF;UPDATE checklist_items SET board_id='other' WHERE position=0");
    assert(!wena_checklist_mutation_load(&a,"b","c",s));assert(!memcmp(s,before,sizeof(*s)));
    sql(d,"UPDATE checklist_items SET board_id='b' WHERE position=0;PRAGMA foreign_keys=ON");
    sql(d,"PRAGMA ignore_check_constraints=ON;UPDATE checklists SET position=0.5 WHERE position=0");
    assert(!wena_checklist_mutation_load(&a,"b","c",s));
    sql(d,"UPDATE checklists SET position=0 WHERE position=0.5;PRAGMA ignore_check_constraints=OFF");
    /* Capacity checks apply to complete card collections in SQL, not just UI. */
    sql(d,"WITH RECURSIVE n(x) AS (SELECT 2 UNION ALL SELECT x+1 FROM n WHERE x<63) INSERT INTO checklists(id,board_id,card_id,title,position) SELECT 'extra-'||x,'b','c','Extra',x FROM n");
    assert(wena_checklist_mutation_load(&a,"b","c",s)&&s->checklist_count==64);edit(&e,s,WENA_CHECKLIST_CREATE,"Overflow");assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    sprintf(q,"WITH RECURSIVE n(x) AS (SELECT 2 UNION ALL SELECT x+1 FROM n WHERE x<1023) INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) SELECT 'item-'||x,'b','c','%s','Item',x FROM n",listid);sql(d,q);
    assert(wena_checklist_mutation_load(&a,"b","c",s)&&s->item_count==1024);edit(&e,s,WENA_CHECKLIST_ADD_ITEM,"Overflow");assert(!wena_checklist_mutation_save(&a,"b","c",&e));
    sprintf(q,"INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES('overflow-item','b','c','%s','Overflow',1024)",listid);sql(d,q);
    *before=*s;assert(!wena_checklist_mutation_load(&a,"b","c",s));assert(!memcmp(s,before,sizeof(*s)));sql(d,"DELETE FROM checklist_items WHERE id='overflow-item'");
    sql(d,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('overflow','b','c','Overflow',64)");*before=*s;assert(!wena_checklist_mutation_load(&a,"b","c",s));assert(!memcmp(s,before,sizeof(*s)));
    wena_checklist_snapshot_free(s);wena_checklist_snapshot_free(before);assert(sqlite3_close(d)==SQLITE_OK);puts("checklist mutation tests passed");return 0;
}
