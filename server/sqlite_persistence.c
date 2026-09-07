#include "sqlite_persistence.h"
#include "sha256.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int value(const WenaDomainCommand *command, const char *name,
                 char *out, size_t capacity)
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
                if (c < 32 || c == 127 || length + 1 >= capacity) return 0;
                out[length++] = (char)c;
            }
            if (!length) return 0;
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

static int create_card(sqlite3 *db, const WenaDomainCommand *command,
                       const char *board, const char *title, char *id,
                       double *position)
{
    sqlite3_stmt *statement;
    char list[65], lane[65];
    int explicit_scope, ok;
    const char *sql;
    explicit_scope = has_value(command, "targetListId") ||
        has_value(command, "targetSwimlaneId");
    if (explicit_scope &&
        (!value(command, "targetListId", list, sizeof(list)) ||
         !value(command, "targetSwimlaneId", lane, sizeof(lane)))) return 0;
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
        "AND (?5 IS NULL OR l.id=?5) ORDER BY s.position,l.position,s.id,l.id LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, title, -1, SQLITE_TRANSIENT);
    if (explicit_scope) {
        sqlite3_bind_text(statement, 4, lane, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, list, -1, SQLITE_TRANSIENT);
    }
    ok = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    if (!ok) return 0;
    return card_position(db, id, position);
}

