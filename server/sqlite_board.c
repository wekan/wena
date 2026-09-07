#include "sqlite_board.h"

#include <stdlib.h>
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
        sqlite3_column_int64(statement, column) > 0;
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
        load_hierarchy(db, board, staged, 1) && load_cards(db, board, staged);
    if (ok) ok = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;
    if (ok) memcpy(output, staged, sizeof(*output));
    else (void)sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    free(staged);
    return ok;
}
