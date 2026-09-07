#include "card_description_mutation.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int scoped(WenaCardDescriptionMutation *a,const char *board,const char *card)
{
    return a&&a->persistence.database&&wena_model_identifier_valid(board)&&
        wena_model_identifier_valid(card)&&!strcmp(a->board_id,board);
}

int wena_card_description_mutation_init(WenaCardDescriptionMutation *a,
    sqlite3 *database,const char *actor,const char *board)
{
    if(!a)return 0;
    memset(a,0,sizeof(*a));
    if(!database||!wena_model_identifier_valid(actor)||!wena_model_identifier_valid(board))return 0;
    wena_sqlite_persistence_init(&a->persistence,database);
    strcpy(a->actor_id,actor);strcpy(a->board_id,board);sprintf(a->route,"/b/%s/native",board);
    return 1;
}

int wena_card_description_mutation_load(void *context,const char *board,
    const char *card,char *description,size_t capacity,unsigned long *version)
{
    WenaCardDescriptionMutation *a;
    sqlite3_stmt *statement;
    sqlite3_int64 stored_version;
    const char *text;
    size_t length;
    int ok;
    a=(WenaCardDescriptionMutation*)context;
    if(!scoped(a,board,card)||!description||!capacity||!version)return 0;
    if(sqlite3_prepare_v2(a->persistence.database,
        "SELECT d.card_id,d.description,c.version FROM cards c LEFT JOIN card_descriptions d "
        "ON d.card_id=c.id AND d.board_id=c.board_id WHERE c.id=?1 AND c.board_id=?2 "
        "AND c.archived=0 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)",
        -1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(statement,1,card,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,3,a->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    if(ok)ok=sqlite3_step(statement)==SQLITE_ROW&&sqlite3_column_type(statement,2)==SQLITE_INTEGER;
    if(ok){
        stored_version=sqlite3_column_int64(statement,2);
        text="";length=0;
        if(sqlite3_column_type(statement,0)!=SQLITE_NULL){
            ok=sqlite3_column_type(statement,0)==SQLITE_TEXT&&sqlite3_column_type(statement,1)==SQLITE_TEXT;
            if(ok){text=(const char*)sqlite3_column_text(statement,1);length=(size_t)sqlite3_column_bytes(statement,1);}
        }
        ok=ok&&stored_version>0&&stored_version<LONG_MAX&&length<capacity&&wena_model_description_valid(text,length);
        if(ok){memcpy(description,text,length);description[length]=0;*version=(unsigned long)stored_version;}
    }
    sqlite3_finalize(statement);return ok;
}

int wena_card_description_mutation_save_request(WenaCardDescriptionMutation *a,
    const char *board,const char *card,unsigned long expected,unsigned long request,
    const char *description)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    char encoded[3*(WENA_DESCRIPTION_CAPACITY-1)+1];
    const char hex[]="0123456789ABCDEF";
    size_t length,index;
    unsigned char c;
    if(!scoped(a,board,card)||!description||!expected||expected>=(unsigned long)LONG_MAX||
        !request||request>=(unsigned long)LONG_MAX)return 0;
    for(length=0;length<WENA_DESCRIPTION_CAPACITY&&description[length];++length){}
    if(!wena_model_description_valid(description,length))return 0;
    for(index=0;index<length;++index){c=(unsigned char)description[index];encoded[index*3]='%';encoded[index*3+1]=hex[c>>4];encoded[index*3+2]=hex[c&15];}
    encoded[length*3]=0;
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_EDIT_CARD_DESCRIPTION;
    command.request_version=request;strcpy(command.user_id,a->actor_id);strcpy(command.route,a->route);
    sprintf(command.form_body,"cardId=%s&expectedVersion=%lu&description=%s",card,expected,encoded);
    command.form_body_length=strlen(command.form_body);
    return wena_sqlite_persistence_apply(&a->persistence,&command,&response);
}

int wena_card_description_mutation_save(void *context,const char *board,
    const char *card,unsigned long expected,const char *description)
{
    WenaCardDescriptionMutation *a;
    sqlite3_stmt *statement;
    sqlite3_int64 request;
    int ok;
    a=(WenaCardDescriptionMutation*)context;
    if(!scoped(a,board,card))return 0;
    if(sqlite3_prepare_v2(a->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE actor_id=?1 AND route=?2 AND operation='edit-card-description'",
        -1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(statement,1,a->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,2,a->route,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    request=-1;if(ok&&sqlite3_step(statement)==SQLITE_ROW)request=sqlite3_column_int64(statement,0);
    sqlite3_finalize(statement);if(request<0||request>=LONG_MAX-1)return 0;
    return wena_card_description_mutation_save_request(a,board,card,expected,(unsigned long)request+1,description);
}
