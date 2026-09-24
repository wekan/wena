#include "summary.h"
#include "../checklist_store.h"
#include <stdlib.h>
#include <string.h>

/* These queries stream selected-card rows through existing card/parent indexes.
 * They do not filter the child's claimed board, which would hide corruption. */
static const char checklist_query[] =
    "SELECT id,board_id,card_id,title,position,version,hide_checked_items,hide_all_items,"
    "show_on_minicard,created_at,updated_at FROM checklists WHERE card_id IN "
    "(SELECT id FROM cards WHERE board_id=?1) ORDER BY card_id,position,id";
static const char item_query[] =
    "SELECT i.id,i.board_id,i.card_id,i.checklist_id,i.title,i.position,i.is_finished,"
    "i.version,i.created_at,i.updated_at,p.board_id,p.card_id FROM checklist_items i "
    "LEFT JOIN checklists p ON p.id=i.checklist_id WHERE i.card_id IN "
    "(SELECT id FROM cards WHERE board_id=?1) ORDER BY i.card_id,i.checklist_id,i.position,i.id";
static const char child_scope_query[] =
    "SELECT 1 FROM checklists p JOIN checklist_items i ON i.checklist_id=p.id "
    "WHERE p.card_id IN (SELECT id FROM cards WHERE board_id=?1) AND "
    "(i.board_id IS NOT p.board_id OR i.card_id IS NOT p.card_id) LIMIT 1";

