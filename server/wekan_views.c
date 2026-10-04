#include "wekan_views.h"
#include "ferretdb_sqlite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHEMA "fdb"

/* The table of a collection, quoted; 0 when the file has no such collection. */
static int table(sqlite3 *db, const char *collection, char out[WENA_FERRETDB_TABLE_CAPACITY + 16])
{
    char name[WENA_FERRETDB_TABLE_CAPACITY];
    if (!wena_ferretdb_collection(db, SCHEMA, collection, 0, name, sizeof(name))) return 0;
    sprintf(out, SCHEMA ".\"%s\"", name);
    return 1;
}

static void copy(char *out, size_t capacity, const unsigned char *text)
{
    size_t length = text != NULL ? strlen((const char *)text) : 0;
    if (capacity == 0) return;
    if (length >= capacity) {
        length = capacity - 1;
        while (length > 0 && (text[length] & 0xC0u) == 0x80u) --length;
    }
    if (length) memcpy(out, text, length);
    out[length] = '\0';
}

static WenaViewTime time_of(sqlite3_stmt *statement, int column)
{
    WenaViewTime time;
    int type = sqlite3_column_type(statement, column);
    time.set = type == SQLITE_INTEGER || type == SQLITE_FLOAT;
    time.ms = time.set ? sqlite3_column_double(statement, column) : 0.0;
    return time;
}

/* A growing array of `size`-byte records. */
static void *grow(void *items, size_t *capacity, size_t count, size_t size)
{
    void *bigger;
    if (count < *capacity) return items;
    *capacity = *capacity ? *capacity * 2 : 64;
    bigger = realloc(items, *capacity * size);
    if (bigger == NULL) free(items);
    return bigger;
}

static size_t split_ids(char out[][WENA_VIEW_ID], size_t capacity, const unsigned char *text)
{
    size_t count = 0;
    const char *at = (const char *)text;
    while (at != NULL && *at && count < capacity) {
        const char *end = strchr(at, ',');
        size_t length = end != NULL ? (size_t)(end - at) : strlen(at);
        if (length > 0 && length < WENA_VIEW_ID) { memcpy(out[count], at, length); out[count][length] = '\0'; ++count; }
        at = end != NULL ? end + 1 : NULL;
    }
    return count;
}

static size_t split_dependencies(WenaViewDependency *out, size_t capacity, const unsigned char *text)
{
    size_t count = 0;
    const char *at = (const char *)text;
    while (at != NULL && *at && count < capacity) {
        const char *end = strchr(at, ';'), *colon = strchr(at, ':');
        size_t length = colon != NULL && (end == NULL || colon < end) ? (size_t)(colon - at) : 0;
        if (length > 0 && length < WENA_VIEW_ID) {
            memcpy(out[count].card_id, at, length);
            out[count].card_id[length] = '\0';
            out[count].blocks = colon[1] == 'b';
            ++count;
        }
        at = end != NULL ? end + 1 : NULL;
    }
    return count;
}

/* The card fields, of a document `x`, as parts: C89 keeps a literal short. */
static const char *const card_columns[] = {
    "x->>'_id', x->>'title', x->>'boardId', x->>'listId', x->>'swimlaneId', coalesce(x->>'sort', 0), ",
    "CASE WHEN coalesce(x->>'archived', 0) THEN 1 ELSE 0 END, x->>'createdAt', x->>'archivedAt', x->>'startAt', ",
    "x->>'endAt', x->>'dueAt', x->>'receivedAt', x->>'deletedAt', x->>'modifiedAt', x->>'spentTime', ",
    "CASE WHEN coalesce(x->>'isOvertime', 0) THEN 1 ELSE 0 END, ",
    "(SELECT group_concat(value, ',') FROM json_each(x, '$.assignees')), ",
    "(SELECT group_concat(value, ',') FROM json_each(x, '$.members')), ",
    "(SELECT group_concat(value, ',') FROM json_each(x, '$.labelIds')), ",
    "coalesce(json_array_length(x, '$.vote.positive'), 0), coalesce(json_array_length(x, '$.vote.negative'), 0), ",
    "x->>'$.poker.estimation', ",
    "(SELECT group_concat((value->>'cardId') || ':' || CASE value->>'type' WHEN 'blocks' THEN 'b' ELSE 'i' END, ';') ",
    "FROM json_each(x, '$.cardDependencies') WHERE value->>'type' IN ('blocks', 'is-blocked-by')), ",
    "substr(coalesce(x->>'description', ''), 1, 255)",
    NULL};

