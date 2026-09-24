#include "card_archive.h"
#include "../sqlite_storage.h"
#include "../../models/model.h"
#include <string.h>
int wena_sqlite_card_archive_read(sqlite3 *db,const char *board,const char *card,
    unsigned long expected,int *archived,sqlite3_int64 *at)
{
    sqlite3_stmt *s;int available,ok,flag;sqlite3_int64 time;const unsigned char *scope;
    const char *q="SELECT c.version,c.archived,a.card_id,a.board_id,a.archived_at FROM cards c "
        "LEFT JOIN card_archive_state a ON a.card_id=c.id WHERE c.id=?1 AND c.board_id=?2";
    if(!db||!archived||!at||expected>WENA_VERSION_READ_MAX||!wena_model_identifier_valid(board)||!wena_model_identifier_valid(card))return 0;
    available=wena_sqlite_optional_table(db,"card_archive_state",13);if(available<0)return 0;
    if(!available)q="SELECT version,archived,NULL,NULL,NULL FROM cards WHERE id=?1 AND board_id=?2";
    if(sqlite3_prepare_v2(db,q,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,card,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_int64(s,0)>0&&
        sqlite3_column_int64(s,0)<=(sqlite3_int64)WENA_VERSION_READ_MAX&&(!expected||sqlite3_column_int64(s,0)==(sqlite3_int64)expected)&&
        sqlite3_column_type(s,1)==SQLITE_INTEGER&&(sqlite3_column_int64(s,1)==0||sqlite3_column_int64(s,1)==1);
    flag=sqlite3_column_int(s,1);time=0;
    if(ok&&sqlite3_column_type(s,2)!=SQLITE_NULL){
        ok=sqlite3_column_type(s,2)==SQLITE_TEXT&&sqlite3_column_type(s,3)==SQLITE_TEXT&&sqlite3_column_type(s,4)==SQLITE_INTEGER;
        scope=sqlite3_column_text(s,3);time=sqlite3_column_int64(s,4);
        ok=ok&&scope&&sqlite3_column_bytes(s,3)==(int)strlen(board)&&!strcmp((const char*)scope,board)&&time>=0;
    }
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok){*archived=flag;*at=time;}return ok;
}
int wena_sqlite_archive_time_after(sqlite3 *db,sqlite3_int64 prior,sqlite3_int64 *at)
{
    sqlite3_stmt *s;int ok;sqlite3_int64 time;
    if(!db||!at||prior<0)return 0;
    if(sqlite3_prepare_v2(db,"SELECT CASE WHEN ?1<9223372036854775807 THEN max(CAST(strftime('%s','now') AS INTEGER)*1000,?1+1) END",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_int64(s,1,prior)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_type(s,0)==SQLITE_INTEGER;
    time=sqlite3_column_int64(s,0);ok=ok&&time>prior;
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)*at=time;return ok;
}
int wena_sqlite_card_archive_change(sqlite3 *db,const char *board,const char *card,
    unsigned long expected,int archived,sqlite3_int64 floor)
{
    sqlite3_stmt *s;int ok,available,before,after;sqlite3_int64 prior,desired,stored;
    if(!db||sqlite3_get_autocommit(db)||!expected||expected>WENA_VERSION_MUTATE_MAX||floor<0||
        (archived!=0&&archived!=1)||!wena_sqlite_card_archive_read(db,board,card,expected,&before,&prior)||before==archived)return 0;
    available=wena_sqlite_optional_table(db,"card_archive_state",13);if(available<0)return 0;
    desired=prior;
    if(available&&archived){
        if(!wena_sqlite_archive_time_after(db,prior>floor?prior:floor,&desired))return 0;
        if(sqlite3_prepare_v2(db,"INSERT INTO card_archive_state(card_id,board_id,archived_at) VALUES(?1,?2,?3) "
            "ON CONFLICT(card_id) DO UPDATE SET archived_at=excluded.archived_at WHERE card_archive_state.board_id=excluded.board_id",-1,&s,NULL)!=SQLITE_OK)return 0;
        ok=sqlite3_bind_text(s,1,card,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
            sqlite3_bind_int64(s,3,desired)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
        if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;if(!ok)return 0;
    }
    if(sqlite3_prepare_v2(db,"UPDATE cards SET archived=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4 AND archived=?5",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_int(s,1,archived)==SQLITE_OK&&sqlite3_bind_text(s,2,card,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,3,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_int64(s,4,(sqlite3_int64)expected)==SQLITE_OK&&
        sqlite3_bind_int(s,5,before)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    return ok&&wena_sqlite_card_archive_read(db,board,card,expected+1,&after,&stored)&&after==archived&&stored==desired;
}
