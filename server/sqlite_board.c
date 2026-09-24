#include "sqlite_storage.h"
#include "sqlite_board.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *text_column(sqlite3_stmt *statement, int column, int is_id)
{
    const unsigned char *text;
    int length;
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) return NULL;
    text = sqlite3_column_text(statement, column);
    length = sqlite3_column_bytes(statement, column);
    if (!text || length <= 0 || strlen((const char *)text) != (size_t)length)
        return NULL;
    if (is_id ? !wena_model_identifier_valid((const char *)text) :
        !wena_model_title_valid((const char *)text, (size_t)length,
                                WENA_TITLE_CAPACITY)) return NULL;
    return (const char *)text;
}

static int position_column(sqlite3_stmt *statement, int column, double *out)
{
    sqlite3_int64 value;
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) return 0;
    value = sqlite3_column_int64(statement, column);
    if (value < 0 || (double)value > 9007199254740991.0) return 0;
    *out = (double)value;
    return 1;
}

static int version_column(sqlite3_stmt *statement, int column)
{
    return sqlite3_column_type(statement, column) == SQLITE_INTEGER &&
        sqlite3_column_int64(statement, column) > 0 &&
        sqlite3_column_int64(statement, column) <= (sqlite3_int64)WENA_VERSION_READ_MAX;
}

static int prepare(sqlite3 *db, const char *sql, const char *board,
                   sqlite3_stmt **statement)
{
    if (sqlite3_prepare_v2(db, sql, -1, statement, NULL) != SQLITE_OK) return 0;
    if (sqlite3_bind_text(*statement, 1, board, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(*statement);
        return 0;
    }
    return 1;
}

static int load_board(sqlite3 *db, const char *board, WenaSqliteBoardSnapshot *s)
{
    sqlite3_stmt *statement;
    const char *id, *title;
    int ok;
    if (!prepare(db, "SELECT id,title,version FROM boards WHERE id=?1", board,
        &statement)) return 0;
    ok = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        id = text_column(statement, 0, 1);
        title = text_column(statement, 1, 0);
        ok = id && title && !strcmp(id, board) && version_column(statement, 2) &&
            wena_board_init(&s->board, id, title, 0);
        if (ok) ok = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);
    return ok;
}

static int load_hierarchy(sqlite3 *db, const char *board,
                           WenaSqliteBoardSnapshot *s, int lists)
{
    sqlite3_stmt *statement;
    const char *id, *parent, *title;
    double position;
    int result, ok;
    const char *sql;
    sql = lists ? "SELECT id,board_id,title,position,version FROM lists WHERE board_id=?1 ORDER BY position,id" :
        "SELECT id,board_id,title,position,version FROM swimlanes WHERE board_id=?1 ORDER BY position,id";
    if (!prepare(db, sql, board, &statement)) return 0;
    ok = 1;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        id = text_column(statement, 0, 1);
        parent = text_column(statement, 1, 1);
        title = text_column(statement, 2, 0);
        if (!id || !parent || !title || strcmp(parent, board) ||
            !position_column(statement, 3, &position) ||
            !version_column(statement, 4)) { ok = 0; break; }
        if (lists) {
            if (s->list_count == WENA_SQLITE_BOARD_MAX_LISTS ||
                !wena_list_init(&s->lists[s->list_count], id, parent, "", title,
                    position, 0)) { ok = 0; break; }
            ++s->list_count;
        } else {
            if (s->swimlane_count == WENA_SQLITE_BOARD_MAX_SWIMLANES ||
                !wena_swimlane_init(&s->swimlanes[s->swimlane_count], id,
                    parent, title, position, 0)) { ok = 0; break; }
            ++s->swimlane_count;
        }
    }
    if (result != SQLITE_DONE) ok = 0;
    sqlite3_finalize(statement);
    return ok;
}

/* One bounded query per hierarchy kind; no per-item storage reads. Include
 * mismatched metadata pointing into this board so corruption cannot be hidden.
 * kind: 0 swimlane colors, 1 list colors, 2 list WIP settings,
 * 3 list archive state, 4 swimlane archive state. */
