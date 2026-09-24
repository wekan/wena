#include "card_people.h"
#include "../list_state.h"
#include <stdlib.h>
#include <string.h>
static int parents(sqlite3 *db,const WenaCardPeopleSnapshot *card)
{
    return !card->archived&&wena_sqlite_list_active(db,card->fields[0].board_id,card->list_id)&&
        wena_sqlite_swimlane_active(db,card->fields[0].board_id,card->swimlane_id);
}
static int row_write(sqlite3 *db,const WenaCardPeopleSnapshot *card,int field,
    const char *actor,int enabled,unsigned long position)
{
    sqlite3_stmt *q;int ok;const char *name;name=field==WENA_PERSON_MEMBERS?"members":"assignees";
    if(sqlite3_prepare_v2(db,enabled?
        "INSERT INTO card_people(board_id,card_id,field,actor_id,position) VALUES(?1,?2,?3,?4,?5)":
        "DELETE FROM card_people WHERE board_id=?1 AND card_id=?2 AND field=?3 AND actor_id=?4",
        -1,&q,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(q,1,card->fields[0].board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(q,2,card->card_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(q,3,name,-1,SQLITE_STATIC)==SQLITE_OK&&
        sqlite3_bind_text(q,4,actor,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    if(ok&&enabled)ok=sqlite3_bind_int64(q,5,(sqlite3_int64)position)==SQLITE_OK;
    if(ok)ok=sqlite3_step(q)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(q)!=SQLITE_OK)ok=0;return ok;
}
static int advance(sqlite3 *db,const WenaCardPeopleSnapshot *card)
{
    sqlite3_stmt *q;int ok;
    if(sqlite3_prepare_v2(db,"UPDATE cards SET version=version+1 WHERE board_id=?1 AND id=?2 AND version=?3 AND archived=0",-1,&q,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(q,1,card->fields[0].board_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(q,2,card->card_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int64(q,3,(sqlite3_int64)card->card_version)==SQLITE_OK&&
        sqlite3_step(q)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(q)!=SQLITE_OK)ok=0;return ok;
}
int wena_card_person_plan(const WenaMemberRoster *roster,const WenaCardPeopleSnapshot *current,
    int field,const char *actor,int enabled,WenaCardPeopleSnapshot *output)
{
    WenaCardPeopleSnapshot *planned;size_t i,j;unsigned long position;int ok,changed,result;
    if(!roster||!current||!output||field<0||field>=WENA_PERSON_FIELD_COUNT||current->archived||
        !wena_card_people_valid(&current->fields[0])||!wena_card_people_valid(&current->fields[1])||
        strcmp(current->fields[0].board_id,current->fields[1].board_id)||
        !wena_model_identifier_valid(roster->board_id)||strcmp(roster->board_id,current->fields[0].board_id)||
        roster->board_version!=current->board_version||!current->board_version||current->board_version>WENA_VERSION_MUTATE_MAX||
        !current->card_version||current->card_version>WENA_VERSION_MUTATE_MAX)return 0;
    planned=(WenaCardPeopleSnapshot*)malloc(sizeof(*planned));if(!planned)return 0;*planned=*current;
    ok=wena_card_people_set(&current->fields[field],roster->members,roster->count,actor,enabled,&planned->fields[field],&changed);
    if(ok&&changed){
        if(enabled){
            i=current->fields[field].count;position=0;
            if(i){position=current->positions[field][i-1];if(position>=2147483647UL)ok=0;else ++position;}
            if(ok)planned->positions[field][i]=position;
        }else{
            for(i=0;i<current->fields[field].count;++i)if(!strcmp(current->fields[field].ids[i],actor))break;
            for(j=i+1;j<current->fields[field].count;++j)planned->positions[field][j-1]=current->positions[field][j];
            planned->positions[field][planned->fields[field].count]=0;
        }
        ++planned->card_version;
    }
    result=0;if(ok){*output=*planned;result=changed?1:2;}free(planned);return result;
}
int wena_sqlite_card_person_set(sqlite3 *db,const WenaMemberRoster *roster,
    const WenaCardPeopleSnapshot *expected,int field,const char *actor,int enabled,WenaCardPeopleSnapshot *output)
{
    WenaMemberRoster *current_roster;WenaCardPeopleSnapshot *current,*planned;
    unsigned long position;int ok,changed,result;
    if(!db||sqlite3_get_autocommit(db)||!roster||!expected||!output||field<0||field>=WENA_PERSON_FIELD_COUNT||
        !wena_model_identifier_valid(actor)||(enabled!=0&&enabled!=1)||
        !wena_model_identifier_valid(roster->board_id)||!wena_model_identifier_valid(expected->card_id)||
        !wena_model_identifier_valid(expected->fields[0].board_id)||strcmp(roster->board_id,expected->fields[0].board_id)||
        !expected->card_version||expected->card_version>WENA_VERSION_MUTATE_MAX||
        !expected->board_version||expected->board_version>WENA_VERSION_MUTATE_MAX)return 0;
    current_roster=(WenaMemberRoster*)malloc(sizeof(*current_roster));
    current=(WenaCardPeopleSnapshot*)malloc(sizeof(*current));planned=(WenaCardPeopleSnapshot*)malloc(sizeof(*planned));
    if(!current_roster||!current||!planned){free(current_roster);free(current);free(planned);return 0;}
    result=0;changed=0;
    ok=wena_sqlite_member_roster_read(db,roster->board_id,current_roster)&&!memcmp(roster,current_roster,sizeof(*roster))&&
        wena_sqlite_card_people_read(db,roster->board_id,expected->card_id,current)&&!memcmp(expected,current,sizeof(*expected))&&parents(db,current);
    if(ok){result=wena_card_person_plan(roster,current,field,actor,enabled,planned);ok=result!=0;changed=result==1;}
    if(ok&&changed){
        position=enabled?planned->positions[field][planned->fields[field].count-1]:0;
        ok=row_write(db,current,field,actor,enabled,position)&&advance(db,current);
        if(ok)ok=wena_sqlite_member_roster_read(db,roster->board_id,current_roster)&&!memcmp(roster,current_roster,sizeof(*roster))&&
            wena_sqlite_card_people_read(db,roster->board_id,expected->card_id,current)&&!memcmp(planned,current,sizeof(*current))&&parents(db,current);
    }
    result=0;
    if(ok){*output=*planned;result=changed?1:2;}
    free(current_roster);free(current);free(planned);return result;
}
