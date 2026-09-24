#include "list_wip.h"
#include "../list_state.h"
#include "../sqlite_storage.h"
#include "../../models/model.h"
#include <string.h>

int wena_sqlite_list_wip_read(sqlite3 *db,const char *board,const char *list,
    unsigned long expected,WenaWipLimit *output)
{
    sqlite3_stmt *s;WenaWipLimit candidate;int ok;sqlite3_int64 value;
    const unsigned char *scope;
    if(!db||!output||!expected||expected>WENA_VERSION_READ_MAX||
        !wena_model_identifier_valid(board)||!wena_model_identifier_valid(list)||
        wena_sqlite_optional_table(db,"list_wip_limits",12)!=1)return 0;
    if(sqlite3_prepare_v2(db,"SELECT l.version,w.list_id,w.board_id,w.value,w.enabled,w.soft "
        "FROM lists l LEFT JOIN list_wip_limits w ON w.list_id=l.id WHERE l.id=?1 AND l.board_id=?2",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_int64(s,0)==(sqlite3_int64)expected;
    wena_wip_limit_init(&candidate);
    if(ok&&sqlite3_column_type(s,1)!=SQLITE_NULL){
        ok=sqlite3_column_type(s,1)==SQLITE_TEXT&&sqlite3_column_type(s,2)==SQLITE_TEXT&&
            sqlite3_column_type(s,3)==SQLITE_INTEGER&&sqlite3_column_type(s,4)==SQLITE_INTEGER&&
            sqlite3_column_type(s,5)==SQLITE_INTEGER;
        scope=sqlite3_column_text(s,2);value=sqlite3_column_int64(s,3);
        ok=ok&&scope&&sqlite3_column_bytes(s,2)==(int)strlen(board)&&!strcmp((const char*)scope,board)&&
            value>=1&&value<=2147483647&&
            sqlite3_column_int64(s,4)>=0&&sqlite3_column_int64(s,4)<=1&&
            sqlite3_column_int64(s,5)>=0&&sqlite3_column_int64(s,5)<=1;
        if(ok){candidate.value=(size_t)value;candidate.enabled=sqlite3_column_int(s,4);candidate.soft=sqlite3_column_int(s,5);}
    }
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)*output=candidate;
    return ok;
}
int wena_sqlite_list_wip_count(sqlite3 *db,const char *board,const char *list,size_t *output)
{
    sqlite3_stmt *s;int ok;sqlite3_int64 count;
    if(!db||!output||!wena_model_identifier_valid(board)||!wena_model_identifier_valid(list))return 0;
    if(sqlite3_prepare_v2(db,"SELECT coalesce(sum(CASE WHEN archived=0 THEN 1 ELSE 0 END),0),"
        "coalesce(sum(CASE WHEN typeof(board_id)='text' AND board_id=?2 AND typeof(archived)='integer' "
        "AND archived IN (0,1) THEN 0 ELSE 1 END),0) FROM cards WHERE list_id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_type(s,1)==SQLITE_INTEGER&&sqlite3_column_int64(s,1)==0;
    count=sqlite3_column_int64(s,0);ok=ok&&count>=0&&count<=2147483647;
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)*output=(size_t)count;
    return ok;
}
static int same(const WenaWipLimit *a,const WenaWipLimit *b)
{return a->value==b->value&&a->enabled==b->enabled&&a->soft==b->soft;}
int wena_sqlite_list_wip_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version,WenaWipLimit *result_limit)
{
    char list[WENA_ID_CAPACITY],text[32],action[16];unsigned long expected,value;
    WenaWipLimit before,desired,after;WenaWipEdit edit;size_t count,post_count;sqlite3_stmt *s;int ok;
    if(!db||!command||!result_version||!result_limit||sqlite3_get_autocommit(db)||
        command->operation!=WENA_DOMAIN_EDIT_LIST_WIP||!wena_model_identifier_valid(board)||
        !wena_mutation_text(command,"listId",list,sizeof(list),0)||!wena_model_identifier_valid(list)||
        !wena_mutation_text(command,"expectedVersion",text,sizeof(text),0)||
        !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected)||
        !wena_mutation_text(command,"action",action,sizeof(action),0))return 0;
    value=0;
    if(!strcmp(action,"value")){
        edit=WENA_WIP_APPLY_VALUE;
        if(!wena_mutation_text(command,"value",text,sizeof(text),0)||!wena_mutation_decimal(text,0,99,&value))return 0;
    }else if(!strcmp(action,"enabled"))edit=WENA_WIP_TOGGLE_ENABLED;
    else if(!strcmp(action,"soft"))edit=WENA_WIP_TOGGLE_SOFT;
    else return 0;
    if(!wena_sqlite_list_active(db,board,list)||!wena_sqlite_list_wip_read(db,board,list,expected,&before)||
        !wena_sqlite_list_wip_count(db,board,list,&count)||!wena_wip_edit(&before,edit,(size_t)value,count,&desired))return 0;
    if(same(&before,&desired)){*result_version=expected;*result_limit=desired;return 2;}
    if(sqlite3_prepare_v2(db,"INSERT INTO list_wip_limits(list_id,board_id,value,enabled,soft) VALUES(?1,?2,?3,?4,?5) "
        "ON CONFLICT(list_id) DO UPDATE SET value=excluded.value,enabled=excluded.enabled,soft=excluded.soft "
        "WHERE list_wip_limits.board_id=excluded.board_id",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int64(s,3,(sqlite3_int64)desired.value)==SQLITE_OK&&sqlite3_bind_int(s,4,desired.enabled)==SQLITE_OK&&
        sqlite3_bind_int(s,5,desired.soft)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(!ok||sqlite3_prepare_v2(db,"UPDATE lists SET version=version+1 WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int64(s,3,(sqlite3_int64)expected)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(!ok||!wena_sqlite_list_wip_read(db,board,list,expected+1,&after)||!same(&after,&desired)||
        !wena_sqlite_list_active(db,board,list)||!wena_sqlite_list_wip_count(db,board,list,&post_count)||post_count!=count)return 0;
    *result_version=expected+1;*result_limit=after;return 1;
}

int wena_sqlite_list_wip_check(sqlite3 *db,const char *board,const char *list,
    int increase,int applied)
{
    sqlite3_stmt *s;sqlite3_int64 version;WenaWipLimit limit;WenaWipDecision decision;
    size_t count;int available,ok;
    if(!db||sqlite3_get_autocommit(db)||!wena_model_identifier_valid(board)||!wena_model_identifier_valid(list)||
        (increase!=0&&increase!=1)||(applied!=0&&applied!=1)||(!increase&&applied))return 0;
    available=wena_sqlite_optional_table(db,"list_wip_limits",12);if(available<=0)return available==0;
    if(sqlite3_prepare_v2(db,"SELECT version FROM lists WHERE id=?1 AND board_id=?2",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_type(s,0)==SQLITE_INTEGER;
    version=sqlite3_column_int64(s,0);ok=ok&&version>0&&version<=(sqlite3_int64)WENA_VERSION_READ_MAX;
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(!ok||!wena_sqlite_list_wip_read(db,board,list,(unsigned long)version,&limit)||
        !wena_sqlite_list_wip_count(db,board,list,&count)||(applied&&!count))return 0;
    if(applied)--count;
    return wena_wip_evaluate(&limit,count,0,(size_t)increase,&decision)&&decision.allowed;
}
