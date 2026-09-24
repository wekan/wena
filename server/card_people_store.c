#include "card_people_store.h"
#include <stdlib.h>
#include <string.h>
static const char *text(sqlite3_stmt *q,int column,size_t capacity)
{
    const char *value;int bytes;
    if(sqlite3_column_type(q,column)!=SQLITE_TEXT)return NULL;
    value=(const char*)sqlite3_column_text(q,column);bytes=sqlite3_column_bytes(q,column);
    if(!value||bytes<0||(size_t)bytes>=capacity||memchr(value,0,(size_t)bytes))return NULL;
    return value;
}
static int version(sqlite3_stmt *q,int column,unsigned long *output)
{
    sqlite3_int64 value;
    if(sqlite3_column_type(q,column)!=SQLITE_INTEGER)return 0;
    value=sqlite3_column_int64(q,column);
    if(value<1||value>(sqlite3_int64)WENA_VERSION_READ_MAX)return 0;
    *output=(unsigned long)value;return 1;
}
static int board_version(sqlite3 *db,const char *board,unsigned long *output)
{
    sqlite3_stmt *q;int ok;
    if(sqlite3_prepare_v2(db,"SELECT version FROM boards WHERE id=?1",-1,&q,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(q,1,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(q)==SQLITE_ROW&&
        version(q,0,output)&&sqlite3_step(q)==SQLITE_DONE;
    if(sqlite3_finalize(q)!=SQLITE_OK)ok=0;
    return ok;
}
int wena_sqlite_member_roster_read(sqlite3 *db,const char *board,WenaMemberRoster *output)
{
    WenaMemberRoster *candidate;sqlite3_stmt *q;const char *actor,*name,*scope;
    size_t n;int ok,step,active;sqlite3_int64 created,updated;
    if(!db||sqlite3_get_autocommit(db)||!output||!wena_model_identifier_valid(board))return 0;
    candidate=(WenaMemberRoster*)calloc(1,sizeof(*candidate));if(!candidate)return 0;
    strcpy(candidate->board_id,board);q=NULL;
    ok=board_version(db,board,&candidate->board_version)&&sqlite3_prepare_v2(db,
        "SELECT m.board_id,m.actor_id,m.active,m.version,m.created_at,m.updated_at,a.display_name,a.version "
        "FROM board_members m LEFT JOIN actors a ON a.id=m.actor_id WHERE m.board_id=?1 ORDER BY m.actor_id COLLATE BINARY LIMIT 2049",
        -1,&q,NULL)==SQLITE_OK;
    if(ok)ok=sqlite3_bind_text(q,1,board,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    step=SQLITE_DONE;n=0;
    while(ok&&(step=sqlite3_step(q))==SQLITE_ROW){
        if(n==WENA_BOARD_MEMBER_CAPACITY){ok=0;break;}
        scope=text(q,0,WENA_ID_CAPACITY);actor=text(q,1,WENA_ID_CAPACITY);name=text(q,6,WENA_TITLE_CAPACITY);
        if(!scope||strcmp(scope,board)||!wena_model_identifier_valid(actor)||
            !wena_model_title_string_valid(name,WENA_TITLE_CAPACITY)||
            (n&&strcmp(candidate->members[n-1].actor_id,actor)>=0)||
            sqlite3_column_type(q,2)!=SQLITE_INTEGER||sqlite3_column_type(q,4)!=SQLITE_INTEGER||
            sqlite3_column_type(q,5)!=SQLITE_INTEGER||!version(q,3,&candidate->versions[n])||
            !version(q,7,&candidate->actor_versions[n])){ok=0;break;}
        active=sqlite3_column_int(q,2);created=sqlite3_column_int64(q,4);updated=sqlite3_column_int64(q,5);
        if((sqlite3_column_int64(q,2)!=0&&sqlite3_column_int64(q,2)!=1)||created<0||updated<created){ok=0;break;}
        strcpy(candidate->members[n].board_id,board);strcpy(candidate->members[n].actor_id,actor);
        candidate->members[n].active=active;strcpy(candidate->names[n],name);
        candidate->created[n]=created;candidate->updated[n]=updated;++n;
    }
    if(step!=SQLITE_DONE)ok=0;
    if(q&&sqlite3_finalize(q)!=SQLITE_OK)ok=0;
    if(ok){candidate->count=n;ok=wena_board_members_valid(candidate->members,n,board);}
    if(ok)*output=*candidate;
    free(candidate);return ok;
}
int wena_sqlite_card_people_read(sqlite3 *db,const char *board,const char *card,WenaCardPeopleSnapshot *output)
{
    WenaCardPeopleSnapshot *candidate;sqlite3_stmt *q;const char *scope,*actor,*field;
    size_t n;int ok,step,f;sqlite3_int64 position;unsigned long actor_version;
    if(!db||sqlite3_get_autocommit(db)||!output||!wena_model_identifier_valid(board)||!wena_model_identifier_valid(card))return 0;
    candidate=(WenaCardPeopleSnapshot*)calloc(1,sizeof(*candidate));if(!candidate)return 0;
    strcpy(candidate->card_id,card);q=NULL;
    for(f=0;f<WENA_PERSON_FIELD_COUNT;++f)(void)wena_card_people_init(&candidate->fields[f],board);
    ok=board_version(db,board,&candidate->board_version)&&
        sqlite3_prepare_v2(db,"SELECT board_id,version,archived FROM cards WHERE id=?1",-1,&q,NULL)==SQLITE_OK;
    if(ok)ok=sqlite3_bind_text(q,1,card,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(q)==SQLITE_ROW;
    if(ok){
        scope=text(q,0,WENA_ID_CAPACITY);
        ok=scope&&!strcmp(scope,board)&&version(q,1,&candidate->card_version)&&sqlite3_column_type(q,2)==SQLITE_INTEGER&&
            (sqlite3_column_int64(q,2)==0||sqlite3_column_int64(q,2)==1);
        if(ok){candidate->archived=sqlite3_column_int(q,2);ok=sqlite3_step(q)==SQLITE_DONE;}
    }
    if(q&&sqlite3_finalize(q)!=SQLITE_OK)ok=0;q=NULL;
    if(ok)ok=sqlite3_prepare_v2(db,
        "SELECT p.board_id,p.field,p.actor_id,p.position,a.version FROM card_people p "
        "LEFT JOIN actors a ON a.id=p.actor_id WHERE p.card_id=?1 ORDER BY p.field COLLATE BINARY,p.position,p.actor_id COLLATE BINARY LIMIT 4097",
        -1,&q,NULL)==SQLITE_OK;
    if(ok)ok=sqlite3_bind_text(q,1,card,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    step=SQLITE_DONE;
    while(ok&&(step=sqlite3_step(q))==SQLITE_ROW){
        scope=text(q,0,WENA_ID_CAPACITY);field=text(q,1,16);actor=text(q,2,WENA_ID_CAPACITY);
        if(!scope||strcmp(scope,board)||!field||!wena_model_identifier_valid(actor)||
            sqlite3_column_type(q,3)!=SQLITE_INTEGER||!version(q,4,&actor_version)){ok=0;break;}
        if(!strcmp(field,"members"))f=WENA_PERSON_MEMBERS;
        else if(!strcmp(field,"assignees"))f=WENA_PERSON_ASSIGNEES;
        else{ok=0;break;}
        n=candidate->fields[f].count;position=sqlite3_column_int64(q,3);
        if(n==WENA_CARD_PEOPLE_CAPACITY||position<0||position>2147483647||
            (n&&candidate->positions[f][n-1]>=(unsigned long)position)){ok=0;break;}
        strcpy(candidate->fields[f].ids[n],actor);candidate->positions[f][n]=(unsigned long)position;
        ++candidate->fields[f].count;
    }
    if(step!=SQLITE_DONE)ok=0;
    if(q&&sqlite3_finalize(q)!=SQLITE_OK)ok=0;
    for(f=0;ok&&f<WENA_PERSON_FIELD_COUNT;++f)ok=wena_card_people_valid(&candidate->fields[f]);
    if(ok)*output=*candidate;
    free(candidate);return ok;
}