WenaChecklistBoardSummary *wena_checklist_summary_create(void)
{
    return (WenaChecklistBoardSummary *)calloc(1, sizeof(WenaChecklistBoardSummary));
}
void wena_checklist_summary_free(WenaChecklistBoardSummary *summary)
{
    free(summary);
}
static size_t find_index(const WenaChecklistBoardSummary *summary, const char *id)
{
    size_t first, last, middle;
    int order;
    first = 0; last = summary->card_count;
    while (first < last) {
        middle = first + (last - first) / 2;
        order = strcmp(id, summary->cards[middle].card_id);
        if (!order) return middle;
        if (order < 0) last = middle; else first = middle + 1;
    }
    return summary->card_count;
}
static WenaChecklistCardSummary *find_card(WenaChecklistBoardSummary *summary,
    const char *id)
{
    size_t index;
    index = find_index(summary, id);
    return index < summary->card_count ? &summary->cards[index] : NULL;
}
const WenaChecklistCardSummary *wena_checklist_summary_find(
    const WenaChecklistBoardSummary *summary, const char *card_id)
{
    size_t index;
    if (!summary || !summary->enabled ||
        summary->card_count > WENA_SQLITE_BOARD_MAX_CARDS ||
        !wena_model_identifier_valid(card_id)) return NULL;
    index = find_index(summary, card_id);
    return index < summary->card_count ? &summary->cards[index] : NULL;
}
void wena_checklist_contents_free(WenaChecklistBoardContents *contents)
{
    size_t index;
    WenaChecklistContents *list, *next;
    if (!contents) return;
    for (index = 0; index < WENA_SQLITE_BOARD_MAX_CARDS; ++index) {
        list = contents->cards[index];
        while (list) {
            next = list->next; free(list->items); free(list->item_versions); free(list); list = next;
        }
    }
    free(contents);
}
const WenaChecklistContents *wena_checklist_contents_find(
    const WenaChecklistBoardContents *contents, const char *card_id)
{
    size_t index;
    if (!contents || !wena_checklist_summary_find(&contents->summary, card_id)) return NULL;
    index = find_index(&contents->summary, card_id);
    return contents->cards[index];
}
static int retain_checklist(WenaChecklistBoardContents *contents, size_t card_index,
    const WenaChecklist *checklist, unsigned long version)
{
    WenaChecklistContents **tail, *node;
    if (!contents) return 1;
    node = (WenaChecklistContents *)calloc(1, sizeof(*node));
    if (!node) return 0;
    node->checklist = *checklist; node->version = version;
    tail = &contents->cards[card_index];
    while (*tail) tail = &(*tail)->next;
    *tail = node; return 1;
}
static int retain_item(WenaChecklistBoardContents *contents, size_t card_index,
    const WenaChecklistItem *item, unsigned long version)
{
    WenaChecklistContents *list;
    WenaChecklistItem *items;
    unsigned long *versions;
    size_t capacity;
    if (!contents) return 1;
    list = contents->cards[card_index];
    while (list && strcmp(list->checklist.id, item->checklist_id)) list = list->next;
    if (!list || list->item_count >= WENA_CHECKLIST_MAX_ITEMS) return 0;
    if (list->item_count == list->item_capacity) {
        capacity = list->item_capacity ? list->item_capacity * 2 : 4;
        if (capacity > WENA_CHECKLIST_MAX_ITEMS) capacity = WENA_CHECKLIST_MAX_ITEMS;
        items = (WenaChecklistItem *)realloc(list->items, capacity * sizeof(*items));
        if (!items) return 0;
        list->items = items;
        versions = (unsigned long *)realloc(list->item_versions, capacity * sizeof(*versions));
        if (!versions) return 0;
        list->item_versions = versions; list->item_capacity = capacity;
    }
    list->item_versions[list->item_count] = version;
    list->items[list->item_count++] = *item; return 1;
}
static const char *read_text(sqlite3_stmt *statement, int column, size_t capacity)
{
    const char *text; int bytes;
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) return NULL;
    text = (const char *)sqlite3_column_text(statement, column);
    bytes = sqlite3_column_bytes(statement, column);
    if (!text || bytes < 0 || (size_t)bytes >= capacity ||
        memchr(text, 0, (size_t)bytes)) return NULL;
    return text;
}
static int read_integer(sqlite3_stmt *statement, int column,
    unsigned long maximum, unsigned long *output)
{
    sqlite3_int64 value;
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) return 0;
    value = sqlite3_column_int64(statement, column);
    if (value < 0 || value > (sqlite3_int64)maximum) return 0;
    *output = (unsigned long)value; return 1;
}
static int timestamps_valid(sqlite3_stmt *statement, int first)
{
    return sqlite3_column_type(statement, first) == SQLITE_INTEGER &&
        sqlite3_column_type(statement, first + 1) == SQLITE_INTEGER &&
        sqlite3_column_int64(statement, first) >= 0 &&
        sqlite3_column_int64(statement, first + 1) >= sqlite3_column_int64(statement, first);
}
static int prepare(sqlite3 *db, const char *sql, const char *board,
    sqlite3_stmt **statement)
{
    if (sqlite3_prepare_v2(db, sql, -1, statement, NULL) != SQLITE_OK) return 0;
    if (sqlite3_bind_text(*statement, 1, board, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(*statement); *statement = NULL; return 0;
    }
    return 1;
}
static int load_summary(sqlite3 *database, const char *actor_id,
    const char *board_id, int enabled, WenaChecklistBoardSummary *output,
    WenaChecklistBoardContents *contents)
{
    WenaChecklistBoardSummary *candidate;
    WenaChecklistCardSummary *card, *previous_card;
    WenaChecklist checklist;
    WenaChecklistItem item;
    sqlite3_stmt *statement;
    const char *id, *scope, *card_id, *title, *parent;
    unsigned long version, position, flag, previous_position;
    WenaId previous_parent;
    size_t index;
    int step, success;
    if (!output || !wena_model_identifier_valid(actor_id) ||
        !wena_model_identifier_valid(board_id) || (enabled != 0 && enabled != 1)) return 0;
    if (!enabled) {
        memset(output, 0, sizeof(*output)); strcpy(output->board_id, board_id); return 1;
    }
    if (!database || !sqlite3_get_autocommit(database)) return 0;
    candidate = wena_checklist_summary_create(); if (!candidate) return 0;
    statement = NULL; success = 0; previous_card = NULL; previous_position = 0;
    previous_parent[0] = '\0';
    if (sqlite3_exec(database, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) goto done;
    if (!prepare(database, "SELECT version FROM boards WHERE id=?1 AND "
        "EXISTS(SELECT 1 FROM actors WHERE id=?2)", board_id, &statement) ||
        sqlite3_bind_text(statement, 2, actor_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW ||
        !read_integer(statement, 0, WENA_VERSION_READ_MAX, &candidate->board_version) ||
        !candidate->board_version || sqlite3_step(statement) != SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement); statement = NULL;
    if (!prepare(database, "SELECT id,version,archived FROM cards WHERE board_id=?1 ORDER BY id",
        board_id, &statement)) goto rollback;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (candidate->card_count >= WENA_SQLITE_BOARD_MAX_CARDS) goto rollback;
        id = read_text(statement, 0, WENA_ID_CAPACITY);
        if (!wena_model_identifier_valid(id) ||
            !read_integer(statement, 1, WENA_VERSION_READ_MAX, &version) || !version ||
            !read_integer(statement, 2, 1UL, &flag)) goto rollback;
        card = &candidate->cards[candidate->card_count];
        if (candidate->card_count && strcmp(candidate->cards[candidate->card_count - 1].card_id, id) >= 0) goto rollback;
        strcpy(card->card_id, id); card->card_version = version; card->archived = (int)flag;
        ++candidate->card_count;
    }
    if (step != SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement); statement = NULL;
    if (!prepare(database, checklist_query, board_id, &statement)) goto rollback;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        id = read_text(statement, 0, WENA_ID_CAPACITY);
        scope = read_text(statement, 1, WENA_ID_CAPACITY);
        card_id = read_text(statement, 2, WENA_ID_CAPACITY);
        title = read_text(statement, 3, WENA_CHECKLIST_TITLE_CAPACITY);
        if (!scope || strcmp(scope, board_id) || !wena_model_identifier_valid(card_id)) goto rollback;
        card = find_card(candidate, card_id);
        if (!card || card->checklist_count >= WENA_CARD_CHECKLIST_CAPACITY ||
            !read_integer(statement, 4, WENA_CHECKLIST_POSITION_MAX, &position) ||
            !read_integer(statement, 5, WENA_VERSION_READ_MAX, &version) || !version ||
            !timestamps_valid(statement, 9) ||
            !wena_checklist_init(&checklist, id, scope, card_id, title, position)) goto rollback;
        if (!read_integer(statement, 6, 1UL, &flag)) goto rollback;
        checklist.hide_checked_items = (int)flag;
        if (!read_integer(statement, 7, 1UL, &flag)) goto rollback;
        checklist.hide_all_items = (int)flag;
        if (sqlite3_column_type(statement, 8) != SQLITE_NULL) {
            if (!read_integer(statement, 8, 1UL, &flag)) goto rollback;
            checklist.show_on_minicard = (WenaChecklistMinicard)flag;
        }
        if (!wena_checklist_valid(&checklist) ||
            (previous_card == card && previous_position >= position)) goto rollback;
        if (!retain_checklist(contents, (size_t)(card - candidate->cards), &checklist,
            version)) goto rollback;
        previous_card = card; previous_position = position; ++card->checklist_count;
    }
    if (step != SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement); statement = NULL; previous_card = NULL;
    if (!prepare(database, child_scope_query, board_id, &statement) ||
        sqlite3_step(statement) != SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement); statement = NULL;
    if (!prepare(database, item_query, board_id, &statement)) goto rollback;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        id = read_text(statement, 0, WENA_ID_CAPACITY);
        scope = read_text(statement, 1, WENA_ID_CAPACITY);
        card_id = read_text(statement, 2, WENA_ID_CAPACITY);
        parent = read_text(statement, 3, WENA_ID_CAPACITY);
        title = read_text(statement, 4, WENA_CHECKLIST_TITLE_CAPACITY);
        if (!scope || strcmp(scope, board_id) || !wena_model_identifier_valid(card_id) ||
            !read_text(statement, 10, WENA_ID_CAPACITY) || strcmp(read_text(statement, 10, WENA_ID_CAPACITY), board_id) ||
            !read_text(statement, 11, WENA_ID_CAPACITY) || strcmp(read_text(statement, 11, WENA_ID_CAPACITY), card_id)) goto rollback;
        card = find_card(candidate, card_id);
        if (!card || !card->checklist_count || card->progress.total >= WENA_CARD_CHECKLIST_ITEM_CAPACITY ||
            !read_integer(statement, 5, WENA_CHECKLIST_POSITION_MAX, &position) ||
            !read_integer(statement, 6, 1UL, &flag) ||
            !read_integer(statement, 7, WENA_VERSION_READ_MAX, &version) || !version ||
            !timestamps_valid(statement, 8) ||
            !wena_checklist_item_init(&item, id, scope, card_id, parent, title, position, (int)flag)) goto rollback;
        if (previous_card == card && !strcmp(previous_parent, parent) && previous_position >= position) goto rollback;
        previous_card = card; strcpy(previous_parent, parent); previous_position = position;
        if (!retain_item(contents, (size_t)(card - candidate->cards), &item, version)) goto rollback;
        ++card->progress.total; card->progress.finished += (size_t)flag;
    }
    if (step != SQLITE_DONE) goto rollback;
    sqlite3_finalize(statement); statement = NULL;
    for (index = 0; index < candidate->card_count; ++index) {
        WenaChecklistProgress *progress;
        progress = &candidate->cards[index].progress;
        if (progress->total) progress->percent = (unsigned int)
            (((unsigned long)progress->finished * 100UL + (unsigned long)progress->total / 2UL) /
            (unsigned long)progress->total);
        progress->all_items_finished = progress->total != 0 && progress->finished == progress->total;
        progress->is_finished = progress->all_items_finished;
    }
    if (sqlite3_exec(database, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) goto rollback;
    strcpy(candidate->board_id, board_id); candidate->enabled = 1;
    *output = *candidate; success = 1; goto done;
rollback:
    if (statement) { sqlite3_finalize(statement); statement = NULL; }
    sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
done:
    if (statement) sqlite3_finalize(statement);
    free(candidate); return success;
}

int wena_checklist_summary_load(sqlite3 *database, const char *actor_id,
    const char *board_id, int enabled, WenaChecklistBoardSummary *output)
{
    return load_summary(database, actor_id, board_id, enabled, output, NULL);
}
int wena_checklist_contents_load(sqlite3 *database, const char *actor_id,
    const char *board_id, WenaChecklistBoardContents **output)
{
    WenaChecklistBoardContents *candidate;
    if (!output) return 0;
    candidate = (WenaChecklistBoardContents *)calloc(1, sizeof(*candidate));
    if (!candidate) return 0;
    if (!load_summary(database, actor_id, board_id, 1, &candidate->summary, candidate)) {
        wena_checklist_contents_free(candidate); return 0;
    }
    wena_checklist_contents_free(*output); *output = candidate; return 1;
}
