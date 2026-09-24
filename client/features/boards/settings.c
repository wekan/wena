#include "settings.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int scope_valid(WenaBoardSettingsMutation *adapter,const char *board_id)
{
    return adapter && adapter->persistence.database &&
        wena_model_identifier_valid(board_id) && !strcmp(adapter->board_id,board_id);
}
int wena_board_settings_mutation_init(WenaBoardSettingsMutation *adapter,
    sqlite3 *database,const char *actor_id,const char *board_id)
{
    if (!adapter) return 0;
    memset(adapter,0,sizeof(*adapter));
    if (!database || !wena_model_identifier_valid(actor_id) ||
        !wena_model_identifier_valid(board_id)) return 0;
    wena_sqlite_persistence_init(&adapter->persistence,database);
    strcpy(adapter->actor_id,actor_id);strcpy(adapter->board_id,board_id);
    sprintf(adapter->route,"/b/%s/native",board_id);
    return 1;
}
int wena_board_settings_mutation_load(void *context,const char *board_id,
    WenaBoardSettingsSnapshot *output)
{
    WenaBoardSettingsMutation *adapter;
    WenaBoardSettingsSnapshot candidate;
    sqlite3_stmt *statement;
    sqlite3_int64 version;
    int valid;
    adapter=(WenaBoardSettingsMutation *)context;
    if (!scope_valid(adapter,board_id) || !output) return 0;
    memset(&candidate,0,sizeof(candidate));candidate.show_checklists=1;candidate.allow_minicard_collapse=1;
    if (sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT b.version,s.board_id,s.show_checklist_count,m.board_id,m.show_checklists,c.board_id,c.allow_collapse FROM boards b "
        "LEFT JOIN board_settings s ON s.board_id=b.id LEFT JOIN board_minicard_settings m ON m.board_id=b.id LEFT JOIN board_card_collapse_settings c ON c.board_id=b.id WHERE b.id=?1 AND "
        "EXISTS(SELECT 1 FROM actors WHERE id=?2)",-1,&statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,2,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_ROW && sqlite3_column_type(statement,0)==SQLITE_INTEGER;
    version=valid?sqlite3_column_int64(statement,0):0;
    valid=valid && version>0 && version<=(sqlite3_int64)WENA_VERSION_READ_MAX;
    if (valid && sqlite3_column_type(statement,1)!=SQLITE_NULL) {
        valid=sqlite3_column_type(statement,1)==SQLITE_TEXT &&
            sqlite3_column_type(statement,2)==SQLITE_INTEGER &&
            (sqlite3_column_int64(statement,2)==0 || sqlite3_column_int64(statement,2)==1);
        if (valid) candidate.show_checklist_count=sqlite3_column_int(statement,2);
    }
    if (valid && sqlite3_column_type(statement,3)!=SQLITE_NULL) {
        valid=sqlite3_column_type(statement,3)==SQLITE_TEXT &&
            sqlite3_column_type(statement,4)==SQLITE_INTEGER &&
            (sqlite3_column_int64(statement,4)==0 || sqlite3_column_int64(statement,4)==1);
        if (valid) candidate.show_checklists=sqlite3_column_int(statement,4);
    }
    if (valid && sqlite3_column_type(statement,5)!=SQLITE_NULL) {
        valid=sqlite3_column_type(statement,5)==SQLITE_TEXT &&
            sqlite3_column_type(statement,6)==SQLITE_INTEGER &&
            (sqlite3_column_int64(statement,6)==0 || sqlite3_column_int64(statement,6)==1);
        if (valid) candidate.allow_minicard_collapse=sqlite3_column_int(statement,6);
    }
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid) return 0;
    strcpy(candidate.board_id,board_id);candidate.board_version=(unsigned long)version;
    if (!wena_board_settings_snapshot_valid(&candidate,board_id)) return 0;
    *output=candidate;
    return 1;
}
static int save_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long expected_board_version,
    int show_checklist_count,int show_checklists,int collapse,int display,unsigned long request_version)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    if (!scope_valid(adapter,board_id) || !expected_board_version ||
        expected_board_version>WENA_VERSION_MUTATE_MAX || !request_version ||
        request_version>=(unsigned long)LONG_MAX ||
        (show_checklist_count!=0 && show_checklist_count!=1) ||
        (show_checklists!=0 && show_checklists!=1) ||
        (display==2 && collapse!=0 && collapse!=1)) return 0;
    memset(&command,0,sizeof(command));
    command.operation=display ? WENA_DOMAIN_SET_BOARD_PRESENTATION : WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT;
    command.request_version=request_version;
    strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"expectedBoardVersion=%lu&showChecklistCount=%d",
        expected_board_version,show_checklist_count);
    if (display) sprintf(command.form_body+strlen(command.form_body),
        "&showChecklists=%d",show_checklists);
    if (display==2) sprintf(command.form_body+strlen(command.form_body),
        "&allowMinicardCollapse=%d",collapse);
    command.form_body_length=strlen(command.form_body);
    return wena_sqlite_persistence_apply(&adapter->persistence,&command,&response);
}
static int save(void *context,const char *board_id,
    unsigned long expected_board_version,int show_checklist_count,int show_checklists,int collapse,int display)
{
    WenaBoardSettingsMutation *adapter;
    sqlite3_stmt *statement;
    sqlite3_int64 version;
    int valid;
    adapter=(WenaBoardSettingsMutation *)context;
    if (!scope_valid(adapter,board_id)) return 0;
    if (sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE "
        "actor_id=?1 AND route=?2 AND operation=?3",
        -1,&statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,2,adapter->route,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,3,display ? "set-board-presentation" :
            "set-board-checklist-count",-1,SQLITE_STATIC)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_ROW && sqlite3_column_type(statement,0)==SQLITE_INTEGER;
    version=valid?sqlite3_column_int64(statement,0):-1;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid || version<0 || version>=(sqlite3_int64)LONG_MAX-1) return 0;
    return save_request(adapter,board_id,
        expected_board_version,show_checklist_count,show_checklists,collapse,display,(unsigned long)version+1UL);
}

int wena_board_settings_mutation_save_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long version,int count,unsigned long request)
{ return save_request(adapter,board_id,version,count,1,1,0,request); }
int wena_board_settings_mutation_save(void *context,const char *board_id,
    unsigned long version,int count)
{ return save(context,board_id,version,count,1,1,0); }
int wena_board_settings_mutation_save_display_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long version,int count,int contents,unsigned long request)
{ return save_request(adapter,board_id,version,count,contents,1,1,request); }
int wena_board_settings_mutation_save_display(void *context,const char *board_id,
    unsigned long version,int count,int contents)
{ return save(context,board_id,version,count,contents,1,1); }

int wena_board_settings_mutation_save_all(void *context,const char *board_id,
    unsigned long version,int count,int contents,int collapse)
{ return save(context,board_id,version,count,contents,collapse,2); }
int wena_board_settings_mutation_save_all_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long version,int count,int contents,int collapse,unsigned long request)
{ return save_request(adapter,board_id,version,count,contents,collapse,2,request); }