/* Appends parts to `sql`; 0 when they do not fit. */
static int append(char *sql, size_t capacity, const char *const *parts)
{
    size_t used = strlen(sql);
    for (; *parts != NULL; ++parts) {
        size_t length = strlen(*parts);
        if (used + length + 1 > capacity) return 0;
        memcpy(sql + used, *parts, length + 1);
        used += length;
    }
    return 1;
}

static int append1(char *sql, size_t capacity, const char *part)
{
    const char *parts[2];
    parts[0] = part;
    parts[1] = NULL;
    return append(sql, capacity, parts);
}

static int read_card(sqlite3_stmt *s, WenaViewCard *card)
{
    int type;
    memset(card, 0, sizeof(*card));
    if (sqlite3_column_text(s, 0) == NULL) return 0;
    copy(card->id, sizeof(card->id), sqlite3_column_text(s, 0));
    copy(card->title, sizeof(card->title), sqlite3_column_text(s, 1));
    copy(card->board_id, sizeof(card->board_id), sqlite3_column_text(s, 2));
    copy(card->list_id, sizeof(card->list_id), sqlite3_column_text(s, 3));
    copy(card->swimlane_id, sizeof(card->swimlane_id), sqlite3_column_text(s, 4));
    card->sort = sqlite3_column_double(s, 5);
    card->archived = sqlite3_column_int(s, 6);
    card->created_at = time_of(s, 7);
    card->archived_at = time_of(s, 8);
    card->start_at = time_of(s, 9);
    card->end_at = time_of(s, 10);
    card->due_at = time_of(s, 11);
    card->received_at = time_of(s, 12);
    card->deleted_at = time_of(s, 13);
    card->modified_at = time_of(s, 14);
    type = sqlite3_column_type(s, 15);
    card->has_spent_time = type == SQLITE_INTEGER || type == SQLITE_FLOAT;
    card->spent_time = card->has_spent_time ? sqlite3_column_double(s, 15) : 0.0;
    card->is_overtime = sqlite3_column_int(s, 16);
    card->assignee_count = split_ids(card->assignees, WENA_VIEW_PEOPLE, sqlite3_column_text(s, 17));
    card->member_count = split_ids(card->members, WENA_VIEW_PEOPLE, sqlite3_column_text(s, 18));
    card->label_count = split_ids(card->label_ids, WENA_VIEW_LABELS, sqlite3_column_text(s, 19));
    card->votes_positive = sqlite3_column_int(s, 20);
    card->votes_negative = sqlite3_column_int(s, 21);
    type = sqlite3_column_type(s, 22);
    /* A Planning Poker estimation is a number, or a string of one. */
    if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) { card->has_poker = 1; card->poker = sqlite3_column_double(s, 22); }
    else if (type == SQLITE_TEXT) {
        char *end;
        const char *text = (const char *)sqlite3_column_text(s, 22);
        double value = strtod(text, &end);
        if (end != text && *end == '\0') { card->has_poker = 1; card->poker = value; }
    }
    card->dependency_count = split_dependencies(card->dependencies, WENA_VIEW_DEPENDENCIES, sqlite3_column_text(s, 23));
    copy(card->description, sizeof(card->description), sqlite3_column_text(s, 24));
    return 1;
}

