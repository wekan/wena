#include "swimlane_archive.h"
#include "card_archive.h"
#include "list_wip.h"
#include "../list_state.h"
#include "../sqlite_storage.h"
#include "../../models/card_order.h"
#include <stdlib.h>
#include <string.h>
typedef struct ArchiveCard {
    WenaId id,list,lane;
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
static int card_row(sqlite3 *db,const char *board,sqlite3_stmt *s,ArchiveCard *card)
{
    WenaId scope;sqlite3_int64 version,at;int archived;
    if(!id_column(s,0,card->id)||!id_column(s,1,scope)||strcmp(scope,board)||
        !id_column(s,2,card->list)||!id_column(s,4,card->lane)||sqlite3_column_type(s,3)!=SQLITE_INTEGER)return 0;
    version=sqlite3_column_int64(s,3);
    if(version<1||version>(sqlite3_int64)WENA_VERSION_READ_MAX||
        !wena_sqlite_list_state_read(db,board,card->list,0,&archived,&at)||
        !wena_sqlite_swimlane_state_read(db,board,card->lane,0,&archived,&at)||
        !wena_sqlite_card_archive_read(db,board,card->id,(unsigned long)version,&card->archived,&card->at))return 0;
    card->version=(unsigned long)version;return 1;
}
static int cards_read(sqlite3 *db,const char *board,const char *lane,const char *list,ArchiveCard *cards,size_t *count)
{
    sqlite3_stmt *s;const char *query;size_t used;int ok,rc;
    query=list?"SELECT id,board_id,list_id,version,swimlane_id FROM cards WHERE list_id=?1 AND (?2='' OR swimlane_id=?2) ORDER BY id":
        "SELECT id,board_id,list_id,version,swimlane_id FROM cards WHERE swimlane_id=?1 ORDER BY id";
    if(sqlite3_prepare_v2(db,query,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,list?list:lane,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    if(ok&&list)ok=sqlite3_bind_text(s,2,lane?lane:"",-1,SQLITE_TRANSIENT)==SQLITE_OK;used=0;rc=SQLITE_ERROR;
    while(ok&&(rc=sqlite3_step(s))==SQLITE_ROW){
        if(used==WENA_CARD_ORDER_CAPACITY||!card_row(db,board,s,&cards[used])){ok=0;break;}
        ++used;
    }
    if(rc!=SQLITE_DONE)ok=0;if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)*count=used;return ok;
}
static int cards_equal(const ArchiveCard *a,const ArchiveCard *b,size_t count)
{
    size_t i;
    for(i=0;i<count;++i)if(strcmp(a[i].id,b[i].id)||strcmp(a[i].list,b[i].list)||
        strcmp(a[i].lane,b[i].lane)||a[i].version!=b[i].version||
        a[i].archived!=b[i].archived||a[i].at!=b[i].at)return 0;
    return 1;
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
    if(!cards_read(db,board,lane,NULL,cards,&count))goto done;
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
        !cards_read(db,board,lane,NULL,verify,&final_count)||final_count!=count){ok=0;goto done;}
    ok=cards_equal(cards,verify,count);
    if(ok)*result_version=expected+1;
done:
    free(cards);free(verify);return ok;
}


/* The list menu selects the whole list or its current swimlane. Reuse the
 * cascade reader and individual archive writer; list/lane state stays intact. */
static int active_revision(sqlite3 *db,const char *board,const char *id,unsigned long version,int lane)
{
    int archived;sqlite3_int64 at;
    return (lane?wena_sqlite_swimlane_state_read(db,board,id,version,&archived,&at):
        wena_sqlite_list_state_read(db,board,id,version,&archived,&at))&&!archived;
}
int wena_sqlite_list_cards_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    char list[WENA_ID_CAPACITY],lane[WENA_ID_CAPACITY],text[32];
    unsigned long expected,lane_version;ArchiveCard *cards,*verify;
    size_t count,final_count,i;int ok,changed;sqlite3_int64 at;int archived;
    if(!db||!command||!result_version||sqlite3_get_autocommit(db)||
        command->operation!=WENA_DOMAIN_ARCHIVE_LIST_CARDS||!wena_model_identifier_valid(board)||
        !wena_mutation_text(command,"listId",list,sizeof(list),0)||!wena_model_identifier_valid(list)||
        !wena_mutation_text(command,"expectedVersion",text,sizeof(text),0)||
        !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected)||
        !active_revision(db,board,list,expected,0))return 0;
    lane[0]=0;lane_version=0;
    if(wena_mutation_has_value(command,"swimlaneId")){
        if(!wena_mutation_text(command,"swimlaneId",lane,sizeof(lane),0)||!wena_model_identifier_valid(lane)||
            !wena_mutation_text(command,"expectedSwimlaneVersion",text,sizeof(text),0)||
            !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&lane_version)||
            !active_revision(db,board,lane,lane_version,1))return 0;
    }else if(wena_mutation_has_value(command,"expectedSwimlaneVersion"))return 0;
    cards=(ArchiveCard*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*cards));
    verify=(ArchiveCard*)calloc(WENA_CARD_ORDER_CAPACITY,sizeof(*verify));
    if(!cards||!verify){free(cards);free(verify);return 0;}
    ok=0;changed=0;count=final_count=0;
    if(!cards_read(db,board,lane,list,cards,&count))goto done;
    for(i=0;i<count;++i){
        if(cards[i].archived)continue;
        if(!wena_sqlite_card_archive_change(db,board,cards[i].id,cards[i].version,1,0))goto done;
        ++cards[i].version;cards[i].archived=1;changed=1;
        if(!wena_sqlite_card_archive_read(db,board,cards[i].id,cards[i].version,&archived,&at))goto done;
        cards[i].at=at;
    }
    if(!active_revision(db,board,list,expected,0)||
        (lane[0]&&!active_revision(db,board,lane,lane_version,1))||
        !cards_read(db,board,lane,list,verify,&final_count)||count!=final_count)goto done;
    if(!cards_equal(cards,verify,count))goto done;
    *result_version=expected;ok=changed?1:2;
