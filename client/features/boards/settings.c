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
    memset(&candidate,0,sizeof(candidate));
    if (sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT b.version,s.board_id,s.show_checklist_count FROM boards b "
        "LEFT JOIN board_settings s ON s.board_id=b.id WHERE b.id=?1 AND "
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
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid) return 0;
    strcpy(candidate.board_id,board_id);candidate.board_version=(unsigned long)version;
    if (!wena_board_settings_snapshot_valid(&candidate,board_id)) return 0;
    *output=candidate;
    return 1;
}
int wena_board_settings_mutation_save_request(WenaBoardSettingsMutation *adapter,
    const char *board_id,unsigned long expected_board_version,
    int show_checklist_count,unsigned long request_version)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    if (!scope_valid(adapter,board_id) || !expected_board_version ||
        expected_board_version>WENA_VERSION_MUTATE_MAX || !request_version ||
        request_version>=(unsigned long)LONG_MAX ||
        (show_checklist_count!=0 && show_checklist_count!=1)) return 0;
    memset(&command,0,sizeof(command));
    command.operation=WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT;
    command.request_version=request_version;
    strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"expectedBoardVersion=%lu&showChecklistCount=%d",
        expected_board_version,show_checklist_count);
    command.form_body_length=strlen(command.form_body);
    return wena_sqlite_persistence_apply(&adapter->persistence,&command,&response);
}
int wena_board_settings_mutation_save(void *context,const char *board_id,
    unsigned long expected_board_version,int show_checklist_count)
{
    WenaBoardSettingsMutation *adapter;
    sqlite3_stmt *statement;
    sqlite3_int64 version;
    int valid;
    adapter=(WenaBoardSettingsMutation *)context;
    if (!scope_valid(adapter,board_id)) return 0;
    if (sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE "
        "actor_id=?1 AND route=?2 AND operation='set-board-checklist-count'",
        -1,&statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,2,adapter->route,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_ROW && sqlite3_column_type(statement,0)==SQLITE_INTEGER;
    version=valid?sqlite3_column_int64(statement,0):-1;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid || version<0 || version>=(sqlite3_int64)LONG_MAX-1) return 0;
    return wena_board_settings_mutation_save_request(adapter,board_id,
        expected_board_version,show_checklist_count,(unsigned long)version+1UL);
}
