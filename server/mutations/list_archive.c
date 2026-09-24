#include "list_archive.h"
#include "../../models/model.h"
#include <string.h>
static int read_state(sqlite3 *db,const char *board,const char *list,unsigned long version,
    int *archived,sqlite3_int64 *at)
{
    sqlite3_stmt *s;int ok;const unsigned char *scope;
    const char *sql="SELECT l.version,a.archived,a.archived_at,a.list_id,a.board_id FROM lists l "
        "LEFT JOIN list_archive_state a ON a.list_id=l.id WHERE l.id=?1 AND l.board_id=?2";
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_int64(s,0)==(sqlite3_int64)version;
    *archived=0;*at=0;
    if(ok&&sqlite3_column_type(s,3)!=SQLITE_NULL){
        scope=sqlite3_column_text(s,4);
        ok=sqlite3_column_type(s,4)==SQLITE_TEXT&&scope&&
            sqlite3_column_bytes(s,4)==(int)strlen(board)&&!strcmp((const char*)scope,board)&&
            sqlite3_column_type(s,1)==SQLITE_INTEGER&&(sqlite3_column_int64(s,1)==0||sqlite3_column_int64(s,1)==1)&&
            sqlite3_column_type(s,2)==SQLITE_INTEGER&&sqlite3_column_int64(s,2)>=0;
        if(ok){*archived=sqlite3_column_int(s,1);*at=sqlite3_column_int64(s,2);}
    }
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    return ok;
}
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
        !read_state(db,board,list,expected,&stored,&before))return 0;
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
    if(!valid||!read_state(db,board,list,expected+1,&stored,&after)||stored!=desired||
        (desired?(after<=0||after<before):after!=before))return 0;
    *result_version=expected+1;return 1;
}
