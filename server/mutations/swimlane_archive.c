#include "swimlane_archive.h"
#include "card_archive.h"
#include "list_wip.h"
#include "../list_state.h"
#include "../sqlite_storage.h"
#include "../../models/card_order.h"
#include <stdlib.h>
#include <string.h>
typedef struct ArchiveCard {
    WenaId id,list;
    unsigned long version;
    int archived;
    sqlite3_int64 at;
} ArchiveCard;
static int id_column(sqlite3_stmt *s,int column,char id[WENA_ID_CAPACITY])
{
    const unsigned char *text;int bytes;
    if(sqlite3_column_type(s,column)!=SQLITE_TEXT)return 0;
    text=sqlite3_column_text(s,column);bytes=sqlite3_column_bytes(s,column);
    if(!text||bytes<1||bytes>=WENA_ID_CAPACITY||memchr(text,0,(size_t)bytes))return 0;
    memcpy(id,text,(size_t)bytes);id[bytes]=0;return wena_model_identifier_valid(id);
}
static int cards_read(sqlite3 *db,const char *board,const char *lane,ArchiveCard *cards,size_t *count)
{
    sqlite3_stmt *s;size_t used;int ok,rc;WenaId scope;sqlite3_int64 version,at;int archived;
    if(sqlite3_prepare_v2(db,"SELECT id,board_id,list_id,version FROM cards WHERE swimlane_id=?1 ORDER BY id",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,lane,-1,SQLITE_TRANSIENT)==SQLITE_OK;used=0;rc=SQLITE_ERROR;
    while(ok&&(rc=sqlite3_step(s))==SQLITE_ROW){
        if(used==WENA_CARD_ORDER_CAPACITY||!id_column(s,0,cards[used].id)||!id_column(s,1,scope)||strcmp(scope,board)||
            !id_column(s,2,cards[used].list)||sqlite3_column_type(s,3)!=SQLITE_INTEGER){ok=0;break;}
        version=sqlite3_column_int64(s,3);
        if(version<1||version>(sqlite3_int64)WENA_VERSION_READ_MAX||
            !wena_sqlite_list_state_read(db,board,cards[used].list,0,&archived,&at)||
            !wena_sqlite_card_archive_read(db,board,cards[used].id,(unsigned long)version,&cards[used].archived,&cards[used].at)){ok=0;break;}
        cards[used].version=(unsigned long)version;++used;
    }
    if(rc!=SQLITE_DONE)ok=0;if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)*count=used;return ok;
}
int wena_sqlite_swimlane_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    char lane[WENA_ID_CAPACITY],text[32];unsigned long expected;int stored,desired,ok,changed;
    sqlite3_int64 before,stamp,after,floor;ArchiveCard *cards,*verify;size_t count,final_count,i;
    sqlite3_stmt *s;
    if(!db||!command||!result_version||sqlite3_get_autocommit(db)||!wena_model_identifier_valid(board)||
        (command->operation!=WENA_DOMAIN_ARCHIVE_SWIMLANE&&command->operation!=WENA_DOMAIN_RESTORE_SWIMLANE)||
        wena_sqlite_optional_table(db,"swimlane_archive_state",13)!=1||wena_sqlite_optional_table(db,"card_archive_state",13)!=1||
        !wena_mutation_text(command,"swimlaneId",lane,sizeof(lane),0)||!wena_model_identifier_valid(lane)||
        !wena_mutation_text(command,"expectedVersion",text,sizeof(text),0)||!wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected)||
        !wena_sqlite_swimlane_state_read(db,board,lane,expected,&stored,&before))return 0;
    desired=command->operation==WENA_DOMAIN_ARCHIVE_SWIMLANE;
    if(stored==desired){*result_version=expected;return 2;}
    cards=(ArchiveCard*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*cards));verify=(ArchiveCard*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*verify));
    if(!cards||!verify){free(cards);free(verify);return 0;}
    ok=0;count=final_count=0;
    if(!cards_read(db,board,lane,cards,&count))goto done;
    stamp=before;floor=before;
    if(desired){
        for(i=0;i<count;++i)if(cards[i].at>floor)floor=cards[i].at;
        if(!wena_sqlite_archive_time_after(db,floor,&stamp))goto done;
    }
    for(i=0;i<count;++i){
        changed=desired?!cards[i].archived:cards[i].archived&&before>0&&cards[i].at>=before;
        if(!changed)continue;
        if(!wena_sqlite_card_archive_change(db,board,cards[i].id,cards[i].version,desired,desired?stamp:0)||
            (!desired&&!wena_sqlite_list_wip_check(db,board,cards[i].list,1,1)))goto done;
        ++cards[i].version;cards[i].archived=desired;
        if(!wena_sqlite_card_archive_read(db,board,cards[i].id,cards[i].version,&stored,&cards[i].at))goto done;
    }
    if(sqlite3_prepare_v2(db,"INSERT INTO swimlane_archive_state(swimlane_id,board_id,archived,archived_at) VALUES(?1,?2,?3,?4) "
        "ON CONFLICT(swimlane_id) DO UPDATE SET archived=excluded.archived,archived_at=excluded.archived_at WHERE swimlane_archive_state.board_id=excluded.board_id",-1,&s,NULL)!=SQLITE_OK)goto done;
    ok=sqlite3_bind_text(s,1,lane,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int(s,3,desired)==SQLITE_OK&&sqlite3_bind_int64(s,4,stamp)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;if(!ok)goto done;ok=0;
    if(sqlite3_prepare_v2(db,"UPDATE swimlanes SET version=version+1 WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)goto done;
    ok=sqlite3_bind_text(s,1,lane,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int64(s,3,(sqlite3_int64)expected)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(!ok||!wena_sqlite_swimlane_state_read(db,board,lane,expected+1,&stored,&after)||stored!=desired||after!=stamp||
        !cards_read(db,board,lane,verify,&final_count)||final_count!=count){ok=0;goto done;}
    for(i=0;i<count;++i)if(strcmp(cards[i].id,verify[i].id)||strcmp(cards[i].list,verify[i].list)||cards[i].version!=verify[i].version||
        cards[i].archived!=verify[i].archived||cards[i].at!=verify[i].at){ok=0;break;}
    if(ok)*result_version=expected+1;
done:
    free(cards);free(verify);return ok;
}
