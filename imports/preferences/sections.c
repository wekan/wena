#include "sections.h"
#include <stdlib.h>
#include <string.h>

static const char *text(sqlite3_stmt *statement,int column,size_t capacity)
{
    const char *value;int length;
    if (sqlite3_column_type(statement,column)!=SQLITE_TEXT) return NULL;
    value=(const char*)sqlite3_column_text(statement,column);length=sqlite3_column_bytes(statement,column);
    if (!value || length<1 || (size_t)length>=capacity || memchr(value,0,(size_t)length)) return NULL;
    return value;
}
static int flag_version(sqlite3_stmt *statement,int first,int *flag,unsigned long *version)
{
    sqlite3_int64 number;
    if (sqlite3_column_type(statement,first)!=SQLITE_INTEGER ||
        (sqlite3_column_int64(statement,first)!=0 && sqlite3_column_int64(statement,first)!=1) ||
        sqlite3_column_type(statement,first+1)!=SQLITE_INTEGER) return 0;
    number=sqlite3_column_int64(statement,first+1);
    if (number<1 || number>(sqlite3_int64)WENA_VERSION_READ_MAX) return 0;
    *flag=sqlite3_column_int(statement,first);*version=(unsigned long)number;return 1;
}
static int bind_scope(sqlite3_stmt *statement,const char *actor,const char *board)
{
    return sqlite3_bind_text(statement,1,actor,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK;
}
int wena_card_sections_load(sqlite3 *database,const char *actor,const char *board,
    WenaCardSectionsSnapshot **output)
{
    WenaCardSectionsSnapshot *candidate;
    WenaCardSectionPreference *entries,*entry;
    sqlite3_stmt *statement;const char *card,*key;
    size_t per_card,capacity;int step;
    if (!database || !output || !wena_model_identifier_valid(actor) ||
        !wena_model_identifier_valid(board) || !sqlite3_get_autocommit(database)) return 0;
    candidate=(WenaCardSectionsSnapshot*)calloc(1,sizeof(*candidate));if (!candidate) return 0;
    statement=NULL;per_card=0;
    if (sqlite3_exec(database,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK) goto done;
    if (sqlite3_prepare_v2(database,"SELECT 1 FROM actors a,boards b WHERE a.id=?1 AND b.id=?2",-1,&statement,NULL)!=SQLITE_OK ||
        !bind_scope(statement,actor,board) || sqlite3_step(statement)!=SQLITE_ROW || sqlite3_step(statement)!=SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement);statement=NULL;
    if (sqlite3_prepare_v2(database,
        "SELECT p.card_id,p.section_key,p.collapsed,p.version FROM actor_card_sections p "
        "WHERE p.actor_id=?1 AND p.card_id IN (SELECT id FROM cards WHERE board_id=?2) "
        "ORDER BY p.card_id,p.section_key",-1,&statement,NULL)!=SQLITE_OK || !bind_scope(statement,actor,board)) goto rollback;
    while ((step=sqlite3_step(statement))==SQLITE_ROW) {
        card=text(statement,0,WENA_ID_CAPACITY);key=text(statement,1,WENA_SECTION_KEY_CAPACITY);
        if (!wena_model_identifier_valid(card) || !wena_card_section_key_valid(key)) goto rollback;
        if (candidate->count && wena_card_section_compare(card,key,&candidate->entries[candidate->count-1])<=0) goto rollback;
        if (!candidate->count || strcmp(card,candidate->entries[candidate->count-1].card_id)) per_card=0;
        if (++per_card>WENA_SECTIONS_PER_CARD || candidate->count>=
            (size_t)WENA_SQLITE_BOARD_MAX_CARDS*WENA_SECTIONS_PER_CARD) goto rollback;
        if (candidate->count==candidate->capacity) {
            capacity=candidate->capacity?candidate->capacity*2:16;
            entries=(WenaCardSectionPreference*)realloc(candidate->entries,capacity*sizeof(*entries));
            if (!entries) goto rollback;
            candidate->entries=entries;candidate->capacity=capacity;
        }
        entry=&candidate->entries[candidate->count];
        if (!flag_version(statement,2,&entry->collapsed,&entry->version)) goto rollback;
        strcpy(entry->card_id,card);strcpy(entry->key,key);++candidate->count;
    }
    if (step!=SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement);statement=NULL;
    if (sqlite3_exec(database,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK) goto rollback;
    strcpy(candidate->actor_id,actor);strcpy(candidate->board_id,board);
    wena_card_sections_free(*output);*output=candidate;return 1;
rollback:
    if (statement) {sqlite3_finalize(statement);statement=NULL;}
    sqlite3_exec(database,"ROLLBACK",NULL,NULL,NULL);
done:
    if (statement) sqlite3_finalize(statement);
    wena_card_sections_free(candidate);return 0;
}
static int bind_preference(sqlite3_stmt *statement,const char *actor,const char *card,const char *key)
{
    return sqlite3_bind_text(statement,1,actor,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,2,card,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
        sqlite3_bind_text(statement,3,key,-1,SQLITE_TRANSIENT)==SQLITE_OK;
}
int wena_card_section_save(sqlite3 *database,const char *actor,const char *board,
    const char *card,const char *key,unsigned long expected,int collapsed)
{
    sqlite3_stmt *statement;unsigned long version;int stored,step,valid;
    if (!database || !wena_model_identifier_valid(actor) || !wena_model_identifier_valid(board) ||
        !wena_model_identifier_valid(card) || !wena_card_section_key_valid(key) ||
        expected>WENA_VERSION_MUTATE_MAX || (collapsed!=0 && collapsed!=1) || !sqlite3_get_autocommit(database)) return 0;
    if (sqlite3_exec(database,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK) return 0;
    statement=NULL;valid=0;stored=0;version=0;
    if (sqlite3_prepare_v2(database,
        "SELECT 1 FROM cards c WHERE c.id=?3 AND c.board_id=?2 AND c.archived=0 "
        "AND EXISTS(SELECT 1 FROM actors WHERE id=?1)",-1,&statement,NULL)!=SQLITE_OK ||
        !bind_scope(statement,actor,board) || sqlite3_bind_text(statement,3,card,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
        sqlite3_step(statement)!=SQLITE_ROW || sqlite3_step(statement)!=SQLITE_DONE) goto done;
    sqlite3_finalize(statement);statement=NULL;
    if (sqlite3_prepare_v2(database,"SELECT collapsed,version FROM actor_card_sections WHERE actor_id=?1 AND card_id=?2 AND section_key=?3",-1,&statement,NULL)!=SQLITE_OK ||
        !bind_preference(statement,actor,card,key)) goto done;
    step=sqlite3_step(statement);
    if (step==SQLITE_ROW) {
        if (!flag_version(statement,0,&stored,&version) || sqlite3_step(statement)!=SQLITE_DONE) goto done;
    } else if (step!=SQLITE_DONE) goto done;
    sqlite3_finalize(statement);statement=NULL;
    if (version!=expected) goto done;
    if (stored==collapsed) {valid=1;goto done;}
    if (!version) {
        if (sqlite3_prepare_v2(database,"SELECT count(*) FROM actor_card_sections WHERE actor_id=?1 AND card_id=?2",-1,&statement,NULL)!=SQLITE_OK ||
            sqlite3_bind_text(statement,1,actor,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
            sqlite3_bind_text(statement,2,card,-1,SQLITE_TRANSIENT)!=SQLITE_OK || sqlite3_step(statement)!=SQLITE_ROW ||
            sqlite3_column_int64(statement,0)>=WENA_SECTIONS_PER_CARD) goto done;
        sqlite3_finalize(statement);statement=NULL;
    }
    if (sqlite3_prepare_v2(database,
        "INSERT INTO actor_card_sections(actor_id,card_id,section_key,collapsed,version) VALUES(?1,?2,?3,?4,1) "
        "ON CONFLICT(actor_id,card_id,section_key) DO UPDATE SET collapsed=excluded.collapsed,version=version+1 WHERE version=?5",
        -1,&statement,NULL)!=SQLITE_OK || !bind_preference(statement,actor,card,key) ||
        sqlite3_bind_int(statement,4,collapsed)!=SQLITE_OK || sqlite3_bind_int64(statement,5,(sqlite3_int64)expected)!=SQLITE_OK ||
        sqlite3_step(statement)!=SQLITE_DONE || sqlite3_changes(database)!=1) goto done;
    sqlite3_finalize(statement);statement=NULL;
    if (sqlite3_prepare_v2(database,"SELECT collapsed,version FROM actor_card_sections WHERE actor_id=?1 AND card_id=?2 AND section_key=?3",-1,&statement,NULL)!=SQLITE_OK ||
        !bind_preference(statement,actor,card,key) || sqlite3_step(statement)!=SQLITE_ROW ||
        !flag_version(statement,0,&stored,&version) || stored!=collapsed || version!=expected+1 ||
        sqlite3_step(statement)!=SQLITE_DONE) goto done;
    valid=1;
done:
    if (statement && sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (valid && sqlite3_exec(database,"COMMIT",NULL,NULL,NULL)==SQLITE_OK) return 1;
    sqlite3_exec(database,"ROLLBACK",NULL,NULL,NULL);return 0;
}
