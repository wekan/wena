#include "board_search.h"
#include <stdlib.h>
#include <string.h>

/* The term as a LIKE pattern: %term%, its own % _ and \ escaped. */
static char *pattern(const char *term)
{
    size_t length = strlen(term), i, used = 0;
    char *out = (char *)malloc(length * 2 + 3);
    if (out == NULL) return NULL;
    out[used++] = '%';
    for (i = 0; i < length; ++i) {
        if (term[i] == '%' || term[i] == '_' || term[i] == '\\') out[used++] = '\\';
        out[used++] = term[i];
    }
    out[used++] = '%';
    out[used] = '\0';
    return out;
}

static int collect(sqlite3 *db, const char *sql, const char *board, const char *like,
                   WenaId *ids, size_t capacity, size_t *count)
{
    sqlite3_stmt *statement = NULL;
    const char *id;
    int step;
    *count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, like, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        id = (const char *)sqlite3_column_text(statement, 0);
        if (*count == capacity) { step = SQLITE_DONE; break; }
        if (id != NULL && wena_model_identifier_valid(id)) strcpy(ids[(*count)++], id);
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

int wena_board_search(sqlite3 *db, const char *board, const char *term,
                      WenaId *lists, size_t list_capacity, size_t *list_count,
                      WenaId *cards, size_t card_capacity, size_t *card_count)
{
    char *like, *trimmed;
    size_t start, end;
    int ok;
    if (db == NULL || board == NULL || term == NULL || lists == NULL || list_count == NULL ||
        cards == NULL || card_count == NULL) return 0;
    *list_count = *card_count = 0;
    /* WeKan trims the term; an empty one finds nothing. */
    for (start = 0; term[start] == ' ' || term[start] == '\t'; ++start) {}
    for (end = strlen(term); end > start && (term[end - 1] == ' ' || term[end - 1] == '\t'); --end) {}
    if (end == start) return 1;
    trimmed = (char *)malloc(end - start + 1);
    if (trimmed == NULL) return 0;
    memcpy(trimmed, term + start, end - start);
    trimmed[end - start] = '\0';
    like = pattern(trimmed);
    free(trimmed);
    if (like == NULL) return 0;
    ok = collect(db, "SELECT id FROM lists WHERE board_id = ?1 AND title LIKE ?2 ESCAPE '\\' "
                     "ORDER BY rowid DESC", board, like, lists, list_capacity, list_count) &&
         collect(db, "SELECT c.id FROM cards c LEFT JOIN card_descriptions d ON d.card_id = c.id "
                     "WHERE c.board_id = ?1 AND (c.title LIKE ?2 ESCAPE '\\' OR d.description LIKE ?2 ESCAPE '\\') "
                     "ORDER BY c.rowid DESC", board, like, cards, card_capacity, card_count);
    free(like);
    return ok;
}
