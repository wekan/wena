#include "board_settings.h"
#include "../../models/model.h"
#include <string.h>

int wena_sqlite_board_settings_change(sqlite3 *database,
    const WenaDomainCommand *command,const char *board_id,
    unsigned long *result_version)
{
    sqlite3_stmt *statement;
    char text[32];
    unsigned long expected;
    int requested,stored,valid;
    if (!database || !command || !result_version || sqlite3_get_autocommit(database) ||
        command->operation!=WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT ||
        !wena_model_identifier_valid(board_id) ||
        !wena_mutation_text(command,"expectedBoardVersion",text,sizeof(text),0) ||
        !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected) ||
        !wena_mutation_text(command,"showChecklistCount",text,sizeof(text),0)) return 0;
    if (!strcmp(text,"0")) requested=0;
    else if (!strcmp(text,"1")) requested=1;
    else return 0;
    if (sqlite3_prepare_v2(database,
        "SELECT b.version,s.board_id,s.show_checklist_count FROM boards b "
        "LEFT JOIN board_settings s ON s.board_id=b.id WHERE b.id=?1",-1,
        &statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_ROW &&
        sqlite3_column_type(statement,0)==SQLITE_INTEGER &&
        sqlite3_column_int64(statement,0)==(sqlite3_int64)expected;
    stored=0;
    if (valid && sqlite3_column_type(statement,1)!=SQLITE_NULL) {
        valid=sqlite3_column_type(statement,1)==SQLITE_TEXT &&
            sqlite3_column_type(statement,2)==SQLITE_INTEGER &&
            (sqlite3_column_int64(statement,2)==0 || sqlite3_column_int64(statement,2)==1);
        if (valid) stored=sqlite3_column_int(statement,2);
    }
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid) return 0;
    if (stored==requested) {*result_version=expected;return 2;}
    if (sqlite3_prepare_v2(database,
        "INSERT INTO board_settings(board_id,show_checklist_count) VALUES(?1,?2) "
        "ON CONFLICT(board_id) DO UPDATE SET show_checklist_count=excluded.show_checklist_count",
        -1,&statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_int(statement,2,requested)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_DONE && sqlite3_changes(database)==1;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid) return 0;
    if (sqlite3_prepare_v2(database,
        "UPDATE boards SET version=version+1 WHERE id=?1 AND version=?2",-1,
        &statement,NULL)!=SQLITE_OK) return 0;
    valid=sqlite3_bind_text(statement,1,board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_int64(statement,2,(sqlite3_int64)expected)==SQLITE_OK &&
        sqlite3_step(statement)==SQLITE_DONE && sqlite3_changes(database)==1;
    if (sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (!valid) return 0;
    *result_version=expected+1UL;
    return 1;
}
