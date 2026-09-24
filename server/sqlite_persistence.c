#include "list_state.h"
#include "sqlite_persistence.h"
#include "sha256.h"
#include "mutations/labels.h"
#include "mutations/checklist_batch.h"
#include "mutations/board_settings.h"
#include "mutations/list_archive.h"
#include "mutations/checklist_order.h"
#include "../models/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int value_mode(const WenaDomainCommand *command, const char *name,
                 char *out, size_t capacity, int multiline)
{
    size_t position, end, equals, length, cursor;
    int found;
    position = 0;
    found = 0;
    if (command->form_body_length >= sizeof(command->form_body)) return 0;
    while (position < command->form_body_length) {
        end = position;
        while (end < command->form_body_length && command->form_body[end] != '&') ++end;
        equals = position;
        while (equals < end && command->form_body[equals] != '=') ++equals;
        if (equals == end) return 0;
        if (equals - position == strlen(name) &&
            !memcmp(command->form_body + position, name, equals - position)) {
            if (found) return 0;
            length = 0;
            for (cursor = equals + 1; cursor < end; ++cursor) {
                unsigned char c;
                int high, low;
                c = (unsigned char)command->form_body[cursor];
                if (c == '%') {
                    if (end - cursor < 3) return 0;
                    high = hex_digit((unsigned char)command->form_body[cursor + 1]);
                    low = hex_digit((unsigned char)command->form_body[cursor + 2]);
                    if (high < 0 || low < 0) return 0;
                    c = (unsigned char)(high * 16 + low);
                    cursor += 2;
                } else if (c == '+') c = ' ';
                if (((c < 32 || c == 127) &&
                     !(multiline == 1 && (c == 9 || c == 10 || c == 13))) ||
                    length + 1 >= capacity) return 0;
                out[length++] = (char)c;
            }
            if (!length && !multiline) return 0;
            out[length] = 0;
            /* Native models reject Unicode C1 controls as well as ASCII ones.
             * Apply the same gate to every decoded server mutation value so a
             * successful write cannot make its board unreadable by the loader. */
            for (cursor = 0; cursor + 1 < length; ++cursor) {
                if ((unsigned char)out[cursor] == 194 &&
                    (unsigned char)out[cursor + 1] >= 128 &&
                    (unsigned char)out[cursor + 1] <= 159) return 0;
            }
            found = 1;
        }
        position = end + 1;
    }
    return found;
}
static int value(const WenaDomainCommand *command, const char *name,
                 char *out, size_t capacity)
{
    return value_mode(command, name, out, capacity, 0);
}

/* Strict decimal input, independent of scanf/strtoul sign and overflow rules.
 * Versions reserve one signed-long value for the successful increment. */
static int decimal(const char *text, int allow_zero, unsigned long maximum,
                   unsigned long *result)
{
    unsigned long number, digit;
    const unsigned char *cursor;
    if (!text || !text[0] || !result) return 0;
    number = 0;
    cursor = (const unsigned char *)text;
    while (*cursor) {
        if (*cursor < '0' || *cursor > '9') return 0;
        digit = (unsigned long)(*cursor - '0');
        if (number > (maximum - digit) / 10UL) return 0;
        number = number * 10UL + digit;
        ++cursor;
    }
    if (!allow_zero && !number) return 0;
    *result = number;
    return 1;
}

static int bounded_string(const char *text, size_t capacity)
{
    return text[0] != '\0' && memchr(text, '\0', capacity) != NULL;
}

static int has_value(const WenaDomainCommand *command, const char *name)
{
    size_t start, end, equals, name_length;
    start = 0;
    name_length = strlen(name);
    while (start < command->form_body_length) {
        end = start;
        while (end < command->form_body_length && command->form_body[end] != '&') ++end;
        equals = start;
        while (equals < end && command->form_body[equals] != '=') ++equals;
        if (equals - start == name_length &&
            !memcmp(command->form_body + start, name, name_length)) return 1;
        start = end + 1;
    }
    return 0;
}

static void create_identity(const WenaDomainCommand *command,
                            const char *operation, char *id)
{
    char identity[400];
    sprintf(identity, "%s|%lu:%s%lu:%s%lu", operation,
        (unsigned long)strlen(command->user_id), command->user_id,
        (unsigned long)strlen(command->route), command->route,
        command->request_version);
    wena_sha256_hex((const unsigned char *)identity, strlen(identity), id);
}

int wena_mutation_text(const WenaDomainCommand *command,const char *name,
    char *out,size_t capacity,int mode)
{
    if (mode<0 || mode>2 || !command || !name || !out || !capacity) return 0;
    return value_mode(command,name,out,capacity,mode);
}
int wena_mutation_has_value(const WenaDomainCommand *command,const char *name)
{ return command && name ? has_value(command,name) : 0; }
int wena_mutation_decimal(const char *text,int allow_zero,unsigned long maximum,
    unsigned long *result)
{
    return decimal(text,allow_zero,maximum,result);
}
void wena_mutation_identity(const WenaDomainCommand *command,
    const char *operation,char *id)
{
    create_identity(command,operation,id);
}