static int scalar(sqlite3 *db,const char *sql,const char *a,const char *b,const char *c,unsigned long n){sqlite3_stmt *s;int ok;if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;if(a)sqlite3_bind_text(s,1,a,-1,SQLITE_TRANSIENT);if(b)sqlite3_bind_text(s,2,b,-1,SQLITE_TRANSIENT);if(c)sqlite3_bind_text(s,3,c,-1,SQLITE_TRANSIENT);if(n)sqlite3_bind_int64(s,4,(sqlite3_int64)n);ok=sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)>0;sqlite3_finalize(s);return ok;}
static int board_id(const char *route,char *out){const char *end;if(strncmp(route,"/b/",3)!=0)return 0;end=strchr(route+3,'/');if(!end||end==route+3||(size_t)(end-route-3)>=65)return 0;memcpy(out,route+3,(size_t)(end-route-3));out[end-route-3]=0;return 1;}
static int run(sqlite3 *db,const char *sql,const char *a,const char *b,const char *c,unsigned long n){sqlite3_stmt *s;int ok;if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;if(a)sqlite3_bind_text(s,1,a,-1,SQLITE_TRANSIENT);if(b)sqlite3_bind_text(s,2,b,-1,SQLITE_TRANSIENT);if(c)sqlite3_bind_text(s,3,c,-1,SQLITE_TRANSIENT);if(n)sqlite3_bind_int64(s,4,(sqlite3_int64)n);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;sqlite3_finalize(s);return ok;}
static int move_card(sqlite3*d,const char*list,const char*lane,const char*id,const char*board,unsigned long version){sqlite3_stmt*s;int ok;const char*q="UPDATE cards SET list_id=?1,swimlane_id=?2,position=COALESCE((SELECT max(position)+1 FROM cards c2 WHERE c2.list_id=?1 AND c2.swimlane_id=?2),0),version=version+1 WHERE id=?3 AND board_id=?4 AND version=?5 AND archived=0";if(sqlite3_prepare_v2(d,q,-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,list,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,lane,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,3,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,4,board,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,5,(sqlite3_int64)version);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(d)==1;sqlite3_finalize(s);return ok;}
static int move_list(sqlite3*d,const char*id,const char*board,unsigned long target,unsigned long version){sqlite3_stmt*s;int old,count,ok=0;if(sqlite3_prepare_v2(d,"SELECT position,(SELECT count(*) FROM lists WHERE board_id=?2) FROM lists WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,3,(sqlite3_int64)version);if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return 0;}old=sqlite3_column_int(s,0);count=sqlite3_column_int(s,1);sqlite3_finalize(s);if(target>=(unsigned long)count)return 0;if(sqlite3_prepare_v2(d,"UPDATE lists SET position=position+100000 WHERE board_id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,board,-1,SQLITE_TRANSIENT);if(sqlite3_step(s)!=SQLITE_DONE){sqlite3_finalize(s);return 0;}sqlite3_finalize(s);if(sqlite3_prepare_v2(d,"UPDATE lists SET position=CASE WHEN id=?1 THEN ?3 WHEN ?2>?3 AND position-100000>=?3 AND position-100000<?2 THEN position-99999 WHEN ?2<?3 AND position-100000>?2 AND position-100000<=?3 THEN position-100001 ELSE position-100000 END,version=CASE WHEN id=?1 THEN version+1 ELSE version END WHERE board_id=?4",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,old);sqlite3_bind_int64(s,3,(sqlite3_int64)target);sqlite3_bind_text(s,4,board,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(d)==count;sqlite3_finalize(s);return ok;}
static int move_swimlane(sqlite3*d,const char*id,const char*board,unsigned long target,unsigned long version){sqlite3_stmt*s;int old,count,ok=0;if(sqlite3_prepare_v2(d,"SELECT position,(SELECT count(*) FROM swimlanes WHERE board_id=?2) FROM swimlanes WHERE id=?1 AND board_id=?2 AND version=?3",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT);sqlite3_bind_int64(s,3,(sqlite3_int64)version);if(sqlite3_step(s)!=SQLITE_ROW){sqlite3_finalize(s);return 0;}old=sqlite3_column_int(s,0);count=sqlite3_column_int(s,1);sqlite3_finalize(s);if(target>=(unsigned long)count)return 0;if(sqlite3_prepare_v2(d,"UPDATE swimlanes SET position=position+100000 WHERE board_id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,board,-1,SQLITE_TRANSIENT);if(sqlite3_step(s)!=SQLITE_DONE){sqlite3_finalize(s);return 0;}sqlite3_finalize(s);if(sqlite3_prepare_v2(d,"UPDATE swimlanes SET position=CASE WHEN id=?1 THEN ?3 WHEN ?2>?3 AND position-100000>=?3 AND position-100000<?2 THEN position-99999 WHEN ?2<?3 AND position-100000>?2 AND position-100000<=?3 THEN position-100001 ELSE position-100000 END,version=CASE WHEN id=?1 THEN version+1 ELSE version END WHERE board_id=?4",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);sqlite3_bind_int(s,2,old);sqlite3_bind_int64(s,3,(sqlite3_int64)target);sqlite3_bind_text(s,4,board,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(d)==count;sqlite3_finalize(s);return ok;}
static int checksum(sqlite3 *db,const char *wire,size_t length){sqlite3_stmt*s;char hash[65];int ok;wena_sha256_hex((const unsigned char *)wire,length,hash);if(sqlite3_prepare_v2(db,"UPDATE idempotency_keys SET response_checksum=?1 WHERE response_checksum='pending'",-1,&s,NULL)!=SQLITE_OK)return 0;sqlite3_bind_text(s,1,hash,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;sqlite3_finalize(s);return ok;}
static const char *operation_name(WenaDomainOperation operation){if(operation==WENA_DOMAIN_CREATE_CARD)return "create-card";if(operation==WENA_DOMAIN_EDIT_CARD_TITLE)return "edit-card-title";if(operation==WENA_DOMAIN_ARCHIVE_CARD)return "archive-card";if(operation==WENA_DOMAIN_EDIT_BOARD_TITLE)return "edit-board-title";if(operation==WENA_DOMAIN_EDIT_LIST_TITLE)return "edit-list-title";if(operation==WENA_DOMAIN_EDIT_SWIMLANE_TITLE)return "edit-swimlane-title";if(operation==WENA_DOMAIN_MOVE_CARD)return "move-card";if(operation==WENA_DOMAIN_MOVE_LIST)return "move-list";if(operation==WENA_DOMAIN_MOVE_SWIMLANE)return "move-swimlane";if(operation==WENA_DOMAIN_CREATE_LIST)return "create-list";if(operation==WENA_DOMAIN_CREATE_SWIMLANE)return "create-swimlane";if(operation==WENA_DOMAIN_RESTORE_CARD)return "restore-card";return NULL;}
void wena_sqlite_persistence_init(WenaSqlitePersistence *s,sqlite3 *db){if(s){memset(s,0,sizeof(*s));s->database=db;}}
int wena_sqlite_persistence_apply(void *context,const WenaDomainCommand *c,WenaRegionResponse *r){WenaSqlitePersistence *s=(WenaSqlitePersistence *)context;sqlite3 *db;char board[65],id[65],title[129],expected[32],wire[WENA_REGION_RESPONSE_MAX_BYTES];const char*op;size_t wire_len;unsigned long version;double created_position=0.0;if(s){s->created_card_id[0]=0;s->created_card_position=0.0;s->moved_card_position=0.0;s->created_hierarchy_id[0]=0;s->created_hierarchy_position=0.0;}if(!r)return 0;memset(r,0,sizeof(*r));if(!s||!(db=s->database)||!c||!bounded_string(c->route,sizeof(c->route))||!bounded_string(c->user_id,sizeof(c->user_id))||c->request_version==0||c->request_version>(unsigned long)LONG_MAX||c->form_body_length>=sizeof(c->form_body)||!board_id(c->route,board)||(op=operation_name(c->operation))==NULL)return 0;if(sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK)return 0;if(!scalar(db,"SELECT count(*) FROM actors WHERE id=?1",c->user_id,NULL,NULL,0)||scalar(db,"SELECT count(*) FROM idempotency_keys WHERE actor_id=?1 AND route=?2 AND operation=?3 AND request_version=?4",c->user_id,c->route,op,c->request_version))goto bad;
if(c->operation==WENA_DOMAIN_CREATE_CARD){if(!value(c,"title",title,sizeof(title))||!create_card(db,c,board,title,id,&created_position))goto bad;version=1;
}else if(c->operation==WENA_DOMAIN_CREATE_LIST||c->operation==WENA_DOMAIN_CREATE_SWIMLANE){if(!value(c,"title",title,sizeof(title))||!create_hierarchy(db,c,board,title,id,&created_position))goto bad;version=1;
}else if(c->operation==WENA_DOMAIN_EDIT_BOARD_TITLE){if(!value(c,"title",title,sizeof(title))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&version)||!run(db,"UPDATE boards SET title=?1,version=version+1 WHERE id=?2 AND id=?3 AND version=?4",title,board,board,version))goto bad;version++;
}else if(c->operation==WENA_DOMAIN_EDIT_LIST_TITLE){if(!value(c,"listId",id,sizeof(id))||!value(c,"title",title,sizeof(title))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&version)||!run(db,"UPDATE lists SET title=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4",title,id,board,version))goto bad;version++;
}else if(c->operation==WENA_DOMAIN_EDIT_SWIMLANE_TITLE){if(!value(c,"swimlaneId",id,sizeof(id))||!value(c,"title",title,sizeof(title))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&version)||!run(db,"UPDATE swimlanes SET title=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4",title,id,board,version))goto bad;version++;
}else if(c->operation==WENA_DOMAIN_MOVE_CARD){char list[65],lane[65];if(!value(c,"cardId",id,sizeof(id))||!value(c,"targetListId",list,sizeof(list))||!value(c,"targetSwimlaneId",lane,sizeof(lane))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&version))goto bad;if(!scalar(db,"SELECT count(*) FROM lists WHERE id=?1 AND board_id=?2",list,board,NULL,0)||!scalar(db,"SELECT count(*) FROM swimlanes WHERE id=?1 AND board_id=?2",lane,board,NULL,0)||!move_card(db,list,lane,id,board,version)||!card_position(db,id,&created_position))goto bad;strcpy(title,"Moved");version++;
}else if(c->operation==WENA_DOMAIN_MOVE_LIST){char target[32];if(!value(c,"listId",id,sizeof(id))||!value(c,"targetPosition",target,sizeof(target))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(target,1,(unsigned long)LONG_MAX,&version))goto bad;{unsigned long expected_version;if(!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&expected_version)||!move_list(db,id,board,version,expected_version))goto bad;version=expected_version+1;}strcpy(title,"Moved list");
}else if(c->operation==WENA_DOMAIN_MOVE_SWIMLANE){char target[32];if(!value(c,"swimlaneId",id,sizeof(id))||!value(c,"targetPosition",target,sizeof(target))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(target,1,(unsigned long)LONG_MAX,&version))goto bad;{unsigned long expected_version;if(!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&expected_version)||!move_swimlane(db,id,board,version,expected_version))goto bad;version=expected_version+1;}strcpy(title,"Moved swimlane");
}else{if(!value(c,"cardId",id,sizeof(id))||!value(c,"expectedVersion",expected,sizeof(expected))||!decimal(expected,0,(unsigned long)LONG_MAX-1UL,&version))goto bad;if(c->operation==WENA_DOMAIN_EDIT_CARD_TITLE){if(!value(c,"title",title,sizeof(title))||!run(db,"UPDATE cards SET title=?1,version=version+1 WHERE id=?2 AND board_id=?3 AND version=?4 AND archived=0",title,id,board,version))goto bad;}else if(c->operation==WENA_DOMAIN_ARCHIVE_CARD){strcpy(title,"Archived");if(!run(db,"UPDATE cards SET archived=1,version=version+1 WHERE id=?1 AND board_id=?2 AND version=?4 AND archived=0",id,board,NULL,version))goto bad;}else if(c->operation==WENA_DOMAIN_RESTORE_CARD){strcpy(title,"Restored");if(!run(db,"UPDATE cards SET archived=0,version=version+1 WHERE id=?1 AND board_id=?2 AND version=?4 AND archived=1",id,board,NULL,version))goto bad;}else goto bad;version++;}
r->request_version=c->request_version;r->region_count=1;strcpy(r->regions[0].name,"board");r->regions[0].version=version;strcpy(r->regions[0].content,title);r->regions[0].content_length=strlen(title);if(!wena_region_response_encode(r,wire,sizeof(wire),&wire_len))goto bad;if(!run(db,"INSERT INTO idempotency_keys(actor_id,route,operation,request_version,response_checksum,committed_at) VALUES(?1,?2,?3,?4,'pending',strftime('%s','now'))",c->user_id,c->route,op,c->request_version)||!checksum(db,wire,wire_len))goto bad;if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto bad;if(c->operation==WENA_DOMAIN_CREATE_CARD){strcpy(s->created_card_id,id);s->created_card_position=created_position;}else if(c->operation==WENA_DOMAIN_MOVE_CARD){s->moved_card_position=created_position;}else if(c->operation==WENA_DOMAIN_CREATE_LIST||c->operation==WENA_DOMAIN_CREATE_SWIMLANE){strcpy(s->created_hierarchy_id,id);s->created_hierarchy_position=created_position;}return 1;bad:sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);memset(r,0,sizeof(*r));return 0;}