static int load_metadata(sqlite3 *db,const char *board,WenaSqliteBoardSnapshot *snapshot,int kind)
{
    sqlite3_stmt *statement;const char *id,*scope,*parent;const unsigned char *text;
    const char *table,*key,*parents,*fields;char query[640],*target;int available,result,ok,bytes,lists,found;
    sqlite3_int64 value,enabled,soft;
    size_t rows,count,i;unsigned char seen[WENA_SQLITE_BOARD_MAX_LISTS+WENA_SQLITE_BOARD_MAX_SWIMLANES];
    lists=kind!=0&&kind!=4;
    table=kind==4?"swimlane_archive_state":kind==3?"list_archive_state":kind==2?"list_wip_limits":lists?"list_colors":"swimlane_colors";key=lists?"list_id":"swimlane_id";parents=lists?"lists":"swimlanes";
    available=wena_sqlite_optional_table(db,table,kind==4?13:kind==3?10:kind==2?12:11);if(available<=0)return available==0;
    fields=kind>=3?"c.archived,c.archived_at,NULL":kind==2?"c.value,c.enabled,c.soft":"c.color,NULL,NULL";
    sprintf(query,"SELECT c.%s,c.board_id,%s,p.board_id FROM %s c LEFT JOIN %s p ON p.id=c.%s WHERE c.board_id=?1 UNION ALL SELECT c.%s,c.board_id,%s,p.board_id FROM %s c JOIN %s p ON p.id=c.%s WHERE p.board_id=?1 AND c.board_id IS NOT ?1",key,fields,table,parents,key,key,fields,table,parents,key);
    if(!prepare(db,query,board,&statement))return 0;
    count=lists?snapshot->list_count:snapshot->swimlane_count;rows=0;ok=1;memset(seen,0,sizeof(seen));
    while((result=sqlite3_step(statement))==SQLITE_ROW){
        id=text_column(statement,0,1);scope=text_column(statement,1,1);parent=text_column(statement,5,1);
        if(rows++>=count||!id||!scope||!parent||strcmp(scope,board)||strcmp(parent,board)){ok=0;break;}
        found=0;
        for(i=0;i<count;++i)if(!strcmp(lists?snapshot->lists[i].id:snapshot->swimlanes[i].id,id)){
            if(seen[i])break;
            seen[i]=1;found=1;break;}
        if(!found){ok=0;break;}
        if(kind>=3){
            if(sqlite3_column_type(statement,2)!=SQLITE_INTEGER||sqlite3_column_type(statement,3)!=SQLITE_INTEGER){ok=0;break;}
            value=sqlite3_column_int64(statement,2);
            if((value!=0&&value!=1)||sqlite3_column_int64(statement,3)<0){ok=0;break;}
            if(lists)snapshot->lists[i].archived=(int)value;else snapshot->swimlanes[i].archived=(int)value;
        }else if(kind==2){
            if(sqlite3_column_type(statement,2)!=SQLITE_INTEGER||
                sqlite3_column_type(statement,3)!=SQLITE_INTEGER||sqlite3_column_type(statement,4)!=SQLITE_INTEGER){ok=0;break;}
            value=sqlite3_column_int64(statement,2);enabled=sqlite3_column_int64(statement,3);soft=sqlite3_column_int64(statement,4);
            if(value<1||value>2147483647||enabled<0||enabled>1||soft<0||soft>1){ok=0;break;}
            snapshot->lists[i].wip_limit.value=(size_t)value;
            snapshot->lists[i].wip_limit.enabled=(int)enabled;snapshot->lists[i].wip_limit.soft=(int)soft;
        }else{
            if(sqlite3_column_type(statement,2)!=SQLITE_TEXT){ok=0;break;}
            text=sqlite3_column_text(statement,2);bytes=sqlite3_column_bytes(statement,2);
            if(!text||bytes<0||bytes>=WENA_COLOR_CAPACITY||memchr(text,0,(size_t)bytes)||!wena_color_valid((const char*)text)){ok=0;break;}
            target=lists?snapshot->lists[i].color:snapshot->swimlanes[i].color;
            memcpy(target,text,(size_t)bytes);target[bytes]=0;
        }
    }
    if(result!=SQLITE_DONE)ok=0;
    if(sqlite3_finalize(statement)!=SQLITE_OK)ok=0;
    return ok;
}

static int parents_present(const WenaSqliteBoardSnapshot *s,
                            const char *lane, const char *list)
{
    size_t i;
    int found;
    found = 0;
    for (i = 0; i < s->swimlane_count; ++i)
        if (!strcmp(s->swimlanes[i].id, lane)) found = 1;
    if (!found) return 0;
    for (i = 0; i < s->list_count; ++i)
        if (!strcmp(s->lists[i].id, list)) return 1;
    return 0;
}

static int load_cards(sqlite3 *db, const char *board, WenaSqliteBoardSnapshot *s)
{
    sqlite3_stmt *statement;
    const char *id, *parent, *lane, *list, *title;
    double position;
    int result, archived, ok;
    if (!prepare(db, "SELECT id,board_id,swimlane_id,list_id,title,position,archived,version "
        "FROM cards WHERE board_id=?1 ORDER BY position,id", board, &statement)) return 0;
    ok = 1;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        id = text_column(statement, 0, 1);
        parent = text_column(statement, 1, 1);
        lane = text_column(statement, 2, 1);
        list = text_column(statement, 3, 1);
        title = text_column(statement, 4, 0);
        archived = sqlite3_column_int(statement, 6);
        if (!id || !parent || !lane || !list || !title || strcmp(parent, board) ||
            !parents_present(s, lane, list) ||
            !position_column(statement, 5, &position) ||
            sqlite3_column_type(statement, 6) != SQLITE_INTEGER ||
            (sqlite3_column_int64(statement, 6) != 0 &&
             sqlite3_column_int64(statement, 6) != 1) ||
            !version_column(statement, 7) ||
            s->card_count == WENA_SQLITE_BOARD_MAX_CARDS ||
            !wena_card_init(&s->cards[s->card_count], id, parent, lane, list,
                title, position, archived)) { ok = 0; break; }
        ++s->card_count;
    }
    if (result != SQLITE_DONE) ok = 0;
    sqlite3_finalize(statement);
    return ok;
}

int wena_sqlite_board_load(sqlite3 *db, const char *board,
                           WenaSqliteBoardSnapshot *output)
{
    WenaSqliteBoardSnapshot *staged;
    int ok;
    if (!db || !wena_model_identifier_valid(board) || !output || !sqlite3_get_autocommit(db))
        return 0;
    staged = (WenaSqliteBoardSnapshot *)calloc(1, sizeof(*staged));
    if (!staged) return 0;
    if (sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) {
        free(staged);
        return 0;
    }
    ok = load_board(db, board, staged) && load_hierarchy(db, board, staged, 0) &&
        load_hierarchy(db, board, staged, 1) && load_metadata(db,board,staged,3) &&
        load_metadata(db,board,staged,4) && load_metadata(db,board,staged,0) &&
        load_metadata(db,board,staged,1) && load_metadata(db,board,staged,2) && load_cards(db, board, staged);
    if (ok) ok = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;
    if (ok) memcpy(output, staged, sizeof(*output));
    else (void)sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    free(staged);
    return ok;
}
