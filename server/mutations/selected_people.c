#include "card_people.h"
#include "../list_state.h"
#include "../sha256.h"
#include <stdlib.h>
#include <string.h>
typedef struct PersonGuard {char before[65],after[65];int changed;} PersonGuard;
static void hash_number(WenaSha256 *hash,sqlite3_int64 value)
{
    unsigned char bytes[8];int i;
    for(i=7;i>=0;--i){bytes[i]=(unsigned char)(value&255);value>>=8;}
    wena_sha256_update(hash,bytes,sizeof(bytes));
}
static void hash_text(WenaSha256 *hash,const char *value)
{size_t length;length=strlen(value);hash_number(hash,(sqlite3_int64)length);wena_sha256_update(hash,(const unsigned char*)value,length);}
/* Canonical framing, independent of structure padding and platform long size.
 * Only a pair of hashes per selected card is retained, not 2048 full snapshots. */
static void fingerprint(const WenaCardPeopleSnapshot *card,char output[65])
{
    WenaSha256 hash;size_t i;int f;wena_sha256_init(&hash);
    hash_text(&hash,card->card_id);hash_text(&hash,card->list_id);hash_text(&hash,card->swimlane_id);hash_text(&hash,card->title);
    hash_number(&hash,card->position);hash_number(&hash,(sqlite3_int64)card->board_version);
    hash_number(&hash,(sqlite3_int64)card->card_version);hash_number(&hash,card->archived);
    for(f=0;f<WENA_PERSON_FIELD_COUNT;++f){
        hash_text(&hash,card->fields[f].board_id);hash_number(&hash,(sqlite3_int64)card->fields[f].count);
        for(i=0;i<card->fields[f].count;++i){hash_text(&hash,card->fields[f].ids[i]);hash_number(&hash,(sqlite3_int64)card->positions[f][i]);}
    }
    wena_sha256_final_hex(&hash,output);
}
static int active(sqlite3 *db,const char *board,const WenaCardPeopleSnapshot *card)
{return !card->archived&&wena_sqlite_list_active(db,board,card->list_id)&&wena_sqlite_swimlane_active(db,board,card->swimlane_id);}
static int board_advance(sqlite3 *db,const char *board,unsigned long version)
{
    sqlite3_stmt *q;int ok;
    if(sqlite3_prepare_v2(db,"UPDATE boards SET version=version+1 WHERE id=?1 AND version=?2",-1,&q,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(q,1,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_int64(q,2,(sqlite3_int64)version)==SQLITE_OK&&
        sqlite3_step(q)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(q)!=SQLITE_OK)ok=0;return ok;
}
int wena_sqlite_selected_people_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    WenaMemberRoster *roster,*after;WenaCardPeopleSnapshot *card,*planned;PersonGuard *guards;
    char person[65],field_text[16],enabled_text[4],version_text[32],hash[65];
    unsigned long version;size_t i,j;int field,enabled,ok,any,change,result;
    if(!db||sqlite3_get_autocommit(db)||!command||!result_version||command->operation!=WENA_DOMAIN_SET_SELECTED_PERSON||
        !wena_model_identifier_valid(board)||!command->selected_cards||!command->selected_card_count||
        command->selected_card_count>WENA_DOMAIN_CARD_BATCH_CAPACITY)return 0;
    if(!wena_mutation_text(command,"personId",person,sizeof(person),0)||!wena_model_identifier_valid(person)||
        !wena_mutation_text(command,"field",field_text,sizeof(field_text),0)||
        !wena_mutation_text(command,"enabled",enabled_text,sizeof(enabled_text),0)||
        !wena_mutation_text(command,"expectedBoardVersion",version_text,sizeof(version_text),0)||
        !wena_mutation_decimal(version_text,0,WENA_VERSION_MUTATE_MAX,&version))return 0;
    if(!strcmp(field_text,"members"))field=WENA_PERSON_MEMBERS;
    else if(!strcmp(field_text,"assignees"))field=WENA_PERSON_ASSIGNEES;else return 0;
    if(!strcmp(enabled_text,"1"))enabled=1;else if(!strcmp(enabled_text,"0"))enabled=0;else return 0;
    for(i=0;i<command->selected_card_count;++i){
        if(!wena_model_identifier_valid(command->selected_cards[i].id)||!command->selected_cards[i].version||
            command->selected_cards[i].version>WENA_VERSION_MUTATE_MAX)return 0;
        for(j=0;j<i;++j)if(!strcmp(command->selected_cards[i].id,command->selected_cards[j].id))return 0;
    }
    roster=(WenaMemberRoster*)malloc(sizeof(*roster));after=(WenaMemberRoster*)malloc(sizeof(*after));
    card=(WenaCardPeopleSnapshot*)malloc(sizeof(*card));planned=(WenaCardPeopleSnapshot*)malloc(sizeof(*planned));
    guards=(PersonGuard*)calloc(command->selected_card_count,sizeof(*guards));
    if(!roster||!after||!card||!planned||!guards){free(roster);free(after);free(card);free(planned);free(guards);return 0;}
    ok=wena_sqlite_member_roster_read(db,board,roster)&&roster->board_version==version;any=0;result=0;
    /* Preflight every selected card before any write, keeping bounded hashes. */
    for(i=0;ok&&i<command->selected_card_count;++i){
        ok=wena_sqlite_card_people_read(db,board,command->selected_cards[i].id,card)&&
            card->card_version==command->selected_cards[i].version&&active(db,board,card);
        if(ok){
            change=wena_card_person_plan(roster,card,field,person,enabled,planned);ok=change!=0;
            if(ok){guards[i].changed=change==1;if(guards[i].changed)any=1;
                fingerprint(card,guards[i].before);planned->board_version=version+1;fingerprint(planned,guards[i].after);}
        }
    }
    if(ok&&!any){*result_version=version;result=2;}
    else if(ok){
        for(i=0;ok&&i<command->selected_card_count;++i)if(guards[i].changed){
            ok=wena_sqlite_card_people_read(db,board,command->selected_cards[i].id,card);
            if(ok){fingerprint(card,hash);ok=!strcmp(hash,guards[i].before)&&
                wena_sqlite_card_person_set(db,roster,card,field,person,enabled,planned)==1;}
        }
        if(ok)ok=board_advance(db,board,version);
        if(ok){++roster->board_version;ok=wena_sqlite_member_roster_read(db,board,after)&&!memcmp(roster,after,sizeof(*roster));}
        /* Later writes and board triggers must not alter an earlier card or a
         * no-op member of a mixed selection. Verify the entire final batch. */
        for(i=0;ok&&i<command->selected_card_count;++i){
            ok=wena_sqlite_card_people_read(db,board,command->selected_cards[i].id,card)&&active(db,board,card);
            if(ok){fingerprint(card,hash);ok=!strcmp(hash,guards[i].after);}
        }
        if(ok){*result_version=version+1;result=1;}
    }
    free(roster);free(after);free(card);free(planned);free(guards);return result;
}
