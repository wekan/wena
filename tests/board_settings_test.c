#include "../client/features/boards/settings.h"
#include "../server/mutations/board_settings.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3 *database,const char *query)
{
    char *error=NULL;
    int status;
    status=sqlite3_exec(database,query,NULL,NULL,&error);
    if (status!=SQLITE_OK) {fprintf(stderr,"SQL %s: %s\n",query,error);sqlite3_free(error);}
    assert(status==SQLITE_OK);
}
static sqlite3_int64 number(sqlite3 *database,const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(database,query,-1,&statement,NULL)==SQLITE_OK);
    assert(sqlite3_step(statement)==SQLITE_ROW);
    result=sqlite3_column_int64(statement,0);
    assert(sqlite3_finalize(statement)==SQLITE_OK);return result;
}
static void schema(sqlite3 *database,const char *path)
{
    FILE *file;
    char *data;
    long length;
    file=fopen(path,"rb");assert(file);assert(!fseek(file,0,SEEK_END));
    length=ftell(file);assert(length>0);rewind(file);
    data=(char *)malloc((size_t)length+1);assert(data);
    assert(fread(data,1,(size_t)length,file)==(size_t)length);data[length]=0;
    assert(!fclose(file));sql(database,data);free(data);
}
static void raw(WenaDomainCommand *command,unsigned long version,const char *value)
{
    memset(command,0,sizeof(*command));
    command->operation=WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT;
    command->request_version=900;strcpy(command->user_id,"u");strcpy(command->route,"/b/b/native");
    sprintf(command->form_body,"expectedBoardVersion=%lu&showChecklistCount=%s",version,value);
    command->form_body_length=strlen(command->form_body);
}
static void display_settings(sqlite3 *database)
{
    WenaBoardSettingsMutation adapter;
    WenaBoardSettingsSnapshot snapshot,before;
    WenaDomainCommand command;
    WenaRegionResponse response;
    sqlite3_int64 keys;
    int index;
    const char *triggers[] = {
        "CREATE TRIGGER reject_display BEFORE INSERT ON board_minicard_settings BEGIN SELECT RAISE(ABORT,'display'); END",
        "CREATE TRIGGER reject_display BEFORE INSERT ON board_minicard_settings BEGIN SELECT RAISE(IGNORE); END",
        "CREATE TRIGGER reject_display AFTER INSERT ON board_minicard_settings BEGIN UPDATE board_settings SET show_checklist_count=0 WHERE board_id='display'; END",
        "CREATE TRIGGER reject_display BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END"
    };
    sql(database,"INSERT INTO boards VALUES('display','Display',1)");
    assert(wena_board_settings_mutation_init(&adapter,database,"u","display"));
    assert(wena_board_settings_mutation_load(&adapter,"display",&snapshot));
    assert(snapshot.show_checklists && !snapshot.show_checklist_count);
    assert(!wena_board_settings_mutation_save_display(&adapter,"display",1,0,2));
    assert(!wena_board_settings_mutation_save_display(&adapter,"foreign",1,0,0));
    raw(&command,1,"1");strcpy(command.route,"/b/display/native");command.operation=WENA_DOMAIN_SET_BOARD_PRESENTATION;
    assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    strcat(command.form_body,"&showChecklists=1&showChecklists=0");command.form_body_length=strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    keys=number(database,"SELECT count(*) FROM idempotency_keys");
    for(index=0;index<4;++index) {
        sql(database,triggers[index]);
        assert(!wena_board_settings_mutation_save_display(&adapter,"display",1,1,0));
        sql(database,"DROP TRIGGER reject_display");
        assert(number(database,"SELECT count(*) FROM board_settings WHERE board_id='display'")==0);
        assert(number(database,"SELECT count(*) FROM board_minicard_settings WHERE board_id='display'")==0);
        assert(number(database,"SELECT version FROM boards WHERE id='display'")==1);
        assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    }
    assert(wena_board_settings_mutation_save_display_request(&adapter,"display",1,1,0,100));
    assert(wena_board_settings_mutation_load(&adapter,"display",&snapshot));
    assert(snapshot.board_version==2 && snapshot.show_checklist_count && !snapshot.show_checklists);
    assert(!wena_board_settings_mutation_save_display_request(&adapter,"display",2,0,1,100));
    assert(!wena_board_settings_mutation_save_display(&adapter,"display",1,0,1));
    assert(wena_board_settings_mutation_save(&adapter,"display",2,0));
    assert(wena_board_settings_mutation_load(&adapter,"display",&snapshot));
    assert(snapshot.board_version==3 && !snapshot.show_checklists && !snapshot.show_checklist_count);
    keys=number(database,"SELECT count(*) FROM idempotency_keys");
    assert(wena_board_settings_mutation_save_display(&adapter,"display",3,0,0));
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    before=snapshot;
    sql(database,"PRAGMA ignore_check_constraints=ON;UPDATE board_minicard_settings SET show_checklists=2 WHERE board_id='display'");
    assert(!wena_board_settings_mutation_load(&adapter,"display",&snapshot));assert(!memcmp(&before,&snapshot,sizeof(snapshot)));
    assert(!wena_board_settings_mutation_save_display(&adapter,"display",3,1,1));
    sql(database,"UPDATE board_minicard_settings SET show_checklists=0 WHERE board_id='display';PRAGMA ignore_check_constraints=OFF");
}
int main(int argc,char **argv)
{
    sqlite3 *database,*second;
    WenaBoardSettingsMutation adapter,other;
    WenaBoardSettingsSnapshot snapshot,before;
    WenaDomainCommand command;
    WenaRegionResponse response;
    unsigned long version,output;
    sqlite3_int64 keys;
    char query[256];
    const char *invalid[] = {"", "2", "-1", "+1", "00", "01", "1x", "true", "1&showChecklistCount=0", "%00", "%0A", "%GG", "1%20"};
    size_t index;
    assert(argc==4);assert(sqlite3_open(argv[3],&database)==SQLITE_OK);
    sql(database,"PRAGMA foreign_keys=ON");schema(database,argv[1]);
    sql(database,"INSERT INTO actors VALUES('u','User',1);INSERT INTO actors VALUES('v','Other',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('foreign','Foreign',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1)");
    sql(database,"INSERT INTO cards VALUES('active','b','s','l','Active',0,0,1);INSERT INTO cards VALUES('archived','b','s','l','Archived',1,1,1)");
    assert(!wena_board_settings_mutation_init(NULL,database,"u","b"));
    assert(!wena_board_settings_mutation_init(&adapter,database,"bad/id","b"));
    assert(wena_board_settings_mutation_init(&adapter,database,"u","b"));
    memset(&snapshot,37,sizeof(snapshot));before=snapshot;
    assert(!wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!memcmp(&snapshot,&before,sizeof(snapshot)));
    assert(!wena_board_settings_mutation_save(&adapter,"b",1,1));
    assert(number(database,"SELECT version FROM boards WHERE id='b'")==1);
    schema(database,argv[2]);
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));
    assert(snapshot.board_version==1 && !snapshot.show_checklist_count && !strcmp(snapshot.board_id,"b"));
    assert(wena_board_settings_snapshot_valid(&snapshot,"b"));
    assert(!wena_board_settings_snapshot_valid(&snapshot,"foreign"));
    before=snapshot;before.show_checklist_count=2;assert(!wena_board_settings_snapshot_valid(&before,"b"));
    /* Canonical default false is a guarded no-op without creating a row. */
    assert(wena_board_settings_mutation_save_request(&adapter,"b",1,0,20));
    assert(number(database,"SELECT count(*) FROM board_settings")==0);
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==0);
    assert(!wena_board_settings_mutation_save(&adapter,"b",0,1));
    assert(!wena_board_settings_mutation_save(&adapter,"b",2,1));
    assert(!wena_board_settings_mutation_save(&adapter,"b",1,-1));
    assert(!wena_board_settings_mutation_save(&adapter,"b",1,2));
    assert(!wena_board_settings_mutation_save(&adapter,"foreign",1,1));
    assert(!wena_board_settings_mutation_save(&adapter,"missing",1,1));
    assert(!wena_board_settings_mutation_save_request(&adapter,"b",1,1,0));
    assert(!wena_board_settings_mutation_save_request(&adapter,"b",1,1,(unsigned long)LONG_MAX));
    assert(wena_board_settings_mutation_init(&other,database,"missing","b"));
    before=snapshot;assert(!wena_board_settings_mutation_load(&other,"b",&snapshot));assert(!memcmp(&snapshot,&before,sizeof(snapshot)));
    assert(!wena_board_settings_mutation_save(&other,"b",1,1));
    /* A failure after inserting the setting still leaves no partial row. */
    sql(database,"CREATE TRIGGER reject_board BEFORE UPDATE ON boards BEGIN SELECT RAISE(ABORT,'board'); END");
    assert(!wena_board_settings_mutation_save_request(&adapter,"b",1,1,20));sql(database,"DROP TRIGGER reject_board");
    assert(number(database,"SELECT count(*) FROM board_settings")==0 && number(database,"SELECT version FROM boards WHERE id='b'")==1);
    sql(database,"CREATE TRIGGER reject_key BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'key'); END");
    assert(!wena_board_settings_mutation_save_request(&adapter,"b",1,1,20));sql(database,"DROP TRIGGER reject_key");
    assert(number(database,"SELECT count(*) FROM board_settings")==0 && number(database,"SELECT version FROM boards WHERE id='b'")==1);
    assert(wena_board_settings_mutation_save_request(&adapter,"b",1,1,20));
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));
    assert(snapshot.show_checklist_count && snapshot.board_version==2);
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==1);
    /* Display preference changes neither active nor archived card content. */
    assert(number(database,"SELECT count(*) FROM cards WHERE version=1")==2);
    assert(number(database,"SELECT archived FROM cards WHERE id='archived'")==1);
    assert(number(database,"SELECT version FROM boards WHERE id='foreign'")==1);
    assert(!wena_board_settings_mutation_save_request(&adapter,"b",2,0,20));
    assert(wena_board_settings_mutation_save(&adapter,"b",2,1));
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==1);
    assert(!wena_board_settings_mutation_save(&adapter,"b",1,1));
    for (index=0;index<sizeof(invalid)/sizeof(invalid[0]);++index) {
        raw(&command,snapshot.board_version,invalid[index]);
        assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
        assert(response.region_count==0);
    }
    raw(&command,snapshot.board_version,"0");
    assert(!wena_sqlite_board_settings_change(database,&command,"b",&output));
    strcpy(command.route,"/b/foreign/native");assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    raw(&command,snapshot.board_version,"0");strcpy(command.user_id,"missing");assert(!wena_sqlite_persistence_apply(&adapter.persistence,&command,&response));
    /* A change on another connection invalidates a captured board revision. */
    version=snapshot.board_version;assert(sqlite3_open(argv[3],&second)==SQLITE_OK);
    assert(wena_board_settings_mutation_init(&other,second,"v","b"));
    assert(wena_board_settings_mutation_save(&other,"b",version,0));assert(sqlite3_close(second)==SQLITE_OK);
    assert(!wena_board_settings_mutation_save(&adapter,"b",version,1));
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!snapshot.show_checklist_count && snapshot.board_version==version+1);
    /* Existing-row update failures retain the previous value and metadata. */
    version=snapshot.board_version;keys=number(database,"SELECT count(*) FROM idempotency_keys");
    sql(database,"CREATE TRIGGER reject_setting BEFORE UPDATE ON board_settings BEGIN SELECT RAISE(ABORT,'setting'); END");
    assert(!wena_board_settings_mutation_save(&adapter,"b",version,1));sql(database,"DROP TRIGGER reject_setting");
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!snapshot.show_checklist_count && snapshot.board_version==version);
    assert(wena_board_settings_mutation_save(&adapter,"b",version,1));assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));
    before=snapshot;assert(sqlite3_close(database)==SQLITE_OK);assert(sqlite3_open(argv[3],&database)==SQLITE_OK);
    assert(wena_board_settings_mutation_init(&adapter,database,"u","b"));
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!memcmp(&snapshot,&before,sizeof(snapshot)));
    assert(!wena_board_settings_mutation_save_request(&adapter,"b",snapshot.board_version,0,20));
    /* Malformed stored booleans fail atomically, rather than coercing to true. */
    sql(database,"PRAGMA ignore_check_constraints=ON;UPDATE board_settings SET show_checklist_count=2 WHERE board_id='b'");
    before=snapshot;assert(!wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!memcmp(&snapshot,&before,sizeof(snapshot)));
    assert(!wena_board_settings_mutation_save(&adapter,"b",snapshot.board_version,0));
    sql(database,"UPDATE board_settings SET show_checklist_count='bad' WHERE board_id='b'");
    assert(!wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!memcmp(&snapshot,&before,sizeof(snapshot)));
    sql(database,"UPDATE board_settings SET show_checklist_count=1.5 WHERE board_id='b'");
    assert(!wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(!memcmp(&snapshot,&before,sizeof(snapshot)));
    sql(database,"UPDATE board_settings SET show_checklist_count=1 WHERE board_id='b';PRAGMA ignore_check_constraints=OFF");
    display_settings(database);
    /* Terminal readable version never overflows into an unreadable state. */
    sprintf(query,"UPDATE boards SET version=%lu WHERE id='b'",WENA_VERSION_MUTATE_MAX);sql(database,query);
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));
    assert(wena_board_settings_mutation_save(&adapter,"b",snapshot.board_version,0));
    assert(wena_board_settings_mutation_load(&adapter,"b",&snapshot));assert(snapshot.board_version==WENA_VERSION_READ_MAX && !snapshot.show_checklist_count);
    keys=number(database,"SELECT count(*) FROM idempotency_keys");
    assert(!wena_board_settings_mutation_save(&adapter,"b",snapshot.board_version,1));
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    assert(sqlite3_close(database)==SQLITE_OK);
    puts("board settings default, strict booleans, optimistic/replay/no-op, rollback and reopen passed");
    return 0;
}
