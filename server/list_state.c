#include "list_state.h"
#include "../models/model.h"
#include <string.h>
/* Legacy snapshots have no archive table. A v10-or-newer database must have it;
 * disappearance or a same-named view is corruption, not an unarchived default. */
int wena_sqlite_list_state_available(sqlite3 *db)
{
    sqlite3_stmt *s;int result,available;const unsigned char *type;
    if(sqlite3_prepare_v2(db,"SELECT type FROM sqlite_schema WHERE name='list_archive_state'",-1,&s,NULL)!=SQLITE_OK)return -1;
    result=sqlite3_step(s);available=-1;
    if(result==SQLITE_ROW){type=sqlite3_column_text(s,0);
        if(type&&!strcmp((const char*)type,"table")&&sqlite3_step(s)==SQLITE_DONE)available=1;}
    else if(result==SQLITE_DONE)available=0;
    if(sqlite3_finalize(s)!=SQLITE_OK)return -1;
    if(available)return available;
    if(sqlite3_prepare_v2(db,"PRAGMA user_version",-1,&s,NULL)!=SQLITE_OK)return -1;
    available=sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_type(s,0)==SQLITE_INTEGER&&
        sqlite3_column_int64(s,0)>=0&&sqlite3_column_int64(s,0)<10?0:-1;
    if(sqlite3_finalize(s)!=SQLITE_OK)available=-1;
    return available;
}

int wena_sqlite_list_state_read(sqlite3 *db,const char *board,const char *list,unsigned long version,
    int *archived,sqlite3_int64 *at)
{
    sqlite3_stmt *s;int ok,available;const unsigned char *scope;
    const char *sql="SELECT l.version,a.archived,a.archived_at,a.list_id,a.board_id FROM lists l "
        "LEFT JOIN list_archive_state a ON a.list_id=l.id WHERE l.id=?1 AND l.board_id=?2";
    if(!db||!archived||!at||!wena_model_identifier_valid(board)||!wena_model_identifier_valid(list))return 0;
    available=wena_sqlite_list_state_available(db);if(available<0)return 0;
    if(!available)sql="SELECT version,NULL,NULL,NULL,NULL FROM lists WHERE id=?1 AND board_id=?2";
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_int64(s,0)>0&&
        sqlite3_column_int64(s,0)<=(sqlite3_int64)WENA_VERSION_READ_MAX&&
        (!version||sqlite3_column_int64(s,0)==(sqlite3_int64)version);
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
int wena_sqlite_list_active(sqlite3 *db,const char *board,const char *list)
{
    int archived;sqlite3_int64 at;
    return wena_sqlite_list_state_read(db,board,list,0,&archived,&at)&&!archived;
}
