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
int main(int argc,char **argv)
{
    sqlite3 *database,*concurrent;
    WenaChecklistMutation adapter,unknown;
    WenaChecklistSnapshot *snapshot,*before;
    WenaChecklistEdit change;
    WenaId checklist_id,item_id;
    unsigned long card_version,checklist_version;
    int keys;
    assert(argc==5);
    assert(sqlite3_open(argv[4],&database)==SQLITE_OK);
    sql(database,"PRAGMA foreign_keys=ON");
    schema(database,argv[1]);schema(database,argv[2]);schema(database,argv[3]);
    sql(database,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);INSERT INTO cards VALUES('c2','b','s','l','Other',1,0,1)");
    sql(database,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('check','b','c','Checklist',0),('keep','b','c','Keep',1),('other-card','b','c2','Other card',0);INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES('item0','b','c','check','Zero',0),('item1','b','c','check','One',1),('item2','b','c','check','Two',2),('kept-item','b','c','keep','Keep item',0),('other-item','b','c2','other-card','Other item',0)");
    snapshot=wena_checklist_snapshot_create();before=wena_checklist_snapshot_create();assert(snapshot&&before);
    assert(wena_checklist_mutation_init(&adapter,database,"u","b"));
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));
    strcpy(checklist_id,"check");strcpy(item_id,"item1");
    edit(&change,snapshot,WENA_CHECKLIST_DELETE_ITEM,NULL);change.item_id=item_id;
    assert(wena_checklist_mutation_init(&unknown,database,"absent","b"));
    assert(!wena_checklist_mutation_save(&unknown,"b","c",&change));
    assert(!wena_checklist_mutation_save(&adapter,"other","c",&change));
    assert(!wena_checklist_mutation_save(&adapter,"b","c2",&change));
    change.expected_card_version=2;assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));change.expected_card_version=1;
    change.expected_checklist_version=2;assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));change.expected_checklist_version=1;
    change.expected_item_version=2;assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));change.expected_item_version=1;
    change.checklist_id="keep";assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));change.checklist_id=checklist_id;
    change.item_id="missing";assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));change.item_id=item_id;
    sql(database,"UPDATE cards SET archived=1 WHERE id='c'");assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));sql(database,"UPDATE cards SET archived=0 WHERE id='c'");
    /* Final aggregate failure restores deleted item AND updated parent. */
    sql(database,"UPDATE checklists SET updated_at=8000000000000 WHERE id='check';CREATE TRIGGER fail_card BEFORE UPDATE ON cards BEGIN SELECT RAISE(ABORT,'card'); END");
    assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,20));sql(database,"DROP TRIGGER fail_card");
    assert(number(database,"SELECT count(*) FROM checklist_items WHERE id='item1'")==1);
    assert(number(database,"SELECT version FROM checklists WHERE id='check'")==1);
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==0);
    assert(wena_checklist_mutation_save_request(&adapter,"b","c",&change,20));
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->card_version==2&&snapshot->checklist_versions[0]==2);
    assert(snapshot->item_count==3&&!strcmp(snapshot->items[0].id,"item0")&&!strcmp(snapshot->items[1].id,"item2")&&snapshot->items[1].position==2);
    assert(number(database,"SELECT updated_at=8000000000000 FROM checklists WHERE id='check'")==1);
    edit(&change,snapshot,WENA_CHECKLIST_DELETE_ITEM,NULL);change.item_id=item_id;
    assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));
    change.item_id="item0";assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,20));
    edit(&change,snapshot,WENA_CHECKLIST_ADD_ITEM,"After gap");assert(wena_checklist_mutation_save(&adapter,"b","c",&change));
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->items[2].position==3);
    /* Child edits leave checklist.version unchanged; card aggregate guards delete. */
    edit(&change,snapshot,WENA_CHECKLIST_DELETE,NULL);
    assert(sqlite3_open(argv[4],&concurrent)==SQLITE_OK);
    sql(concurrent,"UPDATE checklist_items SET title='Concurrent',version=version+1 WHERE id='item0';UPDATE cards SET version=version+1 WHERE id='c'");
    assert(sqlite3_close(concurrent)==SQLITE_OK);
    assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));
    edit(&change,snapshot,WENA_CHECKLIST_DELETE,NULL);*before=*snapshot;
    keys=number(database,"SELECT count(*) FROM idempotency_keys");
    /* Child deletion followed by parent deletion error restores every child. */
    sql(database,"CREATE TRIGGER fail_parent BEFORE DELETE ON checklists BEGIN SELECT RAISE(ABORT,'parent'); END");
    assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,40));sql(database,"DROP TRIGGER fail_parent");
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    sql(database,"CREATE TRIGGER fail_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END");
    assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,40));sql(database,"DROP TRIGGER fail_metadata");
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    /* Corrupt scope must not leave a child orphan when FK checks are off. */
    sql(database,"PRAGMA foreign_keys=OFF;UPDATE checklist_items SET board_id='other' WHERE id='item0'");
    assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,40));
    sql(database,"UPDATE checklist_items SET board_id='b' WHERE id='item0';PRAGMA foreign_keys=ON");
    sql(database,"PRAGMA foreign_keys=OFF;CREATE TRIGGER ignore_child BEFORE DELETE ON checklist_items BEGIN SELECT RAISE(IGNORE); END");
    assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,40));
    sql(database,"DROP TRIGGER ignore_child;PRAGMA foreign_keys=ON");
    card_version=snapshot->card_version;checklist_version=snapshot->checklist_versions[1];
    assert(wena_checklist_mutation_save_request(&adapter,"b","c",&change,40));
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->card_version==card_version+1&&snapshot->checklist_count==1&&snapshot->item_count==1);
    assert(!strcmp(snapshot->checklists[0].id,"keep")&&snapshot->checklists[0].position==1&&snapshot->checklist_versions[0]==checklist_version);
    assert(number(database,"SELECT count(*) FROM checklist_items WHERE checklist_id='check'")==0);
    assert(number(database,"SELECT count(*) FROM checklist_items WHERE card_id='c2'")==1&&number(database,"SELECT version FROM cards WHERE id='c2'")==1);
    assert(sqlite3_close(database)==SQLITE_OK);assert(sqlite3_open(argv[4],&database)==SQLITE_OK);sql(database,"PRAGMA foreign_keys=ON");
    assert(wena_checklist_mutation_init(&adapter,database,"u","b"));assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));
    edit(&change,snapshot,WENA_CHECKLIST_DELETE,NULL);assert(!wena_checklist_mutation_save_request(&adapter,"b","c",&change,40));
    change.checklist_id="check";assert(!wena_checklist_mutation_save(&adapter,"b","c",&change));
    edit(&change,snapshot,WENA_CHECKLIST_CREATE,"After checklist gap");assert(wena_checklist_mutation_save(&adapter,"b","c",&change));
    assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->checklists[1].position==2);
    /* Empty checklist removal succeeds: child DELETE may affect zero rows. */
    edit(&change,snapshot,WENA_CHECKLIST_DELETE,NULL);change.checklist_id=snapshot->checklists[1].id;change.expected_checklist_version=snapshot->checklist_versions[1];
    assert(wena_checklist_mutation_save(&adapter,"b","c",&change));assert(wena_checklist_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->checklist_count==1);
    wena_checklist_snapshot_free(snapshot);wena_checklist_snapshot_free(before);
    assert(sqlite3_close(database)==SQLITE_OK);puts("permanent scoped checklist deletion tests passed");return 0;
}
