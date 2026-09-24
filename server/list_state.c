#include "sqlite_storage.h"
#include "list_state.h"
#include "../models/model.h"
#include <string.h>
int wena_sqlite_list_state_available(sqlite3 *db)
{
    return wena_sqlite_optional_table(db,"list_archive_state",10);
}

static int state_read(sqlite3 *db,const char *board,const char *list,unsigned long version,
    int *archived,sqlite3_int64 *at,int lane)
{
    sqlite3_stmt *s;int ok,available,flag;sqlite3_int64 time;const unsigned char *scope;
    const char *sql="SELECT l.version,a.archived,a.archived_at,a.list_id,a.board_id FROM lists l "
        "LEFT JOIN list_archive_state a ON a.list_id=l.id WHERE l.id=?1 AND l.board_id=?2";
    if(!db||!archived||!at||!wena_model_identifier_valid(board)||!wena_model_identifier_valid(list))return 0;
    available=wena_sqlite_optional_table(db,lane?"swimlane_archive_state":"list_archive_state",lane?13:10);if(available<0)return 0;
    if(lane)sql="SELECT l.version,a.archived,a.archived_at,a.swimlane_id,a.board_id FROM swimlanes l LEFT JOIN swimlane_archive_state a ON a.swimlane_id=l.id WHERE l.id=?1 AND l.board_id=?2";
    if(!available)sql=lane?"SELECT version,NULL,NULL,NULL,NULL FROM swimlanes WHERE id=?1 AND board_id=?2":"SELECT version,NULL,NULL,NULL,NULL FROM lists WHERE id=?1 AND board_id=?2";
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_int64(s,0)>0&&
        sqlite3_column_int64(s,0)<=(sqlite3_int64)WENA_VERSION_READ_MAX&&
        (!version||sqlite3_column_int64(s,0)==(sqlite3_int64)version);
    flag=0;time=0;
    if(ok&&sqlite3_column_type(s,3)!=SQLITE_NULL){
        ok=sqlite3_column_type(s,4)==SQLITE_TEXT;
        scope=sqlite3_column_text(s,4);
        ok=ok&&scope&&
            sqlite3_column_bytes(s,4)==(int)strlen(board)&&!strcmp((const char*)scope,board)&&
            sqlite3_column_type(s,1)==SQLITE_INTEGER&&(sqlite3_column_int64(s,1)==0||sqlite3_column_int64(s,1)==1)&&
            sqlite3_column_type(s,2)==SQLITE_INTEGER&&sqlite3_column_int64(s,2)>=0;
        if(ok){flag=sqlite3_column_int(s,1);time=sqlite3_column_int64(s,2);}
    }
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok){*archived=flag;*at=time;}
    return ok;
}
int wena_sqlite_list_state_read(sqlite3 *db,const char *board,const char *list,unsigned long version,int *archived,sqlite3_int64 *at)
{return state_read(db,board,list,version,archived,at,0);}
int wena_sqlite_swimlane_state_read(sqlite3 *db,const char *board,const char *lane,unsigned long version,int *archived,sqlite3_int64 *at)
{return state_read(db,board,lane,version,archived,at,1);}
int wena_sqlite_swimlane_active(sqlite3 *db,const char *board,const char *lane)
{int archived;sqlite3_int64 at;return state_read(db,board,lane,0,&archived,&at,1)&&!archived;}
int wena_sqlite_list_active(sqlite3 *db,const char *board,const char *list)
{
    int archived;sqlite3_int64 at;
    return wena_sqlite_list_state_read(db,board,list,0,&archived,&at)&&!archived;
}
