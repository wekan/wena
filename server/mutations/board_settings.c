#include "board_settings.h"
#include "../../models/model.h"
#include <string.h>

static int requested_flag(const WenaDomainCommand *command, const char *name, int *value)
{
    char text[8];
    if (!wena_mutation_text(command,name,text,sizeof(text),0)) return 0;
    if (!strcmp(text,"0")) *value=0;
    else if (!strcmp(text,"1")) *value=1;
    else return 0;
    return 1;
}
static int stored_flag(sqlite3_stmt *statement,int column,int presence,int default_value,int *value)
{
    if (sqlite3_column_type(statement,presence)==SQLITE_NULL) { *value=default_value; return 1; }
    if (sqlite3_column_type(statement,column)!=SQLITE_INTEGER ||
        (sqlite3_column_int64(statement,column)!=0 && sqlite3_column_int64(statement,column)!=1)) return 0;
    *value=sqlite3_column_int(statement,column);return 1;
}
/* SQL is selected internally, never supplied by form data. Shared guarded
 * boolean persistence serves both independent display preferences. */
static int write_flag(sqlite3 *database,const char *board_id,int value,
    const char *write_sql,const char *read_sql)
{
    sqlite3_stmt *statement;
    int valid;
    if (sqlite3_prepare_v2(database,write_sql,-1,&statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_int(statement,2,value)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_DONE && sqlite3_changes(database)==1;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid || sqlite3_prepare_v2(database,read_sql,-1,&statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_ROW && sqlite3_column_type(statement,0)==SQLITE_INTEGER &&
        sqlite3_column_int64(statement,0)==value && sqlite3_step(statement)==SQLITE_DONE;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    return valid;
}
static int read_settings(sqlite3 *database,const char *board_id,unsigned long expected,
    int *stored,int *stored_contents,int *stored_collapse)
{
    sqlite3_stmt *statement;
    int valid;
    if (sqlite3_prepare_v2(database,
        "SELECT b.version,s.show_checklist_count,m.show_checklists,c.allow_collapse,s.board_id,m.board_id,c.board_id FROM boards b "
        "LEFT JOIN board_settings s ON s.board_id=b.id "
        "LEFT JOIN board_minicard_settings m ON m.board_id=b.id LEFT JOIN board_card_collapse_settings c ON c.board_id=b.id WHERE b.id=?1",-1,
        &statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_ROW &&
        sqlite3_column_type(statement,0)==SQLITE_INTEGER &&
        sqlite3_column_int64(statement,0)==(sqlite3_int64)expected &&
        stored_flag(statement,1,4,0,stored) && stored_flag(statement,2,5,1,stored_contents) &&
        stored_flag(statement,3,6,1,stored_collapse);
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    return valid;
}
int wena_sqlite_board_settings_change(sqlite3 *database,
    const WenaDomainCommand *command,const char *board_id,
    unsigned long *result_version)
{
    sqlite3_stmt *statement;
    char text[32];
    unsigned long expected;
    int requested,stored,requested_contents,stored_contents,valid,display,requested_collapse,stored_collapse;
    if (!database || !command || !result_version || sqlite3_get_autocommit(database) ||
        (command->operation!=WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT &&
         command->operation!=WENA_DOMAIN_SET_BOARD_PRESENTATION) ||
        !wena_model_identifier_valid(board_id) ||
        !wena_mutation_text(command,"expectedBoardVersion",text,sizeof(text),0) ||
        !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected) ||
        !requested_flag(command,"showChecklistCount",&requested)) return 0;
    display=command->operation==WENA_DOMAIN_SET_BOARD_PRESENTATION;
    requested_contents=1;stored_contents=1;
    if (display && !requested_flag(command,"showChecklists",&requested_contents)) return 0;
    if (!read_settings(database,board_id,expected,&stored,&stored_contents,&stored_collapse)) return 0;
    requested_collapse=stored_collapse;
    if (display && wena_mutation_has_value(command,"allowMinicardCollapse") &&
        !requested_flag(command,"allowMinicardCollapse",&requested_collapse)) return 0;
    if (!display) requested_contents=stored_contents;
    if (stored==requested && stored_contents==requested_contents && stored_collapse==requested_collapse) {*result_version=expected;return 2;}
    if (stored!=requested && !write_flag(database,board_id,requested,
        "INSERT INTO board_settings(board_id,show_checklist_count) VALUES(?1,?2) "
        "ON CONFLICT(board_id) DO UPDATE SET show_checklist_count=excluded.show_checklist_count",
        "SELECT show_checklist_count FROM board_settings WHERE board_id=?1")) return 0;
    if (stored_contents!=requested_contents && !write_flag(database,board_id,requested_contents,
        "INSERT INTO board_minicard_settings(board_id,show_checklists) VALUES(?1,?2) "
        "ON CONFLICT(board_id) DO UPDATE SET show_checklists=excluded.show_checklists",
        "SELECT show_checklists FROM board_minicard_settings WHERE board_id=?1")) return 0;
    if (stored_collapse!=requested_collapse && !write_flag(database,board_id,requested_collapse,
        "INSERT INTO board_card_collapse_settings(board_id,allow_collapse) VALUES(?1,?2) "
        "ON CONFLICT(board_id) DO UPDATE SET allow_collapse=excluded.allow_collapse",
        "SELECT allow_collapse FROM board_card_collapse_settings WHERE board_id=?1")) return 0;
    if (sqlite3_prepare_v2(database,
        "UPDATE boards SET version=version+1 WHERE id=?1 AND version=?2",-1,
        &statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_int64(statement,2,(sqlite3_int64)expected)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_DONE && sqlite3_changes(database)==1;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid) return 0;
    if (!read_settings(database,board_id,expected+1UL,&stored,&stored_contents,&stored_collapse) ||
        stored!=requested || stored_contents!=requested_contents || stored_collapse!=requested_collapse) return 0;
    *result_version=expected+1UL;
    return 1;
}
