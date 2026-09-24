#include "../list_state.h"
#include "list_archive.h"
#include "../../models/model.h"
#include <string.h>
int wena_sqlite_list_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    char list[WENA_ID_CAPACITY],text[32];unsigned long expected;sqlite3_stmt *s;
    sqlite3_int64 before,after;int stored,desired,valid;
    const char *sql="INSERT INTO list_archive_state(list_id,board_id,archived,archived_at) "
        "VALUES(?1,?2,?3,CASE WHEN ?3=1 THEN CAST(strftime('%s','now') AS INTEGER)*1000 ELSE 0 END) "
        "ON CONFLICT(list_id) DO UPDATE SET archived=excluded.archived,archived_at="
        "CASE WHEN excluded.archived=1 THEN max(list_archive_state.archived_at,excluded.archived_at) "
        "ELSE list_archive_state.archived_at END WHERE list_archive_state.board_id=excluded.board_id";
    if(!db||!command||!result_version||sqlite3_get_autocommit(db)||!wena_model_identifier_valid(board)||
        (command->operation!=WENA_DOMAIN_ARCHIVE_LIST&&command->operation!=WENA_DOMAIN_RESTORE_LIST)||
        !wena_mutation_text(command,"listId",list,sizeof(list),0)||!wena_model_identifier_valid(list)||
        !wena_mutation_text(command,"expectedVersion",text,sizeof(text),0)||
        !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected)||
        !wena_sqlite_list_state_read(db,board,list,expected,&stored,&before))return 0;
    desired=command->operation==WENA_DOMAIN_ARCHIVE_LIST;
    if(stored==desired){*result_version=expected;return 2;}
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    valid=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_int(s,3,desired)==SQLITE_OK&&
        sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)valid=0;
    if(!valid||sqlite3_prepare_v2(db,"UPDATE lists SET version=version+1 WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)return 0;
    valid=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int64(s,3,(sqlite3_int64)expected)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)valid=0;
    if(!valid||!wena_sqlite_list_state_read(db,board,list,expected+1,&stored,&after)||stored!=desired||
        (desired?(after<=0||after<before):after!=before))return 0;
    *result_version=expected+1;return 1;
}