static int load_cards(sqlite3 *db, const char *cards, const char *where, const char *bind, WenaViewData *data)
{
    char sql[4096];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0;
    int step;
    strcpy(sql, "SELECT ");
    if (!append(sql, sizeof(sql), card_columns) || !append1(sql, sizeof(sql), " FROM (SELECT _ferretdb_sjson AS x FROM ") ||
        !append1(sql, sizeof(sql), cards) || !append1(sql, sizeof(sql), ") WHERE ") || !append1(sql, sizeof(sql), where) ||
        !append1(sql, sizeof(sql), " ORDER BY coalesce(x->>'sort', 0), x->>'_id'")) return 0;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    if (bind != NULL) sqlite3_bind_text(statement, 1, bind, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        data->cards = (WenaViewCard *)grow(data->cards, &capacity, data->card_count, sizeof(WenaViewCard));
        if (data->cards == NULL) { sqlite3_finalize(statement); return 0; }
        if (read_card(statement, &data->cards[data->card_count])) ++data->card_count;
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

static int load_board(sqlite3 *db, const char *board, WenaViewData *data)
{
    char boards[WENA_FERRETDB_TABLE_CAPACITY + 16], sql[1024];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0;
    int step;
    if (!table(db, "boards", boards)) return 1;
    sprintf(sql, "SELECT x->>'title', l.value->>'_id', coalesce(l.value->>'name', ''), coalesce(l.value->>'color', '') "
                 "FROM (SELECT _ferretdb_sjson AS x FROM %s WHERE _ferretdb_sjson->'_id' = json_quote(?1)) "
                 "LEFT JOIN json_each(x, '$.labels') l", boards);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        copy(data->board_title, sizeof(data->board_title), sqlite3_column_text(statement, 0));
        if (sqlite3_column_text(statement, 1) == NULL) continue;
        data->labels = (WenaViewLabel *)grow(data->labels, &capacity, data->label_count, sizeof(WenaViewLabel));
        if (data->labels == NULL) break;
        copy(data->labels[data->label_count].id, WENA_VIEW_ID, sqlite3_column_text(statement, 1));
        copy(data->labels[data->label_count].name, WENA_VIEW_TITLE, sqlite3_column_text(statement, 2));
        copy(data->labels[data->label_count].color, 33, sqlite3_column_text(statement, 3));
        ++data->label_count;
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

static int load_lists(sqlite3 *db, const char *board, WenaViewData *data)
{
    char lists[WENA_FERRETDB_TABLE_CAPACITY + 16], sql[1024];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0;
    int step;
    if (!table(db, "lists", lists)) return 1;
    sprintf(sql, "SELECT x->>'_id', coalesce(x->>'title', ''), coalesce(x->>'sort', 0), "
                 "CASE WHEN coalesce(x->>'$.wipLimit.enabled', 0) THEN 1 ELSE 0 END, coalesce(x->>'$.wipLimit.value', 0) "
                 "FROM (SELECT _ferretdb_sjson AS x FROM %s) WHERE x->>'boardId' = ?1 AND NOT coalesce(x->>'archived', 0) "
                 "AND coalesce(x->>'type', 'list') = 'list' ORDER BY coalesce(x->>'sort', 0), x->>'_id'", lists);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        WenaViewList *list;
        data->lists = (WenaViewList *)grow(data->lists, &capacity, data->list_count, sizeof(WenaViewList));
        if (data->lists == NULL) { sqlite3_finalize(statement); return 0; }
        list = &data->lists[data->list_count++];
        memset(list, 0, sizeof(*list));
        copy(list->id, sizeof(list->id), sqlite3_column_text(statement, 0));
        copy(list->title, sizeof(list->title), sqlite3_column_text(statement, 1));
        list->sort = sqlite3_column_double(statement, 2);
        list->wip_enabled = sqlite3_column_int(statement, 3);
        list->wip_value = sqlite3_column_int(statement, 4);
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

static int load_swimlanes(sqlite3 *db, const char *board, WenaViewData *data)
{
    char swimlanes[WENA_FERRETDB_TABLE_CAPACITY + 16], sql[1024];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0;
    int step;
    if (!table(db, "swimlanes", swimlanes)) return 1;
    sprintf(sql, "SELECT x->>'_id', coalesce(x->>'title', ''), coalesce(x->>'sort', 0) FROM (SELECT _ferretdb_sjson AS x "
                 "FROM %s) WHERE x->>'boardId' = ?1 AND NOT coalesce(x->>'archived', 0) "
                 "ORDER BY coalesce(x->>'sort', 0), x->>'_id'", swimlanes);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        WenaViewSwimlane *lane;
        data->swimlanes = (WenaViewSwimlane *)grow(data->swimlanes, &capacity, data->swimlane_count,
                                                   sizeof(WenaViewSwimlane));
        if (data->swimlanes == NULL) { sqlite3_finalize(statement); return 0; }
        lane = &data->swimlanes[data->swimlane_count++];
        copy(lane->id, sizeof(lane->id), sqlite3_column_text(statement, 0));
        copy(lane->title, sizeof(lane->title), sqlite3_column_text(statement, 1));
        lane->sort = sqlite3_column_double(statement, 2);
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

static int load_users(sqlite3 *db, WenaViewData *data)
{
    char users[WENA_FERRETDB_TABLE_CAPACITY + 16], sql[512];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0;
    int step;
    if (!table(db, "users", users)) return 1;
    sprintf(sql, "SELECT x->>'_id', coalesce(nullif(trim(x->>'$.profile.fullname'), ''), x->>'username', '') "
                 "FROM (SELECT _ferretdb_sjson AS x FROM %s)", users);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (sqlite3_column_text(statement, 0) == NULL) continue;
        data->users = (WenaViewUser *)grow(data->users, &capacity, data->user_count, sizeof(WenaViewUser));
        if (data->users == NULL) { sqlite3_finalize(statement); return 0; }
        copy(data->users[data->user_count].id, WENA_VIEW_ID, sqlite3_column_text(statement, 0));
        copy(data->users[data->user_count].name, WENA_VIEW_TITLE, sqlite3_column_text(statement, 1));
        ++data->user_count;
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

static int load_activities(sqlite3 *db, const char *board, WenaViewData *data)
{
    char activities[WENA_FERRETDB_TABLE_CAPACITY + 16], sql[1024];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0;
    int step;
    if (!table(db, "activities", activities)) return 1;
    sprintf(sql, "SELECT coalesce(x->>'activityType', ''), coalesce(x->>'cardId', ''), coalesce(x->>'listId', ''), "
                 "coalesce(x->>'oldListId', ''), coalesce(x->>'userId', ''), x->>'createdAt' "
                 "FROM (SELECT _ferretdb_sjson AS x FROM %s) WHERE x->>'boardId' = ?1 "
                 "ORDER BY x->>'createdAt', x->>'_id'", activities);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        WenaViewActivity *a;
        data->activities = (WenaViewActivity *)grow(data->activities, &capacity, data->activity_count,
                                                    sizeof(WenaViewActivity));
        if (data->activities == NULL) { sqlite3_finalize(statement); return 0; }
        a = &data->activities[data->activity_count++];
        copy(a->type, sizeof(a->type), sqlite3_column_text(statement, 0));
        copy(a->card_id, sizeof(a->card_id), sqlite3_column_text(statement, 1));
        copy(a->list_id, sizeof(a->list_id), sqlite3_column_text(statement, 2));
        copy(a->old_list_id, sizeof(a->old_list_id), sqlite3_column_text(statement, 3));
        copy(a->user_id, sizeof(a->user_id), sqlite3_column_text(statement, 4));
        a->at = time_of(statement, 5);
    }
    sqlite3_finalize(statement);
    return step == SQLITE_DONE;
}

/* A dependency list of changeHistory content, as "id:b;id:i". */
#define DEPENDENCIES(path) \
    "(SELECT group_concat((value->>'cardId') || ':' || CASE value->>'type' WHEN 'blocks' THEN 'b' ELSE 'i' END, ';') " \
    "FROM json_each(x, '" path "') WHERE value->>'type' IN ('blocks', 'is-blocked-by'))"

static size_t add_dependencies(WenaViewData *data, size_t *capacity, const unsigned char *text, size_t *count)
{
    WenaViewDependency parsed[WENA_VIEW_DEPENDENCIES];
    size_t start = data->change_dependency_count, i;
    *count = split_dependencies(parsed, WENA_VIEW_DEPENDENCIES, text);
    for (i = 0; i < *count; ++i) {
        data->change_dependencies = (WenaViewDependency *)grow(data->change_dependencies, capacity,
                                                               data->change_dependency_count, sizeof(WenaViewDependency));
        if (data->change_dependencies == NULL) { *count = 0; return 0; }
        data->change_dependencies[data->change_dependency_count++] = parsed[i];
    }
    return start;
}

static int load_changes(sqlite3 *db, const char *board, WenaViewData *data)
{
    char history[WENA_FERRETDB_TABLE_CAPACITY + 16], sql[4096];
    sqlite3_stmt *statement = NULL;
    size_t capacity = 0, dependency_capacity = 0;
    int step;
    if (!table(db, "changeHistory", history)) return 1;
    /* The rows the flow charts replay (boardChartData.js): moves, removals and
     * restores with their snapshot, and the fields that end or block a card. */
    {
        static const char *const head[] = {
            "SELECT x->>'group', coalesce(x->>'entityId', ''), coalesce(x->>'userId', ''), ",
            "coalesce(x->>'$.newContent.field', x->>'$.previousContent.field', ''), ",
            "coalesce(x->>'$.previousContent.listId', ''), coalesce(x->>'$.newContent.listId', ''), ",
            "x->>'$.previousContent.value', x->>'$.newContent.value', ",
            "CASE WHEN x->'$.newContent.document' IS NULL THEN 1 ELSE 0 END, ",
            DEPENDENCIES("$.previousContent.value"), ", ", DEPENDENCIES("$.newContent.value"), ", ",
            "x->>'createdAt', json_type(x, '$.previousContent.value'), json_type(x, '$.newContent.value') ",
            "FROM (SELECT _ferretdb_sjson AS x FROM ", NULL};
        static const char *const tail[] = {
            ") WHERE x->>'boardId' = ?1 AND x->>'entityType' = 'card' ",
            "AND (x->>'group' = 'position' OR (x->>'group' = 'lifecycle' AND x->'$.previousContent.document' ",
            "IS NOT NULL) OR coalesce(x->>'$.newContent.field', x->>'$.previousContent.field') IN ",
            "('cardDependencies', 'endAt', 'archivedAt', 'archived', 'deletedAt', 'spentTime')) ",
            "ORDER BY x->>'createdAt', x->>'_id'", NULL};
        sql[0] = '\0';
        if (!append(sql, sizeof(sql), head) || !append1(sql, sizeof(sql), history) || !append(sql, sizeof(sql), tail))
            return 0;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        WenaViewChange *c;
        const char *group = (const char *)sqlite3_column_text(statement, 0);
        const unsigned char *old_type = sqlite3_column_text(statement, 12), *new_type = sqlite3_column_text(statement, 13);
        data->changes = (WenaViewChange *)grow(data->changes, &capacity, data->change_count, sizeof(WenaViewChange));
        if (data->changes == NULL) { sqlite3_finalize(statement); return 0; }
        c = &data->changes[data->change_count++];
        memset(c, 0, sizeof(*c));
        c->kind = group != NULL && !strcmp(group, "position") ? WENA_VIEW_CHANGE_POSITION :
                  group != NULL && !strcmp(group, "lifecycle") ? WENA_VIEW_CHANGE_LIFECYCLE : WENA_VIEW_CHANGE_FIELD;
        copy(c->card_id, sizeof(c->card_id), sqlite3_column_text(statement, 1));
        copy(c->user_id, sizeof(c->user_id), sqlite3_column_text(statement, 2));
        copy(c->field, sizeof(c->field), sqlite3_column_text(statement, 3));
        copy(c->old_list_id, sizeof(c->old_list_id), sqlite3_column_text(statement, 4));
        copy(c->new_list_id, sizeof(c->new_list_id), sqlite3_column_text(statement, 5));
        /* A true is 1, a false 0; null or no value is "not set". */
        c->has_old = old_type != NULL && strcmp((const char *)old_type, "null") && strcmp((const char *)old_type, "false");
        c->has_new = new_type != NULL && strcmp((const char *)new_type, "null") && strcmp((const char *)new_type, "false");
        c->old_value = c->has_old ? sqlite3_column_double(statement, 6) : 0.0;
        c->new_value = c->has_new ? sqlite3_column_double(statement, 7) : 0.0;
        if (old_type != NULL && !strcmp((const char *)old_type, "true")) c->old_value = 1.0;
        if (new_type != NULL && !strcmp((const char *)new_type, "true")) c->new_value = 1.0;
        c->removed = sqlite3_column_int(statement, 8);
        c->restored = !c->removed;
        if (!strcmp(c->field, "cardDependencies")) {
            c->old_dependency_start = add_dependencies(data, &dependency_capacity, sqlite3_column_text(statement, 9),
                                                       &c->old_dependency_count);
            c->new_dependency_start = add_dependencies(data, &dependency_capacity, sqlite3_column_text(statement, 10),
                                                       &c->new_dependency_count);
        }
        c->at = time_of(statement, 11);
    }
    sqlite3_finalize(statement);
    if (step != SQLITE_DONE) return 0;
    /* withRemovedCards: a removed card's recorded snapshot, newest first, when
     * the card is not on the board any more. */
    {
        size_t j;
        static const char *const head[] = {
            ", deleted FROM (SELECT y -> '$.previousContent.document' AS x, ",
            "y->>'createdAt' AS deleted, y->>'_id' AS row FROM (SELECT _ferretdb_sjson AS y FROM ", NULL};
        static const char *const tail[] = {
            ") WHERE y->>'group' = 'lifecycle' AND y->'$.newContent' IS NULL AND y->>'entityType' = 'card' ",
            "AND y->>'$.previousContent.document.boardId' = ?1 ",
            "AND y->>'$.previousContent.document._id' = y->>'entityId') ORDER BY deleted DESC, row", NULL};
        strcpy(sql, "SELECT ");
        if (!append(sql, sizeof(sql), card_columns) || !append(sql, sizeof(sql), head) ||
            !append1(sql, sizeof(sql), history) || !append(sql, sizeof(sql), tail)) return 0;
        if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
        sqlite3_bind_text(statement, 1, board, -1, SQLITE_TRANSIENT);
        capacity = data->card_count;
        while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
            WenaViewCard card;
            int known = 0;
            if (!read_card(statement, &card)) continue;
            for (j = 0; j < data->card_count; ++j) if (!strcmp(data->cards[j].id, card.id)) { known = 1; break; }
            if (known) continue;
            card.deleted_at = time_of(statement, 25);
            data->cards = (WenaViewCard *)grow(data->cards, &capacity, data->card_count, sizeof(WenaViewCard));
            if (data->cards == NULL) { sqlite3_finalize(statement); return 0; }
            data->cards[data->card_count++] = card;
        }
        sqlite3_finalize(statement);
    }
    return step == SQLITE_DONE;
}

int wena_wekan_views_load(sqlite3 *db, const char *board, WenaViewData *data)
{
    char cards[WENA_FERRETDB_TABLE_CAPACITY + 16];
    if (db == NULL || board == NULL || data == NULL || strlen(board) >= sizeof(data->board_id)) return 0;
    memset(data, 0, sizeof(*data));
    strcpy(data->board_id, board);
    if ((table(db, "cards", cards) && !load_cards(db, cards, "x->>'boardId' = ?1", board, data)) ||
        !load_board(db, board, data) || !load_lists(db, board, data) || !load_swimlanes(db, board, data) ||
        !load_users(db, data) || !load_activities(db, board, data) || !load_changes(db, board, data)) {
        wena_view_data_free(data);
        return 0;
    }
    return 1;
}

int wena_wekan_views_load_all(sqlite3 *db, const char *actor, WenaViewData *data)
{
    char cards[WENA_FERRETDB_TABLE_CAPACITY + 16], boards[WENA_FERRETDB_TABLE_CAPACITY + 16], where[1024];
    size_t i;
    if (db == NULL || actor == NULL || data == NULL) return 0;
    memset(data, 0, sizeof(*data));
    if (!table(db, "cards", cards) || !table(db, "boards", boards)) return 1;
    sprintf(where, "x->>'boardId' IN (SELECT b._ferretdb_sjson->>'_id' FROM %s b WHERE NOT "
                   "coalesce(b._ferretdb_sjson->>'archived', 0) AND EXISTS (SELECT 1 FROM json_each(b._ferretdb_sjson, "
                   "'$.members') m WHERE m.value->>'userId' = ?1 AND coalesce(m.value->>'isActive', 1))) "
                   "AND NOT coalesce(x->>'archived', 0)", boards);
    if (!load_cards(db, cards, where, actor, data) || !load_users(db, data)) { wena_view_data_free(data); return 0; }
    /* Each card's board, by title, for the calendar to name. */
    for (i = 0; i < data->card_count; ++i) {
        char sql[512];
        sqlite3_stmt *statement = NULL;
        sprintf(sql, "SELECT _ferretdb_sjson->>'title' FROM %s WHERE _ferretdb_sjson->'_id' = json_quote(?1)", boards);
        if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) continue;
        sqlite3_bind_text(statement, 1, data->cards[i].board_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(statement) == SQLITE_ROW) {
            char title[WENA_VIEW_TITLE];
            copy(title, sizeof(title), sqlite3_column_text(statement, 0));
            copy(data->cards[i].board_title, sizeof(data->cards[i].board_title), (const unsigned char *)title);
        }
        sqlite3_finalize(statement);
    }
    return 1;
}