done:
    free(cards);free(verify);return ok;
}


static int selected_card_read(sqlite3 *db,const char *board,const char *id,ArchiveCard *card)
{
    sqlite3_stmt *s;int ok;
    if(sqlite3_prepare_v2(db,"SELECT id,board_id,list_id,version,swimlane_id FROM cards WHERE id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        card_row(db,board,s,card)&&sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;return ok;
}
int wena_sqlite_card_archive_version(sqlite3 *db,const char *board,const char *id,unsigned long *version)
{
    ArchiveCard card;
    if(!db||!version||sqlite3_get_autocommit(db)||!wena_model_identifier_valid(board)||
        !wena_model_identifier_valid(id)||!selected_card_read(db,board,id,&card)||
        card.archived||card.version>WENA_VERSION_MUTATE_MAX)return 0;
    *version=card.version;return 1;
}
int wena_sqlite_selected_cards_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    ArchiveCard *cards,*verify;size_t count,i,j;int ok,archived;unsigned long version;
    if(!db||!command||!result_version||sqlite3_get_autocommit(db)||!wena_model_identifier_valid(board)||
        command->operation!=WENA_DOMAIN_ARCHIVE_SELECTED_CARDS||!command->selected_cards||
        !command->selected_card_count||command->selected_card_count>WENA_DOMAIN_CARD_BATCH_CAPACITY)return 0;
    count=command->selected_card_count;
    for(i=0;i<count;++i){
        if(!wena_model_identifier_valid(command->selected_cards[i].id)||!command->selected_cards[i].version||
            command->selected_cards[i].version>WENA_VERSION_MUTATE_MAX)return 0;
        for(j=0;j<i;++j)if(!strcmp(command->selected_cards[i].id,command->selected_cards[j].id))return 0;
    }
    cards=(ArchiveCard*)calloc(count,sizeof(*cards));verify=(ArchiveCard*)calloc(count,sizeof(*verify));
    if(!cards||!verify){free(cards);free(verify);return 0;}
    ok=0;version=0;
    /* Validate the complete exact selection before changing its first card. */
    for(i=0;i<count;++i)if(!selected_card_read(db,board,command->selected_cards[i].id,&cards[i])||
        cards[i].archived||cards[i].version!=command->selected_cards[i].version)goto done;
    for(i=0;i<count;++i){
        if(!wena_sqlite_card_archive_change(db,board,cards[i].id,cards[i].version,1,0))goto done;
        ++cards[i].version;cards[i].archived=1;
        if(cards[i].version>version)version=cards[i].version;
        if(!wena_sqlite_card_archive_read(db,board,cards[i].id,cards[i].version,&archived,&cards[i].at))goto done;
    }
    for(i=0;i<count;++i)if(!selected_card_read(db,board,cards[i].id,&verify[i]))goto done;
    if(!cards_equal(cards,verify,count))goto done;
    *result_version=version;ok=1;
done:
    free(cards);free(verify);return ok;
}
