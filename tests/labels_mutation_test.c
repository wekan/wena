#include "../client/features/labels/mutation.h"
#include "../server/mutations/labels.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3 *db,const char *query)
{
    char *error=NULL;
    int result;
    result=sqlite3_exec(db,query,NULL,NULL,&error);
    if (result!=SQLITE_OK) {fprintf(stderr,"SQL: %s: %s\n",query,error);sqlite3_free(error);}
    assert(result==SQLITE_OK);
}
static sqlite3_int64 number(sqlite3 *db,const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(db,query,-1,&statement,NULL)==SQLITE_OK);
    assert(sqlite3_step(statement)==SQLITE_ROW);
    result=sqlite3_column_int64(statement,0);
    assert(sqlite3_finalize(statement)==SQLITE_OK);return result;
}
static void schema(sqlite3 *db,const char *path)
{
    FILE *file;
    long length;
    char *data;
    file=fopen(path,"rb");assert(file);assert(!fseek(file,0,SEEK_END));
    length=ftell(file);assert(length>0);rewind(file);
    data=(char *)malloc((size_t)length+1);assert(data);
    assert(fread(data,1,(size_t)length,file)==(size_t)length);data[length]=0;
    assert(!fclose(file));sql(db,data);free(data);
}
static void edit(WenaLabelEdit *change,const WenaLabelSnapshot *snapshot,
    WenaLabelAction action,size_t index,const char *name,const char *color)
{
    memset(change,0,sizeof(*change));change->action=action;
    change->expected_board_version=snapshot->board_version;
    change->expected_card_version=snapshot->card_version;
    change->name=name;change->color=color;
    if (action!=WENA_LABEL_CREATE && index<snapshot->label_count) {
        change->label_id=snapshot->labels[index].id;
        change->expected_label_version=snapshot->label_versions[index];
    }
}
static void raw(WenaDomainCommand *command,WenaDomainOperation action,
    unsigned long board_version,const char *tail)
{
    memset(command,0,sizeof(*command));command->operation=action;
    command->request_version=5000;strcpy(command->user_id,"u");strcpy(command->route,"/b/b/native");
    sprintf(command->form_body,"cardId=&expectedCardVersion=0&expectedBoardVersion=%lu&%s",board_version,tail);
    command->form_body_length=strlen(command->form_body);
}
int main(int argc,char **argv)
{
    sqlite3 *db,*second;
    WenaLabelMutation adapter,other;
    WenaLabelSnapshot *snapshot,*before,*second_snapshot;
    WenaLabelEdit change,stale;
    WenaLabelBoardSnapshot *badges,*before_badges;
    const unsigned char *bits;
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaLabel model,original;
    char query[1024],label_id[65],name[130];
    unsigned long version,result_version;
    sqlite3_int64 keys,card_version,archived_version;
    int index;
    assert(argc==5);
    assert(sqlite3_open(argv[4],&db)==SQLITE_OK);
    sql(db,"PRAGMA foreign_keys=ON");schema(db,argv[1]);schema(db,argv[2]);
    sql(db,"INSERT INTO actors VALUES('u','User',1);INSERT INTO actors VALUES('v','Other',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('foreign','Foreign',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);");
    sql(db,"INSERT INTO lists VALUES('fl','foreign','List',0,1);INSERT INTO swimlanes VALUES('fs','foreign','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);INSERT INTO cards VALUES('c2','b','s','l','Second',1,0,1);INSERT INTO cards VALUES('fc','foreign','fs','fl','Foreign',0,0,1)");
    snapshot=wena_label_snapshot_create();before=wena_label_snapshot_create();second_snapshot=wena_label_snapshot_create();assert(snapshot&&before&&second_snapshot);
    badges=wena_label_board_snapshot_create();before_badges=wena_label_board_snapshot_create();assert(badges && before_badges);
    assert(wena_label_mutation_init(&adapter,db,"u","b"));
    memset(snapshot,37,sizeof(*snapshot));*before=*snapshot;
    assert(!wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    memset(&change,0,sizeof(change));change.action=WENA_LABEL_CREATE;change.expected_board_version=1;change.name="";change.color="";
    assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    schema(db,argv[3]);
    assert(wena_label_mutation_load(&adapter,"b",NULL,snapshot));
    assert(!snapshot->label_count && !snapshot->card_version && snapshot->board_version==1);
    assert(wena_label_snapshot_valid(snapshot,"b",NULL));
    assert(!wena_label_snapshot_valid(snapshot,"foreign",NULL));
    /* Model exact bounds, empty names/colors and atomic failed initialization. */
    memset(&model,37,sizeof(model));original=model;
    assert(!wena_label_init(&model,"bad/id","b","Name","white",0));assert(!memcmp(&model,&original,sizeof(model)));
    assert(wena_label_init(&model,"same","b","","",0));assert(wena_label_valid(&model));
    memset(name,'x',129);name[129]=0;assert(!wena_label_name_string_valid(name));
    name[128]=0;assert(wena_label_name_string_valid(name));
    assert(!wena_label_name_valid("\300\257",2));assert(!wena_label_name_valid("\302\200",2));
    assert(!wena_label_name_valid("\n",1));assert(!wena_label_name_valid("\177",1));
    edit(&change,snapshot,WENA_LABEL_CREATE,0,"","white");
    change.color="White";assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    change.color="#abc";assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    change.color="white";change.name="Bad\nname";assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    change.name="\300\257";assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    name[128]='x';change.name=name;assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    change.name="";change.color="";
    assert(!wena_label_mutation_save(&adapter,"foreign",NULL,&change));
    assert(wena_label_mutation_init(&other,db,"missing","b"));
    assert(!wena_label_mutation_save(&other,"b",NULL,&change));assert(!wena_label_mutation_load(&other,"b",NULL,snapshot));
    assert(wena_label_mutation_save_request(&adapter,"b",NULL,&change,20));
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->label_count==1 && snapshot->board_version==2 && snapshot->card_version==1);
    assert(!snapshot->labels[0].name[0] && !snapshot->labels[0].color[0]);strcpy(label_id,snapshot->labels[0].id);
    edit(&change,snapshot,WENA_LABEL_CREATE,0,"Replay","red");
    assert(!wena_label_mutation_save_request(&adapter,"b","c",&change,20));
    edit(&change,snapshot,WENA_LABEL_CREATE,0,"","");
    assert(!wena_label_mutation_save(&adapter,"b","c",&change));
    /* Exact no-op edit checks board/label/card guards without recording a key. */
    edit(&change,snapshot,WENA_LABEL_EDIT,0,"","");keys=number(db,"SELECT count(*) FROM idempotency_keys");
    assert(wena_label_mutation_save(&adapter,"b","c",&change));
    assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
    change.expected_label_version=2;assert(!wena_label_mutation_save(&adapter,"b","c",&change));change.expected_label_version=1;
    change.expected_board_version=1;assert(!wena_label_mutation_save(&adapter,"b","c",&change));change.expected_board_version=2;
    change.expected_card_version=2;assert(!wena_label_mutation_save(&adapter,"b","c",&change));
    edit(&change,snapshot,WENA_LABEL_EDIT,0," Finnish \303\244 & + ","#aBcD01");
    assert(wena_label_mutation_save(&adapter,"b","c",&change));assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->label_versions[0]==2 && !strcmp(snapshot->labels[0].name," Finnish \303\244 & + ") && !strcmp(snapshot->labels[0].color,"#aBcD01"));
    edit(&change,snapshot,WENA_LABEL_CREATE,0,snapshot->labels[0].name,"#ABCD01");
    assert(wena_label_mutation_save(&adapter,"b","c",&change));assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->label_count==2);
    edit(&change,snapshot,WENA_LABEL_EDIT,1,snapshot->labels[0].name,snapshot->labels[0].color);
    assert(!wena_label_mutation_save(&adapter,"b","c",&change));
    /* Shared IDs are legal on different boards and cannot escape board scope. */
    sprintf(query,"INSERT INTO labels(board_id,id,name,color,position) VALUES('foreign','%s','Foreign','red',0);INSERT INTO card_labels VALUES('foreign','fc','%s')",label_id,label_id);sql(db,query);
    edit(&change,snapshot,WENA_LABEL_ASSIGN,0,NULL,NULL);
    assert(!wena_label_mutation_save(&adapter,"b","fc",&change));
    change.label_id="missing";assert(!wena_label_mutation_save(&adapter,"b","c",&change));
    edit(&change,snapshot,WENA_LABEL_ASSIGN,0,NULL,NULL);stale=change;
    assert(wena_label_mutation_save_request(&adapter,"b","c",&change,30));
    assert(!wena_label_mutation_save(&adapter,"b","c",&stale));
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->assigned[0] && snapshot->assigned_card_counts[0]==1 && snapshot->card_version==2);
    edit(&change,snapshot,WENA_LABEL_ASSIGN,0,NULL,NULL);keys=number(db,"SELECT count(*) FROM idempotency_keys");
    assert(wena_label_mutation_save(&adapter,"b","c",&change));assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
    assert(!wena_label_mutation_save_request(&adapter,"b","c",&change,30));
    edit(&change,snapshot,WENA_LABEL_UNASSIGN,1,NULL,NULL);assert(wena_label_mutation_save(&adapter,"b","c",&change));
    assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
    /* Roll back both join-table write and versions if a later stage fails. */
    edit(&change,snapshot,WENA_LABEL_ASSIGN,1,NULL,NULL);version=snapshot->board_version;
    sql(db,"CREATE TRIGGER reject_board BEFORE UPDATE ON boards BEGIN SELECT RAISE(ABORT,'board'); END");
    assert(!wena_label_mutation_save(&adapter,"b","c",&change));sql(db,"DROP TRIGGER reject_board");
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->board_version==version && !snapshot->assigned[1] && snapshot->card_version==2);
    sql(db,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END");
    assert(!wena_label_mutation_save(&adapter,"b","c",&change));sql(db,"DROP TRIGGER reject_metadata");
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!snapshot->assigned[1] && snapshot->board_version==version);
    assert(wena_label_mutation_save(&adapter,"b","c",&change));assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    edit(&change,snapshot,WENA_LABEL_UNASSIGN,1,NULL,NULL);assert(wena_label_mutation_save(&adapter,"b","c",&change));
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!snapshot->assigned[1]);
    /* Concurrent assignment on another card invalidates a captured deletion. */
    edit(&stale,snapshot,WENA_LABEL_DELETE,0,NULL,NULL);
    assert(sqlite3_open(argv[4],&second)==SQLITE_OK);sql(second,"PRAGMA foreign_keys=ON");
    assert(wena_label_mutation_init(&other,second,"v","b"));assert(wena_label_mutation_load(&other,"b","c2",second_snapshot));
    edit(&change,second_snapshot,WENA_LABEL_ASSIGN,0,NULL,NULL);assert(wena_label_mutation_save(&other,"b","c2",&change));
    assert(sqlite3_close(second)==SQLITE_OK);assert(!wena_label_mutation_save(&adapter,"b","c",&stale));
    sql(db,"UPDATE cards SET archived=1,version=version+1 WHERE id='c2'");
    assert(!wena_label_mutation_load(&adapter,"b","c2",second_snapshot));
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->assigned_card_counts[0]==2);
    assert(wena_label_mutation_load_board(&adapter,"b",badges));
    assert(badges->card_count==2 && badges->catalogue.assigned_card_counts[0]==2);
    assert(wena_label_board_snapshot_valid(badges,"b"));
    bits=wena_label_board_assignments(badges,"b","c");assert(bits && (bits[0]&1));
    bits=wena_label_board_assignments(badges,"b","c2");assert(bits && (bits[0]&1));
    assert(!wena_label_board_assignments(badges,"foreign","c"));
    assert(!wena_label_board_assignments(badges,"b","missing"));
    *before_badges=*badges;
    card_version=number(db,"SELECT version FROM cards WHERE id='c'");archived_version=number(db,"SELECT version FROM cards WHERE id='c2'");
    edit(&change,snapshot,WENA_LABEL_DELETE,0,NULL,NULL);
    sql(db,"CREATE TRIGGER reject_delete BEFORE DELETE ON labels BEGIN SELECT RAISE(ABORT,'delete'); END");
    assert(!wena_label_mutation_save(&adapter,"b","c",&change));sql(db,"DROP TRIGGER reject_delete");
    assert(number(db,"SELECT version FROM cards WHERE id='c'")==card_version && number(db,"SELECT version FROM cards WHERE id='c2'")==archived_version);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b'")==2);
    assert(wena_label_mutation_save_request(&adapter,"b","c",&change,77));
    assert(number(db,"SELECT version FROM cards WHERE id='c'")==card_version+1 && number(db,"SELECT version FROM cards WHERE id='c2'")==archived_version+1);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b'")==0);
    assert(number(db,"SELECT count(*) FROM labels WHERE board_id='foreign'")==1 && number(db,"SELECT count(*) FROM card_labels WHERE board_id='foreign'")==1);
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->label_count==1 && snapshot->labels[0].position==1);
    edit(&change,snapshot,WENA_LABEL_CREATE,0,"Appended","white");assert(wena_label_mutation_save(&adapter,"b","c",&change));
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->labels[1].position==2);
    /* Raw adapter validation retains the same contract as the native form. */
    raw(&command,WENA_DOMAIN_CREATE_LABEL,snapshot->board_version,"name=%00&color=white");
    assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    raw(&command,WENA_DOMAIN_CREATE_LABEL,snapshot->board_version,"name=%C2%80&color=white");assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    raw(&command,WENA_DOMAIN_CREATE_LABEL,snapshot->board_version,"name=a&name=b&color=white");assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    raw(&command,WENA_DOMAIN_CREATE_LABEL,snapshot->board_version,"name=%GG&color=white");assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    raw(&command,WENA_DOMAIN_CREATE_LABEL,snapshot->board_version,"name=%0A&color=white");assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    assert(!wena_sqlite_labels_change(db,&command,"b",&result_version));
    /* Complete snapshot is never partly published after corrupt data. */
    *before=*snapshot;
    sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE labels SET name=char(1) WHERE board_id='b' AND position=1");
    assert(!wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    sql(db,"UPDATE labels SET name='Recovered' WHERE board_id='b' AND position=1;PRAGMA ignore_check_constraints=OFF");
    sql(db,"PRAGMA foreign_keys=OFF;INSERT INTO card_labels VALUES('foreign','c','missing')");
    assert(!wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    assert(!wena_label_mutation_load_board(&adapter,"b",badges));
    assert(!memcmp(badges,before_badges,sizeof(*badges)));
    sql(db,"DELETE FROM card_labels WHERE card_id='c';PRAGMA foreign_keys=ON");
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    /* Reopen preserves catalogue, source hex bytes, replay keys and versions. */
    *before=*snapshot;assert(sqlite3_close(db)==SQLITE_OK);assert(sqlite3_open(argv[4],&db)==SQLITE_OK);sql(db,"PRAGMA foreign_keys=ON");
    assert(wena_label_mutation_init(&adapter,db,"u","b"));assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    edit(&change,snapshot,WENA_LABEL_CREATE,0,"Replay","white");assert(!wena_label_mutation_save_request(&adapter,"b","c",&change,20));
    /* The maximum accepted name survives an actual write and complete load. */
    memset(name,'x',128);name[128]=0;
    edit(&change,snapshot,WENA_LABEL_CREATE,0,name,"white");
    assert(wena_label_mutation_save(&adapter,"b","c",&change));
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    assert(strlen(snapshot->labels[snapshot->label_count-1].name)==128);
    /* Exact capacity: complete 128-label/assignment snapshot, no truncation. */
    sql(db,"DELETE FROM card_labels WHERE board_id='b';DELETE FROM labels WHERE board_id='b';UPDATE cards SET archived=0 WHERE id='c2'");
    for (index=0;index<128;++index) {
        sprintf(query,"INSERT INTO labels(board_id,id,name,color,position) VALUES('b','label%d','Name%d','white',%d);INSERT INTO card_labels VALUES('b','c','label%d')",index,index,index,index);sql(db,query);
    }
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));assert(snapshot->label_count==128);
    for (index=0;index<128;++index) assert(snapshot->assigned[index] && snapshot->assigned_card_counts[index]==1);
    edit(&change,snapshot,WENA_LABEL_CREATE,0,"Overflow","red");assert(!wena_label_mutation_save(&adapter,"b","c",&change));
    *before=*snapshot;sql(db,"INSERT INTO labels(board_id,id,name,color,position) VALUES('b','overflow','Overflow','red',128)");
    assert(!wena_label_mutation_load(&adapter,"b","c",snapshot));assert(!memcmp(snapshot,before,sizeof(*snapshot)));
    sql(db,"DELETE FROM labels WHERE id='overflow'");
    assert(wena_label_mutation_load_board(&adapter,"b",badges));
    bits=wena_label_board_assignments(badges,"b","c");assert(bits);
    for (index=0;index<16;++index) assert(bits[index]==255);
    for (index=2;index<2048;++index) {
        sprintf(query,"INSERT INTO cards VALUES('cap%d','b','s','l','Card',%d,0,1)",index,index);sql(db,query);
    }
    assert(wena_label_mutation_load_board(&adapter,"b",badges));assert(badges->card_count==2048);
    assert(wena_label_board_assignments(badges,"b","cap2047"));
    *before_badges=*badges;
    sql(db,"INSERT INTO cards VALUES('overflowcard','b','s','l','Card',2048,0,1)");
    assert(!wena_label_mutation_load_board(&adapter,"b",badges));assert(!memcmp(badges,before_badges,sizeof(*badges)));
    sql(db,"DELETE FROM cards WHERE id='overflowcard'");
    /* One terminal readable version is reserved; no successful write poisons load. */
    sprintf(query,"UPDATE boards SET version=%lu WHERE id='b'",(unsigned long)LONG_MAX-1UL);sql(db,query);
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    edit(&change,snapshot,WENA_LABEL_UNASSIGN,0,NULL,NULL);assert(!wena_label_mutation_save(&adapter,"b","c",&change));
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b'")==128);
    /* Delete must reserve an increment for every affected card, including
     * archived cards that are absent from the editable selected-card scope. */
    sql(db,"UPDATE boards SET version=1 WHERE id='b';UPDATE cards SET archived=1 WHERE id='c2';INSERT INTO card_labels VALUES('b','c2','label0')");
    sprintf(query,"UPDATE cards SET version=%lu WHERE id='c'",WENA_VERSION_MUTATE_MAX);sql(db,query);
    sprintf(query,"UPDATE cards SET version=%lu WHERE id='c2'",WENA_VERSION_READ_MAX);sql(db,query);
    assert(wena_label_mutation_load(&adapter,"b",NULL,snapshot));
    assert(snapshot->assigned_card_counts[0]==2);
    edit(&change,snapshot,WENA_LABEL_DELETE,0,NULL,NULL);
    keys=number(db,"SELECT count(*) FROM idempotency_keys");
    assert(!wena_label_mutation_save(&adapter,"b",NULL,&change));
    assert(number(db,"SELECT count(*) FROM labels WHERE board_id='b'")==128);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b'")==129);
    assert(number(db,"SELECT version FROM cards WHERE id='c'")==(sqlite3_int64)WENA_VERSION_MUTATE_MAX);
    assert(number(db,"SELECT version FROM cards WHERE id='c2'")==(sqlite3_int64)WENA_VERSION_READ_MAX);
    assert(number(db,"SELECT version FROM boards WHERE id='b'")==1);
    assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys);
    sprintf(query,"UPDATE cards SET version=%lu WHERE id='c2'",WENA_VERSION_MUTATE_MAX);sql(db,query);
    assert(wena_label_mutation_save(&adapter,"b",NULL,&change));
    assert(number(db,"SELECT count(*) FROM labels WHERE board_id='b'")==127);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b'")==127);
    assert(number(db,"SELECT version FROM cards WHERE id='c'")==(sqlite3_int64)WENA_VERSION_READ_MAX);
    assert(number(db,"SELECT version FROM cards WHERE id='c2'")==(sqlite3_int64)WENA_VERSION_READ_MAX);
    assert(number(db,"SELECT version FROM boards WHERE id='b'")==2);
    assert(number(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
    assert(wena_label_mutation_load(&adapter,"b","c",snapshot));
    assert(snapshot->card_version==WENA_VERSION_READ_MAX && snapshot->label_count==127);
    assert(wena_label_mutation_load_board(&adapter,"b",badges));
    assert(sqlite3_close(db)==SQLITE_OK);
    wena_label_board_snapshot_free(badges);wena_label_board_snapshot_free(before_badges);
    wena_label_snapshot_free(snapshot);wena_label_snapshot_free(before);wena_label_snapshot_free(second_snapshot);
    puts("labels model, snapshots, guarded mutations, replay, rollback, concurrency and reopen passed");
    return 0;
}