static int create_hierarchy(sqlite3 *db, const WenaDomainCommand *command,
    const char *board, const char *title, char *id, double *position)
{
    sqlite3_stmt *statement;
    const char *sql, *query;
    sqlite3_int64 stored;
    int ok, is_list;
    const unsigned char *cursor;
    /* The shared region encoder validates UTF-8 before commit. Reject the C1
     * control block here as well as ASCII controls rejected by form decoding. */
    for (cursor = (const unsigned char *)title; *cursor; ++cursor)
        if (cursor[0] == 194 && cursor[1] >= 128 && cursor[1] <= 159) return 0;
    is_list = command->operation == WENA_DOMAIN_CREATE_LIST;
    create_identity(command, is_list ? "create-list" : "create-swimlane", id);
    sql = is_list ?
        "INSERT INTO lists(id,board_id,title,position,version) SELECT ?1,id,?3,"
        "COALESCE((SELECT max(position)+1 FROM lists WHERE board_id=?2),0),1 "
        "FROM boards WHERE id=?2" :
        "INSERT INTO swimlanes(id,board_id,title,position,version) SELECT ?1,id,?3,"
        "COALESCE((SELECT max(position)+1 FROM swimlanes WHERE board_id=?2),0),1 "
        "FROM boards WHERE id=?2";
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, title, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    if (!ok) return 0;
    query = is_list ? "SELECT position FROM lists WHERE id=?1" :
        "SELECT position FROM swimlanes WHERE id=?1";
    if (sqlite3_prepare_v2(db, query, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER;
    if (ok) {
        stored = sqlite3_column_int64(statement, 0);
        ok = stored >= 0 && (double)stored <= 9007199254740991.0;
        if (ok) *position = (double)stored;
    }
    sqlite3_finalize(statement);
    return ok;
}

static int card_position(sqlite3 *db, const char *id, double *position)
{
    sqlite3_stmt *statement;
    sqlite3_int64 stored;
    int ok;
    if (sqlite3_prepare_v2(db, "SELECT position FROM cards WHERE id=?1", -1,
        &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(statement) == SQLITE_ROW &&
        sqlite3_column_type(statement, 0) == SQLITE_INTEGER;
    if (ok) {
        stored = sqlite3_column_int64(statement, 0);
        ok = stored >= 0 && (double)stored <= 9007199254740991.0;
        if (ok) *position = (double)stored;
    }
    sqlite3_finalize(statement);
    return ok;
}

static int card_list_active(sqlite3 *db,const char *board,const char *card)
{
    sqlite3_stmt *statement;const unsigned char *text;char list[WENA_ID_CAPACITY];int ok,bytes;
    if(sqlite3_prepare_v2(db,"SELECT list_id FROM cards WHERE id=?1 AND board_id=?2",-1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(statement,1,card,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(statement)==SQLITE_ROW;
    if(ok){text=sqlite3_column_text(statement,0);bytes=sqlite3_column_bytes(statement,0);
        ok=sqlite3_column_type(statement,0)==SQLITE_TEXT&&text&&bytes>0&&bytes<(int)sizeof(list)&&!memchr(text,0,(size_t)bytes);
        if(ok){memcpy(list,text,(size_t)bytes);list[bytes]=0;ok=sqlite3_step(statement)==SQLITE_DONE;}}
    if(sqlite3_finalize(statement)!=SQLITE_OK)ok=0;
    return ok&&wena_sqlite_list_active(db,board,list);
}

static int create_card(sqlite3 *db, const WenaDomainCommand *command,
                       const char *board, const char *title, char *id,
                       double *position)
{
    sqlite3_stmt *statement;
    char list[65], lane[65], query[1024];
    int explicit_scope, ok, available;
    const char *sql;
    explicit_scope = has_value(command, "targetListId") ||
        has_value(command, "targetSwimlaneId");
    if (explicit_scope &&
        (!value(command, "targetListId", list, sizeof(list)) ||
         !value(command, "targetSwimlaneId", lane, sizeof(lane)))) return 0;
    available=wena_sqlite_list_state_available(db);
    if(available<0||(explicit_scope&&!wena_sqlite_list_active(db,board,list)))return 0;
    /* Length-delimited actor/route/request identity is stable on reopen and
     * separates actors/boards. The DB primary key remains the final collision
     * guard; requests never overwrite an existing card. */
    create_identity(command, "create-card", id);
    /* Legacy server callers omit both parents and choose the first pair.
     * Native callers always provide both; partial/invalid scope never falls
     * back to another list or lane. */
    sql = "INSERT INTO cards(id,board_id,swimlane_id,list_id,title,position,version) "
        "SELECT ?1,?2,s.id,l.id,?3,COALESCE((SELECT max(position)+1 FROM cards "
        "WHERE list_id=l.id AND swimlane_id=s.id),0),1 FROM swimlanes s,lists l "
        "WHERE s.board_id=?2 AND l.board_id=?2 AND (?4 IS NULL OR s.id=?4) "
        "AND (?5 IS NULL OR l.id=?5) %s ORDER BY s.position,l.position,s.id,l.id LIMIT 1";
    sprintf(query,sql,available ? "AND NOT EXISTS(SELECT 1 FROM list_archive_state a WHERE a.list_id=l.id AND a.archived<>0)" : "");
    if (sqlite3_prepare_v2(db, query, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, title, -1, SQLITE_TRANSIENT);
    if (explicit_scope) {
        sqlite3_bind_text(statement, 4, lane, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, list, -1, SQLITE_TRANSIENT);
    }
    ok = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    if (!ok || !card_list_active(db,board,id)) return 0;
    return card_position(db, id, position);
}

static int scalar(sqlite3 *db,const char *sql,const char *a,const char *b,const char *c,unsigned long n){sqlite3_stmt *s;int ok;if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;if(a)sqlite3_bind_text(s,1,a,-1,SQLITE_TRANSIENT);if(b)sqlite3_bind_text(s,2,b,-1,SQLITE_TRANSIENT);if(c)sqlite3_bind_text(s,3,c,-1,SQLITE_TRANSIENT);if(n)sqlite3_bind_int64(s,4,(sqlite3_int64)n);ok=sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)>0;sqlite3_finalize(s);return ok;}
static int board_id(const char *route,char *out){const char *end;if(strncmp(route,"/b/",3)!=0)return 0;end=strchr(route+3,'/');if(!end||end==route+3||(size_t)(end-route-3)>=65)return 0;memcpy(out,route+3,(size_t)(end-route-3));out[end-route-3]=0;return 1;}
static int run(sqlite3 *db,const char *sql,const char *a,const char *b,const char *c,unsigned long n){sqlite3_stmt *s;int ok;if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;if(a)sqlite3_bind_text(s,1,a,-1,SQLITE_TRANSIENT);if(b)sqlite3_bind_text(s,2,b,-1,SQLITE_TRANSIENT);if(c)sqlite3_bind_text(s,3,c,-1,SQLITE_TRANSIENT);if(n)sqlite3_bind_int64(s,4,(sqlite3_int64)n);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;sqlite3_finalize(s);return ok;}
static int move_card(sqlite3*d,const char*list,const char*lane,const char*id,const char*board,unsigned long version){sqlite3_stmt*s;int ok;const char*q="UPDATE cards SET list_id=?1,swimlane_id=?2,position=COALESCE((SELECT max(position)+1 FROM cards c2 WHERE c2.list_id=?1 AND c2.swimlane_id=?2),0),version=version+1 WHERE id=?3 AND board_id=?4 AND version=?5 AND archived=0";if(sqlite3_prepare_v2(d,q,-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,lane,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,3,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,4,board,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,5,(sqlite3_int64)version);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(d)==1;sqlite3_finalize(s);return ok;}
int wena_sqlite_hierarchy_order_add(WenaSha256 *state, const char *id, size_t length)
{
    size_t index;
    unsigned char c;
    char prefix[8];
    if (!state || !id || !length || length > 64) return 0;
    for (index = 0; index < length; ++index) {
        c = (unsigned char)id[index];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    sprintf(prefix, "%lu:", (unsigned long)length);
    wena_sha256_update(state, (const unsigned char *)prefix, strlen(prefix));
    wena_sha256_update(state, (const unsigned char *)id, length);
    return 1;
}

int wena_sqlite_card_order_add(WenaSha256 *state, const char *id,
    size_t length, unsigned long position)
{
    char number[32];
    if (!wena_sqlite_hierarchy_order_add(state,id,length)) return 0;
    sprintf(number,"/%lu;",position);
    wena_sha256_update(state,(const unsigned char*)number,strlen(number));
    return 1;
}

typedef struct WenaCardOrderRow {
    char id[65];
    sqlite3_int64 position;
    unsigned long version;
    int archived;
} WenaCardOrderRow;

/* Shared bounded column reader for reordering and cross-column insertion. */
static int card_column(sqlite3 *db,const char *board,const char *list,const char *lane,
    const char *expected,WenaCardOrderRow **output,size_t *length)
{
    WenaCardOrderRow *rows;
    WenaSha256 hash;sqlite3_stmt *statement;const unsigned char *id;
    sqlite3_int64 position,maximum;char actual[65];size_t count;
    int result,bytes,ok;
    if(!expected||strlen(expected)!=64)return 0;
    rows=(WenaCardOrderRow*)calloc(2048,sizeof(*rows));if(!rows)return 0;
    if(sqlite3_prepare_v2(db,"SELECT id,position,version,archived FROM cards WHERE board_id=?1 AND list_id=?2 AND swimlane_id=?3 ORDER BY position,id LIMIT 2049",-1,&statement,NULL)!=SQLITE_OK){free(rows);return 0;}
    sqlite3_bind_text(statement,1,board,-1,SQLITE_TRANSIENT);sqlite3_bind_text(statement,2,list,-1,SQLITE_TRANSIENT);sqlite3_bind_text(statement,3,lane,-1,SQLITE_TRANSIENT);
    wena_sha256_init(&hash);count=0;ok=1;maximum=-1;
    while((result=sqlite3_step(statement))==SQLITE_ROW){
        if(count>=2048||sqlite3_column_type(statement,0)!=SQLITE_TEXT||sqlite3_column_type(statement,1)!=SQLITE_INTEGER||sqlite3_column_type(statement,2)!=SQLITE_INTEGER||sqlite3_column_int64(statement,2)<=0||sqlite3_column_int64(statement,2)>(sqlite3_int64)WENA_VERSION_READ_MAX||sqlite3_column_type(statement,3)!=SQLITE_INTEGER){ok=0;break;}
        id=sqlite3_column_text(statement,0);bytes=sqlite3_column_bytes(statement,0);position=sqlite3_column_int64(statement,1);
        if(bytes<=0||bytes>64||position<0||position>=LONG_MAX-2048||
            (double)position>9007199254740991.0||position<=maximum||
            (sqlite3_column_int64(statement,3)!=0&&sqlite3_column_int64(statement,3)!=1)||
            !wena_sqlite_card_order_add(&hash,(const char*)id,(size_t)bytes,(unsigned long)position)){ok=0;break;}
        memcpy(rows[count].id,id,(size_t)bytes);rows[count].id[bytes]=0;rows[count].position=position;maximum=position;
        rows[count].version=(unsigned long)sqlite3_column_int64(statement,2);
        rows[count].archived=sqlite3_column_int(statement,3);++count;
    }
    if(result!=SQLITE_DONE)ok=0;
    sqlite3_finalize(statement);wena_sha256_final_hex(&hash,actual);
    if(!ok||strcmp(expected,actual)){free(rows);return 0;}
    *output=rows;*length=count;return 1;
}

/* Two passes avoid collisions with existing positions. Only the explicitly
 * supplied card advances its revision; an inserted card already advanced. */
static int write_card_column(sqlite3 *db,const char *board,const char *list,
    const char *lane,const WenaCardOrderRow *rows,size_t count,
    sqlite3_int64 maximum,const char *advance)
{
    sqlite3_stmt *statement;size_t index,actual_count;int ok,phase;
    WenaSha256 hash;WenaCardOrderRow *actual;char expected[65];
    if(sqlite3_prepare_v2(db,"UPDATE cards SET position=?1,version=version+?6 WHERE id=?2 AND board_id=?3 AND list_id=?4 AND swimlane_id=?5",-1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=1;
    for(phase=0;phase<2&&ok;++phase){
        for(index=0;index<count;++index){
            ok=sqlite3_reset(statement)==SQLITE_OK &&
                sqlite3_bind_int64(statement,1,phase?(sqlite3_int64)index:maximum+1+(sqlite3_int64)index)==SQLITE_OK &&
                sqlite3_bind_text(statement,2,rows[index].id,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
                sqlite3_bind_text(statement,3,board,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
                sqlite3_bind_text(statement,4,list,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
                sqlite3_bind_text(statement,5,lane,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
                sqlite3_bind_int(statement,6,phase&&advance&&!strcmp(rows[index].id,advance))==SQLITE_OK;
            if(!ok||sqlite3_step(statement)!=SQLITE_DONE||sqlite3_changes(db)!=1){ok=0;break;}
        }
    }
    sqlite3_finalize(statement);
    if(!ok)return 0;
    /* Re-read before commit: triggers must not silently change order, revision,
     * archive status or scope while the caller prepares its cache publication. */
    wena_sha256_init(&hash);
    for(index=0;index<count;++index)
        if(!wena_sqlite_card_order_add(&hash,rows[index].id,strlen(rows[index].id),(unsigned long)index))return 0;
    wena_sha256_final_hex(&hash,expected);actual=NULL;
    if(!card_column(db,board,list,lane,expected,&actual,&actual_count))return 0;
    ok=actual_count==count;
    for(index=0;index<count&&ok;++index)
        ok=actual[index].archived==rows[index].archived&&actual[index].version==
            rows[index].version+(advance&&!strcmp(rows[index].id,advance)?1ul:0ul);
    free(actual);return ok;
}
static int reorder_card(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,const char *card,const char *list,const char *lane,
    unsigned long version,unsigned long target)
{
    WenaCardOrderRow *rows,moved;char expected[65];size_t count,selected;
    sqlite3_int64 maximum;int ok;
    if(!value(command,"expectedOrder",expected,sizeof(expected))||
        !card_column(db,board,list,lane,expected,&rows,&count))return 0;
    for(selected=0;selected<count;++selected)if(!strcmp(rows[selected].id,card))break;
    if(selected==count||rows[selected].version!=version||rows[selected].archived||target>=(unsigned long)count){free(rows);return 0;}
    if(selected==(size_t)target){free(rows);return 2;}
    maximum=rows[count-1].position;moved=rows[selected];
    if(selected<(size_t)target)memmove(&rows[selected],&rows[selected+1],((size_t)target-selected)*sizeof(*rows));
    else memmove(&rows[target+1],&rows[target],(selected-(size_t)target)*sizeof(*rows));
    rows[target]=moved;
    ok=write_card_column(db,board,list,lane,rows,count,maximum,card);free(rows);return ok;
}
static int insert_card(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,const char *card,const char *list,const char *lane,
    unsigned long version,unsigned long target)
{
    WenaCardOrderRow *source,*destination,*remaining,moved;
    WenaSha256 hash;size_t source_count,count,selected,index,after_count,at;
    char source_list[65],source_lane[65],expected[65];sqlite3_int64 maximum;int ok;
    source=NULL;destination=NULL;remaining=NULL;ok=0;
    if(has_value(command,"targetPosition")||
        !value(command,"sourceListId",source_list,sizeof(source_list))||
        !value(command,"sourceSwimlaneId",source_lane,sizeof(source_lane))||
        (!strcmp(list,source_list)&&!strcmp(lane,source_lane))||
        !value(command,"expectedSourceOrder",expected,sizeof(expected))||
        !card_column(db,board,source_list,source_lane,expected,&source,&source_count))goto done;
    for(selected=0;selected<source_count;++selected)if(!strcmp(source[selected].id,card))break;
    if(selected==source_count||source[selected].version!=version||source[selected].archived||
        !value(command,"expectedOrder",expected,sizeof(expected))||
        !card_column(db,board,list,lane,expected,&destination,&count)||count>=2048||target>(unsigned long)count)goto done;
    maximum=count?destination[count-1].position+1:0;
    moved=source[selected];moved.position=maximum;++moved.version;
    memmove(&destination[target+1],&destination[target],(count-(size_t)target)*sizeof(*destination));
    destination[target]=moved;++count;
    if(!move_card(db,list,lane,card,board,version)||
        !write_card_column(db,board,list,lane,destination,count,maximum,NULL))goto done;
    wena_sha256_init(&hash);
    for(index=0;index<source_count;++index)if(index!=selected)
        if(!wena_sqlite_card_order_add(&hash,source[index].id,strlen(source[index].id),
            (unsigned long)source[index].position))goto done;
    wena_sha256_final_hex(&hash,expected);
    if(!card_column(db,board,source_list,source_lane,expected,&remaining,&after_count)||
        after_count!=source_count-1)goto done;
    at=0;
    for(index=0;index<source_count;++index)if(index!=selected){
        if(remaining[at].version!=source[index].version||
            remaining[at].archived!=source[index].archived)goto done;
        ++at;
    }
    ok=1;
 done:
    free(source);free(destination);free(remaining);return ok;
}

static int hierarchy_order(sqlite3 *db, const WenaDomainCommand *command,
                            const char *board, int lists)
{
    sqlite3_stmt *statement;
    WenaSha256 hash;
    char expected[65], actual[65];
    const unsigned char *id;
    size_t count;
    int bytes, result, ok;
    if (!has_value(command, "expectedOrder")) return 1;
    if (!value(command, "expectedOrder", expected, sizeof(expected)) || strlen(expected) != 64)
        return 0;
    if (sqlite3_prepare_v2(db, lists ?
        "SELECT id,position FROM lists WHERE board_id=?1 ORDER BY position,id LIMIT 129" :
        "SELECT id,position FROM swimlanes WHERE board_id=?1 ORDER BY position,id LIMIT 65",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    wena_sha256_init(&hash); count = 0; ok = 1;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        if (count >= (lists ? 128u : 64u) ||
            sqlite3_column_type(statement, 0) != SQLITE_TEXT ||
            sqlite3_column_type(statement, 1) != SQLITE_INTEGER ||
            sqlite3_column_int64(statement, 1) != (sqlite3_int64)count) {ok=0;break;}
        id = sqlite3_column_text(statement, 0);
        bytes = sqlite3_column_bytes(statement, 0);
        if (bytes <= 0 || !wena_sqlite_hierarchy_order_add(&hash,(const char*)id,(size_t)bytes)) {ok=0;break;}
        ++count;
    }
    if (result != SQLITE_DONE) ok = 0;
    sqlite3_finalize(statement);
    if (!ok) return 0;
    wena_sha256_final_hex(&hash, actual);
    return strcmp(actual, expected) == 0;
}

static int hierarchy_noop(sqlite3 *db, const char *id, const char *board,
                          unsigned long version, unsigned long target, int lists)
{
    sqlite3_stmt *statement;
    int ok;
    if (sqlite3_prepare_v2(db, lists ?
        "SELECT 1 FROM lists WHERE id=?1 AND board_id=?2 AND version=?3 AND position=?4" :
        "SELECT 1 FROM swimlanes WHERE id=?1 AND board_id=?2 AND version=?3 AND position=?4",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement,3,(sqlite3_int64)version);
    sqlite3_bind_int64(statement,4,(sqlite3_int64)target);
    ok = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return ok;
}

static int move_list(sqlite3*d,const char*id,const char*board,unsigned long target,unsigned long version){sqlite3_stmt*s;int old,count,ok=0;if(sqlite3_prepare_v2(d,"SELECT position,(SELECT count(*) FROM lists WHERE board_id=?2) FROM lists WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,3,(sqlite3_int64)version);if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return 0;}old=sqlite3_column_int(s,0);count=sqlite3_column_int(s,1);sqlite3_finalize(s);if(target>=(unsigned long)count)return 0;if(sqlite3_prepare_v2(d,"UPDATE lists SET position=position+100000 WHERE board_id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,board,-1,SQLITE_TRANSIENT);if(sqlite3_step(s)!=SQLITE_DONE){sqlite3_finalize(s);return 0;}sqlite3_finalize(s);if(sqlite3_prepare_v2(d,"UPDATE lists SET position=CASE WHEN id=?1 THEN ?3 WHEN ?2>?3 AND position-100000>=?3 AND position-100000<?2 THEN position-99999 WHEN ?2<?3 AND position-100000>?2 AND position-100000<=?3 THEN position-100001 ELSE position-100000 END,version=CASE WHEN id=?1 THEN version+1 ELSE version END WHERE board_id=?4",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,old);sqlite3_bind_int64(s,3,(sqlite3_int64)target);sqlite3_bind_text(s,4,board,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(d)==count;sqlite3_finalize(s);return ok;}
static int move_swimlane(sqlite3*d,const char*id,const char*board,unsigned long target,unsigned long version){sqlite3_stmt*s;int old,count,ok=0;if(sqlite3_prepare_v2(d,"SELECT position,(SELECT count(*) FROM swimlanes WHERE board_id=?2) FROM swimlanes WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,3,(sqlite3_int64)version);if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return 0;}old=sqlite3_column_int(s,0);count=sqlite3_column_int(s,1);sqlite3_finalize(s);if(target>=(unsigned long)count)return 0;if(sqlite3_prepare_v2(d,"UPDATE swimlanes SET position=position+100000 WHERE board_id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,board,-1,SQLITE_TRANSIENT);if(sqlite3_step(s)!=SQLITE_DONE){sqlite3_finalize(s);return 0;}sqlite3_finalize(s);if(sqlite3_prepare_v2(d,"UPDATE swimlanes SET position=CASE WHEN id=?1 THEN ?3 WHEN ?2>?3 AND position-100000>=?3 AND position-100000<?2 THEN position-99999 WHEN ?2<?3 AND position-100000>?2 AND position-100000<=?3 THEN position-100001 ELSE position-100000 END,version=CASE WHEN id=?1 THEN version+1 ELSE version END WHERE board_id=?4",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,old);sqlite3_bind_int64(s,3,(sqlite3_int64)target);sqlite3_bind_text(s,4,board,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(d)==count;sqlite3_finalize(s);return ok;}
static int checksum(sqlite3 *db,const char *wire,size_t length){sqlite3_stmt*s;char hash[65];int ok;wena_sha256_hex((const unsigned char *)wire,length,hash);if(sqlite3_prepare_v2(db,"UPDATE idempotency_keys SET response_checksum=?1 WHERE response_checksum='pending'",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,hash,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;sqlite3_finalize(s);return ok;}
/* Checklist operations share the caller's guarded transaction and idempotency.
 * Millisecond timestamps use a portable SQLite epoch-second clock, never regress. */
static int checklist_statement(sqlite3 *db,const char *sql,const char *board,
    const char *card,const char *list,const char *item,const char *title,
    unsigned long cv,unsigned long lv,unsigned long iv,int finished,const char *id,
    int hide_checked,int hide_all,int minicard,int query)
{
    sqlite3_stmt *s;
    int ok;
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,board,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,card,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,list,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,item,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,5,title,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,6,(sqlite3_int64)cv);
    sqlite3_bind_int64(s,7,(sqlite3_int64)lv);
    sqlite3_bind_int64(s,8,(sqlite3_int64)iv);
    sqlite3_bind_int(s,9,finished);
    sqlite3_bind_text(s,10,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,11,hide_checked);sqlite3_bind_int(s,12,hide_all);
    if(minicard<0)sqlite3_bind_null(s,13);else sqlite3_bind_int(s,13,minicard);
    if(query==1)ok=sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)==1;
    else if(query==2)ok=sqlite3_step(s)==SQLITE_DONE;
    else ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    sqlite3_finalize(s);return ok;
}

static int checklist_change(sqlite3 *db,const WenaDomainCommand *c,
    const char *board,unsigned long *result_version)
{
    char card[65],list[65],item[65],title[129],number[32],id[65];
    unsigned long cv,lv,iv,finished,hide_checked,hide_all;
    const char *sql,*op;
    int create,add,rename,itemedit,flags,minicard,remove_list,remove_item;
    remove_list=c->operation==WENA_DOMAIN_DELETE_CHECKLIST;
    remove_item=c->operation==WENA_DOMAIN_DELETE_CHECKLIST_ITEM;
    flags=c->operation==WENA_DOMAIN_SET_CHECKLIST_FLAGS;minicard=-1;hide_checked=hide_all=0;
    create=c->operation==WENA_DOMAIN_CREATE_CHECKLIST;
    add=c->operation==WENA_DOMAIN_ADD_CHECKLIST_ITEM;
    rename=c->operation==WENA_DOMAIN_RENAME_CHECKLIST;
    itemedit=c->operation==WENA_DOMAIN_RENAME_CHECKLIST_ITEM||c->operation==WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED||remove_item;
    list[0]=item[0]=title[0]=id[0]=0;lv=iv=finished=0;
    if(!value(c,"cardId",card,sizeof(card))||!wena_model_identifier_valid(card)||
        !value(c,"expectedVersion",number,sizeof(number))||
        !decimal(number,0,WENA_VERSION_MUTATE_MAX,&cv))return 0;
    if(!create&&(!value(c,"checklistId",list,sizeof(list))||!wena_model_identifier_valid(list)||
        !value(c,"expectedChecklistVersion",number,sizeof(number))||
        !decimal(number,0,WENA_VERSION_MUTATE_MAX,&lv)))return 0;
    if(itemedit&&(!value(c,"itemId",item,sizeof(item))||!wena_model_identifier_valid(item)||
        !value(c,"expectedItemVersion",number,sizeof(number))||
        !decimal(number,0,WENA_VERSION_MUTATE_MAX,&iv)))return 0;
    if(c->operation==WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED){
        if(!value(c,"isFinished",number,sizeof(number))||!decimal(number,1,1,&finished))return 0;
    }else if(flags){
        if(!value(c,"hideChecked",number,sizeof(number))||!decimal(number,1,1,&hide_checked)||
            !value(c,"hideAll",number,sizeof(number))||!decimal(number,1,1,&hide_all)||
            !value(c,"showOnMinicard",number,sizeof(number)))return 0;
        if(!strcmp(number,"-1"))minicard=-1;
        else if(!strcmp(number,"0"))minicard=0;
        else if(!strcmp(number,"1"))minicard=1;
        else return 0;
    }else if(!remove_list&&!remove_item&&(!value(c,"title",title,sizeof(title))||!wena_model_title_string_valid(title,sizeof(title))))return 0;
#define CHECKLIST_SQL(q,read) checklist_statement(db,q,board,card,list,item,title,cv,lv,iv,(int)finished,id,(int)hide_checked,(int)hide_all,minicard,read)
    if(!CHECKLIST_SQL("SELECT count(*) FROM cards WHERE board_id=?1 AND id=?2 AND archived=0 AND typeof(version)='integer' AND version=?6",1))return 0;
    if(!create&&!CHECKLIST_SQL("SELECT count(*) FROM checklists WHERE board_id=?1 AND card_id=?2 AND id=?3 AND typeof(version)='integer' AND version=?7",1))return 0;
    if(itemedit&&!CHECKLIST_SQL("SELECT count(*) FROM checklist_items WHERE board_id=?1 AND card_id=?2 AND checklist_id=?3 AND id=?4 AND typeof(version)='integer' AND version=?8",1))return 0;
    if(rename||(itemedit&&!remove_item)||flags){
        sql=flags?"SELECT count(*) FROM checklists WHERE id=?3 AND hide_checked_items=?11 AND hide_all_items=?12 AND show_on_minicard IS ?13":rename?"SELECT count(*) FROM checklists WHERE id=?3 AND title=?5":
            c->operation==WENA_DOMAIN_RENAME_CHECKLIST_ITEM?
            "SELECT count(*) FROM checklist_items WHERE id=?4 AND title=?5":
            "SELECT count(*) FROM checklist_items WHERE id=?4 AND is_finished=?9";
        if(CHECKLIST_SQL(sql,1)){*result_version=cv;return 2;}
    }
    if(remove_list){
        /* Reject corrupt foreign-scope children instead of leaving orphans when
         * the caller opened SQLite with foreign-key enforcement disabled. */
        if(!CHECKLIST_SQL("SELECT CASE WHEN count(*)=count(CASE WHEN board_id=?1 AND card_id=?2 THEN 1 END) THEN 1 ELSE 0 END FROM checklist_items WHERE checklist_id=?3",1))return 0;
        if(!CHECKLIST_SQL("DELETE FROM checklist_items WHERE board_id=?1 AND card_id=?2 AND checklist_id=?3",2))return 0;
        if(!CHECKLIST_SQL("SELECT CASE WHEN count(*)=0 THEN 1 ELSE 0 END FROM checklist_items WHERE checklist_id=?3",1))return 0;
        sql="DELETE FROM checklists WHERE board_id=?1 AND card_id=?2 AND id=?3 AND version=?7";
    }else if(remove_item){
        sql="DELETE FROM checklist_items WHERE board_id=?1 AND card_id=?2 AND checklist_id=?3 AND id=?4 AND version=?8";
    }else if(create||add){
        sql=create?
            "SELECT CASE WHEN count(*)<64 AND COALESCE(max(position),-1)<2147483647 AND count(*)=count(CASE WHEN typeof(position)='integer' AND position>=0 THEN 1 END) THEN 1 ELSE 0 END FROM checklists WHERE card_id=?2":
            "SELECT CASE WHEN count(*)<1024 AND count(*)=count(CASE WHEN typeof(position)='integer' AND position>=0 AND position<2147483647 THEN 1 END) THEN 1 ELSE 0 END FROM checklist_items WHERE card_id=?2";
        if(!CHECKLIST_SQL(sql,1))return 0;
        op=create?"create-checklist":"add-checklist-item";create_identity(c,op,id);
        sql=create?
            "INSERT INTO checklists(id,board_id,card_id,title,position,version,created_at,updated_at) VALUES(?10,?1,?2,?5,COALESCE((SELECT max(position)+1 FROM checklists WHERE card_id=?2),0),1,CAST(strftime('%s','now') AS INTEGER)*1000,CAST(strftime('%s','now') AS INTEGER)*1000)":
            "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished,version,created_at,updated_at) VALUES(?10,?1,?2,?3,?5,COALESCE((SELECT max(position)+1 FROM checklist_items WHERE checklist_id=?3),0),0,1,CAST(strftime('%s','now') AS INTEGER)*1000,CAST(strftime('%s','now') AS INTEGER)*1000)";
    }else if(flags)sql="UPDATE checklists SET hide_checked_items=?11,hide_all_items=?12,show_on_minicard=?13,version=version+1,updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE id=?3 AND version=?7";
    else if(rename)sql="UPDATE checklists SET title=?5,version=version+1,updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE id=?3 AND version=?7";
    else if(c->operation==WENA_DOMAIN_RENAME_CHECKLIST_ITEM)sql="UPDATE checklist_items SET title=?5,version=version+1,updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE id=?4 AND version=?8";
    else sql="UPDATE checklist_items SET is_finished=?9,version=version+1,updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE id=?4 AND version=?8";
    if(!CHECKLIST_SQL(sql,0))return 0;
    if((add||remove_item)&&!CHECKLIST_SQL("UPDATE checklists SET version=version+1,updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE id=?3 AND version=?7",0))return 0;
    if(!CHECKLIST_SQL("UPDATE cards SET version=version+1 WHERE board_id=?1 AND id=?2 AND version=?6 AND archived=0",0))return 0;
#undef CHECKLIST_SQL
    *result_version=cv+1;return 1;
}

static const char *operation_name(WenaDomainOperation operation){if(operation==WENA_DOMAIN_ARCHIVE_LIST)return "archive-list";if(operation==WENA_DOMAIN_RESTORE_LIST)return "restore-list";if(operation==WENA_DOMAIN_SET_BOARD_PRESENTATION)return "set-board-presentation";if(operation==WENA_DOMAIN_MOVE_CHECKLIST_ITEM)return "move-checklist-item";if(operation==WENA_DOMAIN_MOVE_CHECKLIST)return "move-checklist";if(operation==WENA_DOMAIN_REORDER_CHECKLIST)return "reorder-checklist";if(operation==WENA_DOMAIN_REORDER_CHECKLIST_ITEM)return "reorder-checklist-item";if(operation==WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT)return "set-board-checklist-count";if(operation==WENA_DOMAIN_ADD_CHECKLIST_ITEMS)return "add-checklist-items";if(operation==WENA_DOMAIN_CREATE_LABEL)return "create-label";if(operation==WENA_DOMAIN_EDIT_LABEL)return "edit-label";if(operation==WENA_DOMAIN_DELETE_LABEL)return "delete-label";if(operation==WENA_DOMAIN_ASSIGN_LABEL)return "assign-label";if(operation==WENA_DOMAIN_UNASSIGN_LABEL)return "unassign-label";if(operation==WENA_DOMAIN_CREATE_CARD)return "create-card";if(operation==WENA_DOMAIN_EDIT_CARD_TITLE)return "edit-card-title";if(operation==WENA_DOMAIN_ARCHIVE_CARD)return "archive-card";if(operation==WENA_DOMAIN_EDIT_BOARD_TITLE)return "edit-board-title";if(operation==WENA_DOMAIN_EDIT_LIST_TITLE)return "edit-list-title";if(operation==WENA_DOMAIN_EDIT_SWIMLANE_TITLE)return "edit-swimlane-title";if(operation==WENA_DOMAIN_MOVE_CARD)return "move-card";if(operation==WENA_DOMAIN_MOVE_LIST)return "move-list";if(operation==WENA_DOMAIN_MOVE_SWIMLANE)return "move-swimlane";if(operation==WENA_DOMAIN_CREATE_LIST)return "create-list";if(operation==WENA_DOMAIN_CREATE_SWIMLANE)return "create-swimlane";if(operation==WENA_DOMAIN_RESTORE_CARD)return "restore-card";if(operation==WENA_DOMAIN_EDIT_CARD_DESCRIPTION)return "edit-card-description";if(operation==WENA_DOMAIN_CREATE_CHECKLIST)return "create-checklist";if(operation==WENA_DOMAIN_RENAME_CHECKLIST)return "rename-checklist";if(operation==WENA_DOMAIN_ADD_CHECKLIST_ITEM)return "add-checklist-item";if(operation==WENA_DOMAIN_RENAME_CHECKLIST_ITEM)return "rename-checklist-item";if(operation==WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED)return "set-checklist-item-finished";if(operation==WENA_DOMAIN_SET_CHECKLIST_FLAGS)return "set-checklist-flags";if(operation==WENA_DOMAIN_DELETE_CHECKLIST)return "delete-checklist";if(operation==WENA_DOMAIN_DELETE_CHECKLIST_ITEM)return "delete-checklist-item";return NULL;}
void wena_sqlite_persistence_init(WenaSqlitePersistence *s,sqlite3 *db){if(s){memset(s,0,sizeof(*s));s->database=db;}}
int wena_sqlite_persistence_apply(void *context,const WenaDomainCommand *c,WenaRegionResponse *r){WenaSqlitePersistence *s=(WenaSqlitePersistence *)context;sqlite3 *db;char board[65],id[65],title[129],expected[32],wire[WENA_REGION_RESPONSE_MAX_BYTES];const char*op;size_t wire_len;unsigned long version;int unchanged=0;double created_position=0.0;if(s){s->created_card_id[0]=0;s->created_card_position=0.0;s->moved_card_position=0.0;s->created_hierarchy_id[0]=0;s->created_hierarchy_position=0.0;}if(!r)return 0;memset(r,0,sizeof(*r));if(!s||!(db=s->database)||!c||!bounded_string(c->route,sizeof(c->route))||!bounded_string(c->user_id,sizeof(c->user_id))||c->request_version==0||c->request_version>(unsigned long)LONG_MAX||c->form_body_length>=sizeof(c->form_body)||!board_id(c->route,board)||(op=operation_name(c->operation))==NULL)return 0;if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)return 0;if(!scalar(db,"SELECT count(*) FROM actors WHERE id=?1",c->user_id,NULL,NULL,0)||scalar(db,"SELECT count(*) FROM idempotency_keys WHERE actor_id=?1 AND route=?2 AND operation=?3 AND request_version=?4",c->user_id,c->route,op,c->request_version))goto bad;
if(c->operation==WENA_DOMAIN_ARCHIVE_LIST||c->operation==WENA_DOMAIN_RESTORE_LIST){int changed;changed=wena_sqlite_list_archive_change(db,c,board,&version);if(!changed)goto bad;if(changed==2)unchanged=1;strcpy(title,c->operation==WENA_DOMAIN_ARCHIVE_LIST?"List archived":"List restored");
}else if(c->operation==WENA_DOMAIN_MOVE_CHECKLIST_ITEM){if(!wena_sqlite_checklist_item_move(db,c,board,&version))goto bad;strcpy(title,"Checklist item moved");
}else if(c->operation==WENA_DOMAIN_MOVE_CHECKLIST){if(!wena_sqlite_checklist_move(db,c,board,&version))goto bad;strcpy(title,"Checklist moved");
}else if(c->operation==WENA_DOMAIN_REORDER_CHECKLIST||c->operation==WENA_DOMAIN_REORDER_CHECKLIST_ITEM){int changed;changed=wena_sqlite_checklist_order(db,c,board,&version);if(!changed)goto bad;unchanged=changed==2;strcpy(title,"Checklist updated");
}else if(c->operation==WENA_DOMAIN_SET_BOARD_CHECKLIST_COUNT||c->operation==WENA_DOMAIN_SET_BOARD_PRESENTATION){int changed;changed=wena_sqlite_board_settings_change(db,c,board,&version);if(!changed)goto bad;unchanged=changed==2;strcpy(title,"Board settings updated");
}else if(c->operation==WENA_DOMAIN_ADD_CHECKLIST_ITEMS){if(!wena_sqlite_checklist_batch(db,c,board,&version))goto bad;strcpy(title,"Checklist updated");
}else if(c->operation>=WENA_DOMAIN_CREATE_LABEL&&c->operation<=WENA_DOMAIN_UNASSIGN_LABEL){int changed;changed=wena_sqlite_labels_change(db,c,board,&version);if(!changed)goto bad;unchanged=changed==2;strcpy(title,"Labels updated");
}else if(c->operation>=WENA_DOMAIN_CREATE_CHECKLIST&&c->operation<=WENA_DOMAIN_DELETE_CHECKLIST_ITEM){int changed;changed=checklist_change(db,c,board,&version);if(!changed)goto bad;unchanged=changed==2;strcpy(title,"Checklist updated");
}else if(c->operation==WENA_DOMAIN_CREATE_CARD){if(!value(c,"title",title,sizeof(title))||!create_card(db,c,board,title,id,&created_position))goto bad;version=1;
}else if(c->operation==WENA_DOMAIN_CREATE_LIST||c->operation==WENA_DOMAIN_CREATE_SWIMLANE){if(!value(c,"title",title,sizeof(title))||!create_hierarchy(db,c,board,title,id,&created_position))goto bad;version=1;
}else if(c->operation==WENA_DOMAIN_EDIT_CARD_DESCRIPTION){char description[WENA_DESCRIPTION_CAPACITY];if(!value(c,"cardId",id,sizeof(id))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&version)||!value_mode(c,"description",description,sizeof(description),1)||!wena_model_description_valid(description,strlen(description)))goto bad;
if(!run(db,"UPDATE cards SET version=version+1 WHERE id=?1 AND board_id=?2 AND version=?4 AND archived=0",id,board,NULL,version)||!run(db,"INSERT INTO card_descriptions(card_id,board_id,description) VALUES(?1,?2,?3) ON CONFLICT(card_id) DO UPDATE SET description=excluded.description WHERE card_descriptions.board_id=excluded.board_id",id,board,description,0))goto bad;
strcpy(title,"Description updated");version++;
}else if(c->operation==WENA_DOMAIN_EDIT_BOARD_TITLE){if(!value(c,"title",title,sizeof(title))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&version)||!run(db,"UPDATE boards SET title=?1,version=version+1 WHERE id=?2 AND id=?3 AND version=?4",title,board,board,version))goto bad;version++;
}else if(c->operation==WENA_DOMAIN_EDIT_LIST_TITLE){if(!value(c,"listId",id,sizeof(id))||!value(c,"title",title,sizeof(title))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&version)||!run(db,"UPDATE lists SET title=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4",title,id,board,version))goto bad;version++;
}else if(c->operation==WENA_DOMAIN_EDIT_SWIMLANE_TITLE){if(!value(c,"swimlaneId",id,sizeof(id))||!value(c,"title",title,sizeof(title))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&version)||!run(db,"UPDATE swimlanes SET title=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4",title,id,board,version))goto bad;version++;
}else if(c->operation==WENA_DOMAIN_MOVE_CARD){char list[65],lane[65];if(!value(c,"cardId",id,sizeof(id))||!value(c,"targetListId",list,sizeof(list))||!value(c,"targetSwimlaneId",lane,sizeof(lane))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&version))goto bad;if(!card_list_active(db,board,id)||!wena_sqlite_list_active(db,board,list)||!scalar(db,"SELECT count(*) FROM swimlanes WHERE id=?1 AND board_id=?2",lane,board,NULL,0))goto bad;
if(has_value(c,"insertPosition")){char target[32];unsigned long ordinal;if(!value(c,"insertPosition",target,sizeof(target))||!decimal(target,1,(unsigned long)LONG_MAX,&ordinal)||!insert_card(db,c,board,id,list,lane,version,ordinal))goto bad;version++;}
else if(has_value(c,"targetPosition")){char target[32];unsigned long ordinal;int reordered;if(!value(c,"targetPosition",target,sizeof(target))||!decimal(target,1,(unsigned long)LONG_MAX,&ordinal))goto bad;reordered=reorder_card(db,c,board,id,list,lane,version,ordinal);if(!reordered)goto bad;if(reordered==2)unchanged=1;else version++;}
else{if(has_value(c,"expectedOrder")||!move_card(db,list,lane,id,board,version))goto bad;version++;}
if(!card_position(db,id,&created_position))goto bad;
strcpy(title,"Moved");
}else if(c->operation==WENA_DOMAIN_MOVE_LIST){char target[32];if(!value(c,"listId",id,sizeof(id))||!value(c,"targetPosition",target,sizeof(target))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(target,1,(unsigned long)LONG_MAX,&version))goto bad;{unsigned long expected_version;if(!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&expected_version)||!wena_sqlite_list_active(db,board,id)||!hierarchy_order(db,c,board,1))goto bad;if(has_value(c,"expectedOrder")&&hierarchy_noop(db,id,board,expected_version,version,1)){unchanged=1;version=expected_version;}else{if(!move_list(db,id,board,version,expected_version))goto bad;version=expected_version+1;}}strcpy(title,"Moved list");
}else if(c->operation==WENA_DOMAIN_MOVE_SWIMLANE){char target[32];if(!value(c,"swimlaneId",id,sizeof(id))||!value(c,"targetPosition",target,sizeof(target))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(target,1,(unsigned long)LONG_MAX,&version))goto bad;{unsigned long expected_version;if(!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&expected_version)||!hierarchy_order(db,c,board,0))goto bad;if(has_value(c,"expectedOrder")&&hierarchy_noop(db,id,board,expected_version,version,0)){unchanged=1;version=expected_version;}else{if(!move_swimlane(db,id,board,version,expected_version))goto bad;version=expected_version+1;}}strcpy(title,"Moved swimlane");
}else{if(!value(c,"cardId",id,sizeof(id))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,WENA_VERSION_MUTATE_MAX,&version))goto bad;if(c->operation==WENA_DOMAIN_EDIT_CARD_TITLE){if(!value(c,"title",title,sizeof(title))||!run(db,"UPDATE cards SET title=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4 AND archived=0",title,id,board,version))goto bad;}else if(c->operation==WENA_DOMAIN_ARCHIVE_CARD){strcpy(title,"Archived");if(!run(db,"UPDATE cards SET archived=1,version=version+1 WHERE id=?1 AND board_id=?2 AND version=?4 AND archived=0",id,board,NULL,version))goto bad;}else if(c->operation==WENA_DOMAIN_RESTORE_CARD){strcpy(title,"Restored");if(!run(db,"UPDATE cards SET archived=0,version=version+1 WHERE id=?1 AND board_id=?2 AND version=?4 AND archived=1",id,board,NULL,version))goto bad;}else goto bad;version++;}
r->request_version=c->request_version;r->region_count=1;strcpy(r->regions[0].name,"board");r->regions[0].version=version;strcpy(r->regions[0].content,title);r->regions[0].content_length=strlen(title);if(!wena_region_response_encode(r,wire,sizeof(wire),&wire_len))goto bad;if(unchanged){if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto bad;if(c->operation==WENA_DOMAIN_MOVE_CARD)s->moved_card_position=created_position;return 1;}if(!run(db,"INSERT INTO idempotency_keys(actor_id,route,operation,request_version,response_checksum,committed_at) VALUES(?1,?2,?3,?4,'pending',strftime('%s','now'))",c->user_id,c->route,op,c->request_version)||!checksum(db,wire,wire_len))goto bad;if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto bad;if(c->operation==WENA_DOMAIN_CREATE_CARD){strcpy(s->created_card_id,id);s->created_card_position=created_position;}else if(c->operation==WENA_DOMAIN_MOVE_CARD){s->moved_card_position=created_position;}else if(c->operation==WENA_DOMAIN_CREATE_LIST||c->operation==WENA_DOMAIN_CREATE_SWIMLANE){strcpy(s->created_hierarchy_id,id);s->created_hierarchy_position=created_position;}return 1;bad:sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);memset(r,0,sizeof(*r));return 0;}
