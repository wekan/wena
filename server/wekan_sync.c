#include "wekan_sync.h"
#include "ferretdb_sqlite.h"
#include "wekan_defaults_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SQL functions ----------------------------------------------------------- */

/* wena_title(text, max_bytes, multiline): text Wena's models accept - valid
 * UTF-8 without control characters (tab and line breaks kept when multiline),
 * cut on a character boundary at max_bytes; an empty title becomes "-". */
static void title_function(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    const unsigned char *in = sqlite3_value_text(argv[0]);
    int maximum = sqlite3_value_int(argv[1]), multiline = argc > 2 && sqlite3_value_int(argv[2]);
    char *out;
    int length = sqlite3_value_bytes(argv[0]), at = 0, used = 0, need, i;
    unsigned long code, minimum;
    if (maximum < 1) maximum = 1;
    out = (char *)sqlite3_malloc(maximum + 2);
    if (out == NULL) { sqlite3_result_error_nomem(context); return; }
    while (in != NULL && at < length) {
        unsigned char c = in[at];
        if (c < 128) {
            need = 0; code = c; minimum = 0;
        } else if (c >= 194 && c <= 223) {
            need = 1; code = c & 31; minimum = 128;
        } else if (c >= 224 && c <= 239) {
            need = 2; code = c & 15; minimum = 2048;
        } else if (c >= 240 && c <= 244) {
            need = 3; code = c & 7; minimum = 65536;
        } else { ++at; continue; }
        if (at + need >= length + (need == 0 ? 1 : 0) && need > 0 && at + need >= length) break;
        for (i = 1; i <= need; ++i) {
            if ((in[at + i] & 192) != 128) break;
            code = (code << 6) | (in[at + i] & 63);
        }
        if (i <= need || code < minimum || code > 1114111 || (code >= 55296 && code <= 57343)) {
            ++at;
            continue;
        }
        if (used + need + 1 > maximum) break;
        if (code < 32 || code == 127 || (code >= 128 && code <= 159)) {
            out[used++] = multiline && (code == 9 || code == 10 || code == 13) ? (char)code : ' ';
        } else {
            memcpy(out + used, in + at, (size_t)need + 1);
            used += need + 1;
        }
        at += need + 1;
    }
    if (used == 0 && !multiline) out[used++] = '-';
    sqlite3_result_text(context, out, used, sqlite3_free);
}

/* The "$s" element FerretDB keeps for a JSON value: its BSON type, and for
 * an object or array the elements inside it. Numbers are int, long or double
 * as the MongoDB driver would send them; dates, which are numbers in JSON,
 * are the caller's to mark. */
typedef struct Parse {
    const char *text;
    size_t at;
    sqlite3_str *out;
} Parse;

static void space(Parse *p)
{
    while (p->text[p->at] == ' ' || p->text[p->at] == '\t' || p->text[p->at] == '\n' || p->text[p->at] == '\r')
        ++p->at;
}

/* Skips a JSON string at p->at; returns its length with the quotes, 0 when malformed. */
static size_t string_token(Parse *p)
{
    size_t start = p->at;
    if (p->text[p->at] != '"') return 0;
    for (++p->at; p->text[p->at] != '\0' && p->text[p->at] != '"'; ++p->at)
        if (p->text[p->at] == '\\' && p->text[p->at + 1] != '\0') ++p->at;
    if (p->text[p->at] != '"') return 0;
    ++p->at;
    return p->at - start;
}

static int element(Parse *p)
{
    space(p);
    switch (p->text[p->at]) {
    case '{': {
        /* Keys collected first: "$s" lists "p" then "$k". */
        size_t keys[256], lengths[256], count = 0, index;
        sqlite3_str *children = sqlite3_str_new(NULL), *saved = p->out;
        ++p->at;
        space(p);
        p->out = children;
        while (p->text[p->at] != '}') {
            if (count == 256) { sqlite3_free(sqlite3_str_finish(children)); p->out = saved; return 0; }
            keys[count] = p->at;
            if ((lengths[count] = string_token(p)) == 0) { sqlite3_free(sqlite3_str_finish(children)); p->out = saved; return 0; }
            space(p);
            if (p->text[p->at] != ':') { sqlite3_free(sqlite3_str_finish(children)); p->out = saved; return 0; }
            ++p->at;
            if (count > 0) sqlite3_str_appendchar(children, 1, ',');
            sqlite3_str_append(children, p->text + keys[count], (int)lengths[count]);
            sqlite3_str_appendchar(children, 1, ':');
            if (!element(p)) { sqlite3_free(sqlite3_str_finish(children)); p->out = saved; return 0; }
            ++count;
            space(p);
            if (p->text[p->at] == ',') { ++p->at; space(p); }
            else if (p->text[p->at] != '}') { sqlite3_free(sqlite3_str_finish(children)); p->out = saved; return 0; }
        }
        ++p->at;
        p->out = saved;
        if (count == 0) {
            sqlite3_free(sqlite3_str_finish(children));
            sqlite3_str_appendall(p->out, "{\"t\":\"object\",\"$s\":{}}");
            return 1;
        }
        sqlite3_str_appendall(p->out, "{\"t\":\"object\",\"$s\":{\"p\":{");
        sqlite3_str_appendall(p->out, sqlite3_str_value(children) ? sqlite3_str_value(children) : "");
        sqlite3_free(sqlite3_str_finish(children));
        sqlite3_str_appendall(p->out, "},\"$k\":[");
        for (index = 0; index < count; ++index) {
            if (index > 0) sqlite3_str_appendchar(p->out, 1, ',');
            sqlite3_str_append(p->out, p->text + keys[index], (int)lengths[index]);
        }
        sqlite3_str_appendall(p->out, "]}}");
        return 1;
    }
    case '[': {
        int first = 1;
        ++p->at;
        sqlite3_str_appendall(p->out, "{\"t\":\"array\",\"i\":[");
        space(p);
        while (p->text[p->at] != ']') {
            if (!first) sqlite3_str_appendchar(p->out, 1, ',');
            first = 0;
            if (!element(p)) return 0;
            space(p);
            if (p->text[p->at] == ',') ++p->at;
            else if (p->text[p->at] != ']') return 0;
            space(p);
        }
        ++p->at;
        sqlite3_str_appendall(p->out, "]}");
        return 1;
    }
    case '"':
        if (string_token(p) == 0) return 0;
        sqlite3_str_appendall(p->out, WENA_FERRET_STRING);
        return 1;
    case 't':
    case 'f':
        if (strncmp(p->text + p->at, "true", 4) == 0) p->at += 4;
        else if (strncmp(p->text + p->at, "false", 5) == 0) p->at += 5;
        else return 0;
        sqlite3_str_appendall(p->out, WENA_FERRET_BOOL);
        return 1;
    case 'n':
        if (strncmp(p->text + p->at, "null", 4) != 0) return 0;
        p->at += 4;
        sqlite3_str_appendall(p->out, WENA_FERRET_NULL);
        return 1;
    default: {
        size_t start = p->at;
        int real = 0;
        double value;
        while (strchr("-+0123456789.eE", p->text[p->at]) != NULL && p->text[p->at] != '\0') {
            if (strchr(".eE", p->text[p->at]) != NULL) real = 1;
            ++p->at;
        }
        if (p->at == start) return 0;
        value = strtod(p->text + start, NULL);
        sqlite3_str_appendall(p->out, real ? WENA_FERRET_DOUBLE :
                              value >= -2147483648.0 && value <= 2147483647.0 ? WENA_FERRET_INT : "{\"t\":\"long\"}");
        return 1;
    }
    }
}

static void element_function(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    Parse p;
    char *result;
    (void)argc;
    p.text = (const char *)sqlite3_value_text(argv[0]);
    if (p.text == NULL) { sqlite3_result_text(context, WENA_FERRET_NULL, -1, SQLITE_STATIC); return; }
    p.at = 0;
    p.out = sqlite3_str_new(NULL);
    if (!element(&p)) {
        sqlite3_free(sqlite3_str_finish(p.out));
        sqlite3_result_error(context, "wena_sjson_element: not JSON", -1);
        return;
    }
    result = sqlite3_str_finish(p.out);
    sqlite3_result_text(context, result, -1, sqlite3_free);
}

int wena_wekan_sync_functions(sqlite3 *db)
{
    return sqlite3_create_function(db, "wena_title", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
                                   title_function, NULL, NULL) == SQLITE_OK &&
           sqlite3_create_function(db, "wena_title", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
                                   title_function, NULL, NULL) == SQLITE_OK &&
           sqlite3_create_function(db, "wena_sjson_element", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
                                   element_function, NULL, NULL) == SQLITE_OK;
}

/* Collections ------------------------------------------------------------- */

static const char *const collections[] = {
    "users", "boards", "swimlanes", "lists", "cards", "checklists", "checklistItems"};
#define COLLECTION_COUNT (sizeof(collections) / sizeof(collections[0]))

static int table_of(sqlite3 *db, const char *collection, char *table)
{
    return wena_ferretdb_collection(db, WENA_WEKAN_SCHEMA, collection, 1, table, WENA_FERRETDB_TABLE_CAPACITY);
}

static char last_error[512];

const char *wena_wekan_sync_error(void)
{
    return last_error;
}

static int exec(sqlite3 *db, const char *sql)
{
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK) return 1;
    sqlite3_snprintf(sizeof(last_error), last_error, "%s: %.300s", sqlite3_errmsg(db), sql);
    return 0;
}

int wena_wekan_sync_attach(sqlite3 *db, const char *path)
{
    sqlite3_stmt *statement = NULL;
    char table[WENA_FERRETDB_TABLE_CAPACITY];
    size_t index;
    int ok;
    if (db == NULL || path == NULL || !wena_wekan_sync_functions(db)) return 0;
    ok = sqlite3_prepare_v2(db, "ATTACH DATABASE ?1 AS " WENA_WEKAN_SCHEMA, -1, &statement, NULL) == SQLITE_OK &&
         sqlite3_bind_text(statement, 1, path, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    /* FerretDB's own settings for the file (pool/uri.go). */
    if (!ok || !exec(db, "PRAGMA " WENA_WEKAN_SCHEMA ".journal_mode=WAL; PRAGMA " WENA_WEKAN_SCHEMA
                         ".synchronous=NORMAL") || !wena_ferretdb_prepare(db, WENA_WEKAN_SCHEMA)) return 0;
    for (index = 0; index < COLLECTION_COUNT; ++index)
        if (!table_of(db, collections[index], table)) return 0;
    return 1;
}

static int expand(sqlite3 *db, const char *template_sql, char *sql, size_t capacity);

/* C89 keeps a string literal to 509 bytes: longer SQL is written as its
 * lines, joined here. */
static int join(const char *const *parts, char *out, size_t capacity)
{
    size_t used = 0, length;
    for (; *parts != NULL; ++parts) {
        length = strlen(*parts);
        if (used + length + 1 > capacity) return 0;
        memcpy(out + used, *parts, length);
        used += length;
    }
    out[used] = '\0';
    return 1;
}

/* Runs SQL with "{users}", "{boards}" ... standing for the collections' tables. */
static int run(sqlite3 *db, const char *const *parts)
{
    char joined[8192], sql[8192];
    return join(parts, joined, sizeof(joined)) && expand(db, joined, sql, sizeof(sql)) && exec(db, sql);
}

/* A document's field: `x` is the document of the row. */
#define ID "x->>'_id'"
#define VALID_ID(e) "(typeof(" e ")='text' AND length(" e ") BETWEEN 1 AND 64 AND " e " NOT GLOB '*[^A-Za-z0-9_-]*')"
#define COLORS "('white','green','yellow','orange','red','purple','blue','sky','lime','pink','black','silver'," \
               "'peachpuff','crimson','plum','darkgreen','slateblue','magenta','gold','navy','gray','saddlebrown'," \
               "'paleturquoise','mistyrose','indigo')"

/* Users: their full name, else username. */
static const char *const import_0[] = {
    "INSERT OR IGNORE INTO actors(id, display_name, version) SELECT " ID ", wena_title(coalesce(",
    "nullif(trim(x->>'$.profile.fullname'), ''), nullif(x->>'username', ''), " ID "), 128), 1 ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {users}) WHERE " VALID_ID(ID),
    NULL};
/* Boards: WeKan's own (not templates, helper or archived boards). */
static const char *const import_1[] = {
    "INSERT OR IGNORE INTO boards(id, title, version) SELECT " ID ", wena_title(x->>'title', 128), 1 ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {boards}) WHERE " VALID_ID(ID) " AND coalesce(x->>'type', 'board') = 'board' ",
    "AND NOT coalesce(x->>'archived', 0) AND coalesce(x->>'title', '') NOT GLOB '^*^'",
    NULL};
static const char *const import_2[] = {
    "INSERT OR IGNORE INTO board_members(board_id, actor_id, active, version, created_at, updated_at) ",
    "SELECT b.id, m.value->>'userId', CASE WHEN coalesce(m.value->>'isActive', 1) THEN 1 ELSE 0 END, 1, m.key, m.key ",
    "FROM boards b JOIN (SELECT _ferretdb_sjson AS x FROM {boards}) ON " ID " = b.id, json_each(x, '$.members') m ",
    "WHERE m.value->>'userId' IN (SELECT id FROM actors)",
    NULL};
static const char *const import_3[] = {
    "INSERT OR IGNORE INTO labels(board_id, id, name, color, position, version, created_at, updated_at) ",
    "SELECT b.id, l.value->>'_id', wena_title(coalesce(l.value->>'name', ''), 128, 1), ",
    "CASE WHEN l.value->>'color' IN " COLORS " THEN l.value->>'color' ELSE '' END, l.key, 1, 0, 0 ",
    "FROM boards b JOIN (SELECT _ferretdb_sjson AS x FROM {boards}) ON " ID " = b.id, json_each(x, '$.labels') l ",
    "WHERE " VALID_ID("l.value->>'_id'"),
    NULL};
static const char *const import_4[] = {
    "INSERT OR IGNORE INTO board_settings(board_id, show_checklist_count) SELECT b.id, ",
    "CASE WHEN coalesce(x->>'allowsChecklistCountBadgeOnCard', 0) THEN 1 ELSE 0 END ",
    "FROM boards b JOIN (SELECT _ferretdb_sjson AS x FROM {boards}) ON " ID " = b.id",
    NULL};
static const char *const import_5[] = {
    "INSERT OR IGNORE INTO board_minicard_settings(board_id, show_checklists) SELECT b.id, ",
    "CASE WHEN coalesce(x->>'allowsChecklistAtMinicard', 0) THEN 1 ELSE 0 END ",
    "FROM boards b JOIN (SELECT _ferretdb_sjson AS x FROM {boards}) ON " ID " = b.id",
    NULL};
static const char *const import_6[] = {
    "INSERT OR IGNORE INTO board_card_collapse_settings(board_id, allow_collapse) SELECT b.id, ",
    "CASE WHEN coalesce(x->>'allowsCardCollapse', 1) THEN 1 ELSE 0 END ",
    "FROM boards b JOIN (SELECT _ferretdb_sjson AS x FROM {boards}) ON " ID " = b.id",
    NULL};
/* Swimlanes and lists, ordered by WeKan's sort. */
static const char *const import_7[] = {
    "INSERT OR IGNORE INTO swimlanes(id, board_id, title, position, version) SELECT " ID ", x->>'boardId', ",
    "wena_title(x->>'title', 128), row_number() OVER (PARTITION BY x->>'boardId' ORDER BY coalesce(x->>'sort', 0), ",
    ID ") - 1, 1 FROM (SELECT _ferretdb_sjson AS x FROM {swimlanes}) WHERE " VALID_ID(ID) " AND x->>'boardId' IN ",
    "(SELECT id FROM boards) AND coalesce(x->>'type', 'swimlane') = 'swimlane'",
    NULL};
static const char *const import_8[] = {
    "INSERT OR IGNORE INTO swimlane_archive_state(swimlane_id, board_id, archived, archived_at) SELECT " ID ", ",
    "x->>'boardId', 1, max(0, coalesce(x->>'archivedAt', 0) / 1000) FROM (SELECT _ferretdb_sjson AS x FROM {swimlanes}) ",
    "WHERE " ID " IN (SELECT id FROM swimlanes) AND coalesce(x->>'archived', 0)",
    NULL};
static const char *const import_9[] = {
    "INSERT OR IGNORE INTO swimlane_colors(swimlane_id, board_id, color) SELECT " ID ", x->>'boardId', x->>'color' ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {swimlanes}) WHERE " ID " IN (SELECT id FROM swimlanes) AND x->>'color' IN " COLORS,
    NULL};
static const char *const import_10[] = {
    "INSERT OR IGNORE INTO lists(id, board_id, title, position, version) SELECT " ID ", x->>'boardId', ",
    "wena_title(x->>'title', 128), row_number() OVER (PARTITION BY x->>'boardId' ORDER BY coalesce(x->>'sort', 0), ",
    ID ") - 1, 1 FROM (SELECT _ferretdb_sjson AS x FROM {lists}) WHERE " VALID_ID(ID) " AND x->>'boardId' IN ",
    "(SELECT id FROM boards) AND coalesce(x->>'type', 'list') = 'list' AND x->>'deletedAt' IS NULL",
    NULL};
static const char *const import_11[] = {
    "INSERT OR IGNORE INTO list_archive_state(list_id, board_id, archived, archived_at) SELECT " ID ", ",
    "x->>'boardId', 1, max(0, coalesce(x->>'archivedAt', 0) / 1000) FROM (SELECT _ferretdb_sjson AS x FROM {lists}) ",
    "WHERE " ID " IN (SELECT id FROM lists) AND coalesce(x->>'archived', 0)",
    NULL};
static const char *const import_12[] = {
    "INSERT OR IGNORE INTO list_colors(list_id, board_id, color) SELECT " ID ", x->>'boardId', x->>'color' ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {lists}) WHERE " ID " IN (SELECT id FROM lists) AND x->>'color' IN " COLORS,
    NULL};
static const char *const import_13[] = {
    "INSERT OR IGNORE INTO list_wip_limits(list_id, board_id, value, enabled, soft) SELECT " ID ", x->>'boardId', ",
    "max(1, coalesce(CAST(x->>'$.wipLimit.value' AS INTEGER), 1)), CASE WHEN coalesce(x->>'$.wipLimit.enabled', 0) ",
    "THEN 1 ELSE 0 END, CASE WHEN coalesce(x->>'$.wipLimit.soft', 0) THEN 1 ELSE 0 END ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {lists}) WHERE " ID " IN (SELECT id FROM lists) AND x->'wipLimit' IS NOT NULL",
    NULL};
/* Cards in a list and swimlane of their board; not linked cards or templates. */
static const char *const import_14[] = {
    "INSERT OR IGNORE INTO cards(id, board_id, swimlane_id, list_id, title, position, archived, version) ",
    "SELECT " ID ", x->>'boardId', x->>'swimlaneId', x->>'listId', wena_title(x->>'title', 128), ",
    "row_number() OVER (PARTITION BY x->>'listId', x->>'swimlaneId' ORDER BY coalesce(x->>'sort', 0), " ID ") - 1, ",
    "CASE WHEN coalesce(x->>'archived', 0) THEN 1 ELSE 0 END, 1 FROM (SELECT _ferretdb_sjson AS x FROM {cards}) ",
    "WHERE " VALID_ID(ID) " AND coalesce(x->>'type', 'cardType-card') = 'cardType-card' AND x->>'deletedAt' IS NULL ",
    "AND EXISTS (SELECT 1 FROM lists l WHERE l.id = x->>'listId' AND l.board_id = x->>'boardId') ",
    "AND EXISTS (SELECT 1 FROM swimlanes s WHERE s.id = x->>'swimlaneId' AND s.board_id = x->>'boardId')",
    NULL};
static const char *const import_15[] = {
    "INSERT OR IGNORE INTO card_archive_state(card_id, board_id, archived_at) SELECT c.id, c.board_id, ",
    "max(0, coalesce(x->>'archivedAt', 0) / 1000) FROM cards c JOIN (SELECT _ferretdb_sjson AS x FROM {cards}) ",
    "ON " ID " = c.id WHERE c.archived",
    NULL};
static const char *const import_16[] = {
    "INSERT OR IGNORE INTO card_descriptions(card_id, board_id, description) SELECT c.id, c.board_id, ",
    "wena_title(x->>'description', 1024, 1) FROM cards c JOIN (SELECT _ferretdb_sjson AS x FROM {cards}) ON " ID " = c.id ",
    "WHERE coalesce(x->>'description', '') <> ''",
    NULL};
static const char *const import_17[] = {
    "INSERT OR IGNORE INTO card_labels(board_id, card_id, label_id) SELECT c.board_id, c.id, l.value ",
    "FROM cards c JOIN (SELECT _ferretdb_sjson AS x FROM {cards}) ON " ID " = c.id, json_each(x, '$.labelIds') l ",
    "WHERE EXISTS (SELECT 1 FROM labels WHERE board_id = c.board_id AND id = l.value)",
    NULL};
static const char *const import_18[] = {
    "INSERT OR IGNORE INTO card_people(board_id, card_id, field, actor_id, position) ",
    "SELECT c.board_id, c.id, f.field, p.value, p.key FROM cards c JOIN (SELECT _ferretdb_sjson AS x FROM {cards}) ",
    "ON " ID " = c.id, (SELECT 'members' AS field UNION ALL SELECT 'assignees') f, json_each(x, '$.' || f.field) p ",
    "WHERE p.value IN (SELECT id FROM actors)",
    NULL};
/* Checklists and their items. */
static const char *const import_19[] = {
    "INSERT OR IGNORE INTO checklists(id, board_id, card_id, title, position, hide_checked_items, hide_all_items, ",
    "show_on_minicard, version, created_at, updated_at) SELECT " ID ", c.board_id, c.id, wena_title(x->>'title', 128), ",
    "row_number() OVER (PARTITION BY c.id ORDER BY coalesce(x->>'sort', 0), " ID ") - 1, ",
    "CASE WHEN coalesce(x->>'hideCheckedChecklistItems', 0) THEN 1 ELSE 0 END, ",
    "CASE WHEN coalesce(x->>'hideAllChecklistItems', 0) THEN 1 ELSE 0 END, ",
    "CASE WHEN x->>'showChecklistAtMinicard' IS NULL THEN NULL WHEN x->>'showChecklistAtMinicard' THEN 1 ELSE 0 END, ",
    "1, max(0, coalesce(x->>'createdAt', 0) / 1000), max(0, coalesce(x->>'modifiedAt', x->>'createdAt', 0) / 1000) ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {checklists}) JOIN cards c ON c.id = x->>'cardId' WHERE " VALID_ID(ID),
    NULL};
static const char *const import_20[] = {
    "INSERT OR IGNORE INTO checklist_items(id, board_id, card_id, checklist_id, title, position, is_finished, version, ",
    "created_at, updated_at) SELECT " ID ", k.board_id, k.card_id, k.id, wena_title(x->>'title', 128), ",
    "row_number() OVER (PARTITION BY k.id ORDER BY coalesce(x->>'sort', 0), " ID ") - 1, ",
    "CASE WHEN coalesce(x->>'isFinished', 0) THEN 1 ELSE 0 END, 1, max(0, coalesce(x->>'createdAt', 0) / 1000), ",
    "max(0, coalesce(x->>'modifiedAt', x->>'createdAt', 0) / 1000) ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {checklistItems}) JOIN checklists k ON k.id = x->>'checklistId' ",
    "WHERE " VALID_ID(ID),
    NULL};
static const char *const *const import_sql[] = {
    import_0, import_1, import_2, import_3, import_4, import_5, import_6, import_7, import_8, import_9, import_10, import_11, import_12, import_13, import_14, import_15, import_16, import_17, import_18, import_19, import_20};

/* Export ------------------------------------------------------------------ */

/* What a WeKan document of each kind carries of Wena's tables: id, then a
 * JSON object of the fields. ?1 is the actor. Fields a WeKan document has
 * but Wena does not keep are not here, and so never written. */
typedef struct Kind {
    const char *name;         /* the collection */
    const char *const *projection;  /* fragments, joined */
    const char *defaults;     /* a new document's other fields, before the projection's */
    int deletable;            /* a row Wena deleted removes the document */
} Kind;

#define BOOL_OF(e) "json(CASE WHEN " e " THEN 'true' ELSE 'false' END)"

static const char *const projection_boards[] = {
     "SELECT b.id, json_object('title', b.title, ",
     "'labels', json(coalesce((SELECT json_group_array(json(v)) FROM (SELECT CASE WHEN e.value IS NOT NULL THEN ",
     "json_set(e.value, '$.name', l.name, '$.color', l.color) ELSE json_object('_id', l.id, 'name', l.name, 'color', l.color) ",
     "END AS v FROM labels l LEFT JOIN (SELECT j.value FROM {boards} d, json_each(d._ferretdb_sjson, '$.labels') j ",
     "WHERE d._ferretdb_sjson->'_id' = json_quote(b.id)) e ON e.value->>'_id' = l.id WHERE l.board_id = b.id ",
     "ORDER BY l.position)), '[]')), ",
     "'members', json(CASE WHEN EXISTS (SELECT 1 FROM board_members WHERE board_id = b.id) THEN ",
     "(SELECT json_group_array(json(v)) FROM (SELECT CASE WHEN e.value IS NOT NULL THEN json_set(e.value, '$.isActive', ",
     BOOL_OF("m.active") ") ELSE json_object('userId', m.actor_id, 'isAdmin', json('false'), 'isActive', " BOOL_OF("m.active"),
     ", 'isNoComments', json('false'), 'isCommentOnly', json('false'), 'isWorker', json('false')) END AS v ",
     "FROM board_members m LEFT JOIN (SELECT j.value FROM {boards} d, json_each(d._ferretdb_sjson, '$.members') j ",
     "WHERE d._ferretdb_sjson->'_id' = json_quote(b.id)) e ON e.value->>'userId' = m.actor_id WHERE m.board_id = b.id ",
     "ORDER BY m.created_at, m.actor_id)) ELSE json_array(json_object('userId', ?1, 'isAdmin', json('true'), ",
     "'isActive', json('true'), 'isNoComments', json('false'), 'isCommentOnly', json('false'), 'isWorker', json('false'))) END), ",
     "'allowsChecklistCountBadgeOnCard', " BOOL_OF("coalesce((SELECT show_checklist_count FROM board_settings WHERE board_id = b.id), 0)") ", ",
     "'allowsChecklistAtMinicard', " BOOL_OF("coalesce((SELECT show_checklists FROM board_minicard_settings WHERE board_id = b.id), 0)") ", ",
     "'allowsCardCollapse', " BOOL_OF("coalesce((SELECT allow_collapse FROM board_card_collapse_settings WHERE board_id = b.id), 1)"),
     ") FROM boards b",
    NULL};
static const char *const projection_swimlanes[] = {
     "SELECT s.id, json_patch(json_object('title', s.title, 'boardId', s.board_id, 'sort', s.position, 'archived', ",
     BOOL_OF("coalesce(a.archived, 0)") "), json_patch(CASE WHEN coalesce(a.archived, 0) THEN json_object('archivedAt', ",
     "a.archived_at * 1000) ELSE '{}' END, CASE WHEN coalesce(c.color, '') <> '' THEN json_object('color', c.color) ",
     "ELSE '{}' END)) FROM swimlanes s LEFT JOIN swimlane_archive_state a ON a.swimlane_id = s.id ",
     "LEFT JOIN swimlane_colors c ON c.swimlane_id = s.id",
    NULL};
static const char *const projection_lists[] = {
     "SELECT l.id, json_patch(json_object('title', l.title, 'boardId', l.board_id, 'sort', l.position, 'archived', ",
     BOOL_OF("coalesce(a.archived, 0)") "), json_patch(json_patch(CASE WHEN coalesce(a.archived, 0) THEN ",
     "json_object('archivedAt', a.archived_at * 1000) ELSE '{}' END, CASE WHEN coalesce(c.color, '') <> '' THEN ",
     "json_object('color', c.color) ELSE '{}' END), CASE WHEN w.list_id IS NOT NULL THEN json_object('wipLimit', ",
     "json_object('value', w.value, 'enabled', " BOOL_OF("w.enabled") ", 'soft', " BOOL_OF("w.soft") ")) ELSE '{}' END)) ",
     "FROM lists l LEFT JOIN list_archive_state a ON a.list_id = l.id LEFT JOIN list_colors c ON c.list_id = l.id ",
     "LEFT JOIN list_wip_limits w ON w.list_id = l.id",
    NULL};
static const char *const projection_cards[] = {
     "SELECT c.id, json_patch(json_object('title', c.title, 'boardId', c.board_id, 'listId', c.list_id, ",
     "'swimlaneId', c.swimlane_id, 'sort', c.position, 'archived', " BOOL_OF("c.archived") ", ",
     "'description', coalesce((SELECT description FROM card_descriptions WHERE card_id = c.id), ''), ",
     "'labelIds', json(coalesce((SELECT json_group_array(label_id) FROM (SELECT k.label_id FROM card_labels k ",
     "JOIN labels l ON l.board_id = k.board_id AND l.id = k.label_id WHERE k.card_id = c.id ORDER BY l.position)), '[]')), ",
     "'members', json(coalesce((SELECT json_group_array(actor_id) FROM (SELECT actor_id FROM card_people ",
     "WHERE card_id = c.id AND field = 'members' ORDER BY position)), '[]')), ",
     "'assignees', json(coalesce((SELECT json_group_array(actor_id) FROM (SELECT actor_id FROM card_people ",
     "WHERE card_id = c.id AND field = 'assignees' ORDER BY position)), '[]'))), ",
     "CASE WHEN c.archived THEN json_object('archivedAt', coalesce((SELECT archived_at FROM card_archive_state ",
     "WHERE card_id = c.id), 0) * 1000) ELSE '{}' END) FROM cards c",
    NULL};
static const char *const projection_checklists[] = {
     "SELECT k.id, json_patch(json_object('cardId', k.card_id, 'boardId', k.board_id, 'title', k.title, 'sort', ",
     "k.position, 'hideCheckedChecklistItems', " BOOL_OF("k.hide_checked_items") ", 'hideAllChecklistItems', ",
     BOOL_OF("k.hide_all_items") "), CASE WHEN k.show_on_minicard IS NULL THEN '{}' ELSE ",
     "json_object('showChecklistAtMinicard', " BOOL_OF("k.show_on_minicard") ") END) FROM checklists k",
    NULL};
static const char *const projection_checklistItems[] = {
     "SELECT i.id, json_object('title', i.title, 'sort', i.position, 'isFinished', " BOOL_OF("i.is_finished") ", ",
     "'checklistId', i.checklist_id, 'cardId', i.card_id, 'boardId', i.board_id) FROM checklist_items i",
    NULL};

static const Kind kinds[] = {
    {"boards", projection_boards,
     "{\"archived\":false,\"permission\":\"private\",\"color\":\"belize\",\"type\":\"board\",\"stars\":0,\"sort\":-1}", 0},
    {"swimlanes", projection_swimlanes,
     "{\"archived\":false,\"type\":\"swimlane\",\"height\":-1}", 0},
    {"lists", projection_lists,
     "{\"archived\":false,\"swimlaneId\":\"\",\"type\":\"list\",\"starred\":false,\"width\":220,"
     "\"wipLimit\":{\"value\":1,\"enabled\":false,\"soft\":false}}", 0},
    {"cards", projection_cards,
     "{\"archived\":false,\"type\":\"cardType-card\",\"description\":\"\",\"labelIds\":[],\"members\":[],"
     "\"assignees\":[],\"requesters\":[],\"assigners\":[],\"customFields\":[],\"parentId\":\"\",\"coverId\":\"\","
     "\"requestedBy\":\"\",\"assignedBy\":\"\",\"dueComplete\":false,\"spentTime\":0,\"isOvertime\":false,"
     "\"linkedId\":\"\",\"subtaskSort\":-1}", 0},
    {"checklists", projection_checklists,
     "{\"resetInterval\":\"none\"}", 1},
    {"checklistItems", projection_checklistItems,
     "{}", 1}
};
#define KIND_COUNT (sizeof(kinds) / sizeof(kinds[0]))

/* Expands "{collection}" in `template_sql` into `sql`. */
static int expand(sqlite3 *db, const char *template_sql, char *sql, size_t capacity)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY];
    const char *at = template_sql;
    size_t used = 0, index;
    while (*at != '\0') {
        if (*at == '{') {
            for (index = 0; index < COLLECTION_COUNT; ++index) {
                size_t length = strlen(collections[index]);
                if (strncmp(at + 1, collections[index], length) == 0 && at[1 + length] == '}') break;
            }
            if (index < COLLECTION_COUNT) {
                if (!table_of(db, collections[index], table) || used + strlen(table) + 10 >= capacity) return 0;
                used += (size_t)sprintf(sql + used, WENA_WEKAN_SCHEMA ".\"%s\"", table);
                at += strlen(collections[index]) + 2;
                continue;
            }
        }
        if (used + 2 >= capacity) return 0;
        sql[used++] = *at++;
    }
    sql[used] = '\0';
    return 1;
}

static int is_date(const char *key)
{
    return !strcmp(key, "createdAt") || !strcmp(key, "modifiedAt") || !strcmp(key, "archivedAt") ||
           !strcmp(key, "dateLastActivity");
}

/* Fills the current projection of a kind into temp.wena_current. */
static int project(sqlite3 *db, const Kind *kind, const char *actor)
{
    char joined[8192], sql[8192], insert[8400];
    sqlite3_stmt *statement = NULL;
    int ok;
    if (!exec(db, "DELETE FROM temp.wena_current") || !join(kind->projection, joined, sizeof(joined)) ||
        !expand(db, joined, sql, sizeof(sql))) return 0;
    sprintf(insert, "INSERT INTO temp.wena_current(id, fields) %s", sql);
    ok = sqlite3_prepare_v2(db, insert, -1, &statement, NULL) == SQLITE_OK &&
         (sqlite3_bind_parameter_count(statement) == 0 ||
          sqlite3_bind_text(statement, 1, actor != NULL ? actor : "", -1, SQLITE_TRANSIENT) == SQLITE_OK) &&
         sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

static int remember(sqlite3 *db, const Kind *kind)
{
    sqlite3_stmt *statement = NULL;
    int ok;
    ok = sqlite3_prepare_v2(db, "DELETE FROM wena_wekan_shadow WHERE kind = ?1", -1, &statement, NULL) == SQLITE_OK &&
         sqlite3_bind_text(statement, 1, kind->name, -1, SQLITE_STATIC) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    statement = NULL;
    ok = ok && sqlite3_prepare_v2(db, "INSERT INTO wena_wekan_shadow(kind, id, fields) SELECT ?1, id, fields "
                                      "FROM temp.wena_current", -1, &statement, NULL) == SQLITE_OK &&
         sqlite3_bind_text(statement, 1, kind->name, -1, SQLITE_STATIC) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

static int tables(sqlite3 *db)
{
    return exec(db, "CREATE TABLE IF NOT EXISTS wena_wekan_shadow(kind TEXT NOT NULL, id TEXT NOT NULL, "
                    "fields TEXT NOT NULL, PRIMARY KEY(kind, id));"
                    "CREATE TEMP TABLE IF NOT EXISTS wena_current(id TEXT PRIMARY KEY, fields TEXT NOT NULL)");
}

int wena_wekan_sync_import(sqlite3 *db)
{
    size_t index;
    if (db == NULL || !tables(db) || !exec(db, "BEGIN")) return 0;
    last_error[0] = '\0';
    for (index = 0; index < sizeof(import_sql) / sizeof(import_sql[0]); ++index) {
        if (!run(db, import_sql[index])) {
            if (last_error[0] == '\0') sqlite3_snprintf(sizeof(last_error), last_error, "import step %d", (int)index);
            goto fail;
        }
    }
    for (index = 0; index < KIND_COUNT; ++index) {
        if (!project(db, &kinds[index], NULL) || !remember(db, &kinds[index])) {
            if (last_error[0] == '\0')
                sqlite3_snprintf(sizeof(last_error), last_error, "%s projection: %s", kinds[index].name, sqlite3_errmsg(db));
            goto fail;
        }
    }
    return exec(db, "COMMIT");
fail:
    exec(db, "ROLLBACK");
    return 0;
}

#define FIELD_CAPACITY 64

/* A field's JSON text and "$s" element, from a JSON object. */
typedef struct Owned {
    char *keys[128], *values[128], *elements[128];
    size_t count;
} Owned;

static void owned_free(Owned *owned)
{
    size_t index;
    for (index = 0; index < owned->count; ++index) {
        sqlite3_free(owned->keys[index]);
        sqlite3_free(owned->values[index]);
        sqlite3_free(owned->elements[index]);
    }
    owned->count = 0;
}

/* A new document's default fields: those of `defaults` its projection does not have. */
static int defaults_of(sqlite3 *db, const char *defaults, char **keys, size_t count, Owned *out)
{
    sqlite3_stmt *statement = NULL;
    size_t index;
    out->count = 0;
    if (sqlite3_prepare_v2(db, "SELECT key, json(?1) -> ('$.\"' || key || '\"'), "
                               "wena_sjson_element(json(?1) -> ('$.\"' || key || '\"')) FROM json_each(?1)",
                           -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, defaults, -1, SQLITE_STATIC);
    while (sqlite3_step(statement) == SQLITE_ROW && out->count < 128) {
        const char *key = (const char *)sqlite3_column_text(statement, 0);
        int present = 0;
        for (index = 0; index < count; ++index) if (!strcmp(keys[index], key)) present = 1;
        if (present) continue;
        out->keys[out->count] = sqlite3_mprintf("%s", key);
        out->values[out->count] = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 1));
        out->elements[out->count] = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 2));
        ++out->count;
    }
    sqlite3_finalize(statement);
    return 1;
}

/* A new board's defaults: Wena's base fields and WeKan's 101 Boolean ones
 * (server/wekan_defaults_data.h), so WeKan shows every part of the board. */
static const char *board_defaults(const char *base)
{
    static char json[8192];
    size_t used, index, length;
    if (json[0] != '\0') return json;
    used = strlen(base);
    if (used < 2 || used >= sizeof(json)) return base;
    memcpy(json, base, used - 1);   /* without its closing brace */
    --used;
    for (index = 0; wekan_board_boolean_defaults[index] != NULL; ++index) {
        length = strlen(wekan_board_boolean_defaults[index]);
        if (used + length + 3 >= sizeof(json)) { json[0] = '\0'; return base; }
        json[used++] = ',';
        memcpy(json + used, wekan_board_boolean_defaults[index], length);
        used += length;
    }
    json[used++] = '}';
    json[used] = '\0';
    return json;
}

/* A string as JSON text. */
static char *json_string(sqlite3 *db, const char *text)
{
    sqlite3_stmt *statement = NULL;
    char *result = NULL;
    if (sqlite3_prepare_v2(db, "SELECT json_quote(?1)", -1, &statement, NULL) == SQLITE_OK &&
        sqlite3_bind_text(statement, 1, text != NULL ? text : "", -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW)
        result = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 0));
    sqlite3_finalize(statement);
    return result;
}

/* Writes one document's changed fields, or the whole of a new one: WeKan's
 * defaults for its kind, the projection, createdAt, and for a card its
 * creator and dateLastActivity. modifiedAt is set either way. */
static int write_document(sqlite3 *db, const Kind *kind, const char *table, const char *id, int fresh,
                          const char *actor, char **keys, char **values, char **elements, size_t count)
{
    WenaFerretField fields[FIELD_CAPACITY + 140];
    char now[32];
    char *creator = NULL, *slug = NULL;
    Owned defaults;
    size_t used = 0, index;
    int ok;
    sqlite3_snprintf(sizeof(now), now, "%lld", wena_ferretdb_now_ms());
    defaults.count = 0;
    if (fresh) {
        if (!defaults_of(db, strcmp(kind->name, "boards") == 0 ? board_defaults(kind->defaults) : kind->defaults,
                         keys, count, &defaults)) return 0;
        for (index = 0; index < defaults.count; ++index) {
            fields[used].key = defaults.keys[index];
            fields[used].value = defaults.values[index];
            fields[used].element = defaults.elements[index];
            ++used;
        }
    }
    for (index = 0; index < count && used < FIELD_CAPACITY + 130; ++index) {
        fields[used].key = keys[index];
        fields[used].value = values[index];
        fields[used].element = is_date(keys[index]) ? WENA_FERRET_DATE : elements[index];
        ++used;
    }
    if (fresh) {
        fields[used].key = "createdAt"; fields[used].element = WENA_FERRET_DATE; fields[used].value = now; ++used;
        if (!strcmp(kind->name, "cards")) {
            creator = json_string(db, actor);
            if (creator == NULL) { owned_free(&defaults); return 0; }
            fields[used].key = "userId"; fields[used].element = WENA_FERRET_STRING; fields[used].value = creator; ++used;
            fields[used].key = "dateLastActivity"; fields[used].element = WENA_FERRET_DATE; fields[used].value = now;
            ++used;
        }
        if (!strcmp(kind->name, "boards")) {
            /* WeKan's slug: the title, lowercased, words joined by "-". */
            for (index = 0; index < count; ++index) {
                if (strcmp(keys[index], "title") != 0) continue;
                {
                    sqlite3_stmt *statement = NULL;
                    if (sqlite3_prepare_v2(db, "SELECT json_quote(coalesce(nullif(trim(replace(lower(json(?1) ->> '$'), "
                                               "' ', '-'), '-'), ''), 'board'))", -1, &statement, NULL) == SQLITE_OK &&
                        sqlite3_bind_text(statement, 1, values[index], -1, SQLITE_STATIC) == SQLITE_OK &&
                        sqlite3_step(statement) == SQLITE_ROW)
                        slug = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 0));
                    sqlite3_finalize(statement);
                }
            }
            fields[used].key = "slug"; fields[used].element = WENA_FERRET_STRING;
            fields[used].value = slug != NULL ? slug : "\"board\""; ++used;
        }
    }
    fields[used].key = "modifiedAt"; fields[used].element = WENA_FERRET_DATE; fields[used].value = now; ++used;
    ok = fresh ? wena_ferretdb_insert(db, WENA_WEKAN_SCHEMA, table, id, fields, used) :
                 wena_ferretdb_update(db, WENA_WEKAN_SCHEMA, table, id, fields, used);
    sqlite3_free(creator);
    sqlite3_free(slug);
    owned_free(&defaults);
    return ok;
}

static int export_kind(sqlite3 *db, const Kind *kind, const char *actor, int *written)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY], current[65];
    char *keys[FIELD_CAPACITY], *values[FIELD_CAPACITY], *elements[FIELD_CAPACITY];
    size_t count = 0, index;
    int fresh = 0, ok = 1;
    sqlite3_stmt *statement = NULL;
    if (!table_of(db, kind->name, table) || !project(db, kind, actor)) return 0;
    /* Each changed field: new documents have all of theirs. */
    if (sqlite3_prepare_v2(db,
        "SELECT c.id, k.key, c.fields -> ('$.\"' || k.key || '\"'), "
        "wena_sjson_element(c.fields -> ('$.\"' || k.key || '\"')), s.id IS NULL "
        "FROM temp.wena_current c, json_each(c.fields) k LEFT JOIN wena_wekan_shadow s ON s.kind = ?1 AND s.id = c.id "
        "WHERE s.id IS NULL OR (s.fields -> ('$.\"' || k.key || '\"')) IS NOT (c.fields -> ('$.\"' || k.key || '\"')) "
        "ORDER BY c.id", -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, kind->name, -1, SQLITE_STATIC);
    current[0] = '\0';
    for (;;) {
        int step = sqlite3_step(statement);
        const char *id = step == SQLITE_ROW ? (const char *)sqlite3_column_text(statement, 0) : NULL;
        if (step != SQLITE_ROW && step != SQLITE_DONE) { ok = 0; break; }
        if (current[0] != '\0' && (id == NULL || strcmp(id, current) != 0)) {
            if (!write_document(db, kind, table, current, fresh, actor, keys, values, elements, count)) ok = 0;
            else ++*written;
            for (index = 0; index < count; ++index) {
                sqlite3_free(keys[index]); sqlite3_free(values[index]); sqlite3_free(elements[index]);
            }
            count = 0;
            current[0] = '\0';
            if (!ok) break;
        }
        if (id == NULL) break;
        if (strlen(id) >= sizeof(current) || count == FIELD_CAPACITY) { ok = 0; break; }
        strcpy(current, id);
        fresh = sqlite3_column_int(statement, 4);
        keys[count] = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 1));
        values[count] = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 2));
        elements[count] = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 3));
        ++count;
    }
    for (index = 0; index < count; ++index) {
        sqlite3_free(keys[index]); sqlite3_free(values[index]); sqlite3_free(elements[index]);
    }
    sqlite3_finalize(statement);
    statement = NULL;
    /* Rows Wena deleted. */
    if (ok && kind->deletable) {
        if (sqlite3_prepare_v2(db, "SELECT id FROM wena_wekan_shadow WHERE kind = ?1 AND id NOT IN "
                                   "(SELECT id FROM temp.wena_current)", -1, &statement, NULL) != SQLITE_OK) return 0;
        sqlite3_bind_text(statement, 1, kind->name, -1, SQLITE_STATIC);
        while (ok && sqlite3_step(statement) == SQLITE_ROW) {
            /* Already gone from the file is as good as deleted. */
            (void)wena_ferretdb_delete(db, WENA_WEKAN_SCHEMA, table, (const char *)sqlite3_column_text(statement, 0));
            ++*written;
        }
        sqlite3_finalize(statement);
    }
    return ok && remember(db, kind);
}

int wena_wekan_sync_export(sqlite3 *db, const char *actor)
{
    size_t index;
    int written = 0;
    if (db == NULL || !tables(db) || !exec(db, "SAVEPOINT wena_wekan_export")) return -1;
    for (index = 0; index < KIND_COUNT; ++index) {
        if (!export_kind(db, &kinds[index], actor, &written)) {
            exec(db, "ROLLBACK TO wena_wekan_export");
            exec(db, "RELEASE wena_wekan_export");
            return -1;
        }
    }
    return exec(db, "RELEASE wena_wekan_export") ? written : -1;
}

/* The user -------------------------------------------------------------- */

int wena_wekan_sync_user(sqlite3 *db, const char *wanted, char *id, size_t capacity)
{
    char sql[1024];
    sqlite3_stmt *statement = NULL;
    const unsigned char *found;
    int ok = 0;
    if (db == NULL || id == NULL || capacity == 0 || !expand(db,
        "SELECT x->>'_id' FROM (SELECT _ferretdb_sjson AS x FROM {users}) WHERE " VALID_ID(ID) " ORDER BY "
        "CASE WHEN ?1 IS NOT NULL AND (x->>'_id' = ?1 OR x->>'username' = ?1) THEN 0 "
        "WHEN coalesce(x->>'isAdmin', 0) THEN 1 ELSE 2 END, coalesce(x->>'createdAt', 0), x->>'_id' LIMIT 1",
        sql, sizeof(sql))) return 0;
    id[0] = '\0';
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    if (wanted != NULL && wanted[0] != '\0') sqlite3_bind_text(statement, 1, wanted, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW && (found = sqlite3_column_text(statement, 0)) != NULL &&
        strlen((const char *)found) < capacity) {
        strcpy(id, (const char *)found);
        ok = 1;
    }
    sqlite3_finalize(statement);
    if (ok) {
        /* `wanted` names somebody else than who was found: not that user. */
        return 1;
    }
    {
        /* A new file: its first user, an admin, as WeKan's first one is. */
        char table[WENA_FERRETDB_TABLE_CAPACITY], new_id[18], now[32];
        static const char alphabet[] = "23456789ABCDEFGHJKLMNPQRSTWXYZabcdefghijkmnopqrstuvwxyz";
        unsigned char random[17];
        WenaFerretField fields[8];
        size_t index;
        const char *name = wanted != NULL && wanted[0] != '\0' ? wanted : "admin";
        char quoted[160];
        if (strlen(name) > 60 || strchr(name, '"') != NULL || strchr(name, '\\') != NULL) return 0;
        sqlite3_randomness(17, random);
        for (index = 0; index < 17; ++index) new_id[index] = alphabet[random[index] % (sizeof(alphabet) - 1)];
        new_id[17] = '\0';
        sqlite3_snprintf(sizeof(now), now, "%lld", wena_ferretdb_now_ms());
        sprintf(quoted, "\"%s\"", name);
        fields[0].key = "username"; fields[0].element = WENA_FERRET_STRING; fields[0].value = quoted;
        fields[1].key = "emails"; fields[1].element = "{\"t\":\"array\",\"i\":[]}"; fields[1].value = "[]";
        fields[2].key = "profile";
        fields[2].element = "{\"t\":\"object\",\"$s\":{\"p\":{\"fullname\":{\"t\":\"string\"}},\"$k\":[\"fullname\"]}}";
        sprintf(sql, "{\"fullname\":\"%s\"}", name);
        fields[2].value = sql;
        fields[3].key = "isAdmin"; fields[3].element = WENA_FERRET_BOOL; fields[3].value = "true";
        fields[4].key = "authenticationMethod"; fields[4].element = WENA_FERRET_STRING; fields[4].value = "\"password\"";
        fields[5].key = "createdAt"; fields[5].element = WENA_FERRET_DATE; fields[5].value = now;
        fields[6].key = "modifiedAt"; fields[6].element = WENA_FERRET_DATE; fields[6].value = now;
        if (!table_of(db, "users", table) || !wena_ferretdb_insert(db, WENA_WEKAN_SCHEMA, table, new_id, fields, 7) ||
            strlen(new_id) >= capacity) return 0;
        strcpy(id, new_id);
        return 1;
    }
}

/* All Boards ------------------------------------------------------------ */

static const char *const boards_query[] = {
    "SELECT " ID ", wena_title(x->>'title', 128), coalesce(x->>'color', 'belize'), ",
    "CASE WHEN coalesce(x->>'archived', 0) THEN 1 ELSE 0 END, ",
    "CASE WHEN " ID " IN (SELECT s.value FROM {users} u, json_each(u._ferretdb_sjson, '$.profile.starredBoards') s ",
    "WHERE u._ferretdb_sjson->'_id' = json_quote(?1)) THEN 1 ELSE 0 END, ",
    "CASE WHEN x->>'type' = 'template-container' THEN 1 ELSE 0 END, ",
    "CASE WHEN " ID " IN (SELECT id FROM boards) THEN 1 ELSE 0 END ",
    "FROM (SELECT _ferretdb_sjson AS x FROM {boards}) WHERE " VALID_ID(ID),
    " AND coalesce(x->>'type', 'board') IN ('board', 'template-container') ",
    "AND (coalesce(x->>'title', '') NOT GLOB '^*^' OR coalesce(x->>'archived', 0)) ",
    "AND EXISTS (SELECT 1 FROM json_each(x, '$.members') m WHERE m.value->>'userId' = ?1 ",
    "AND coalesce(m.value->>'isActive', 1)) ORDER BY lower(x->>'title'), " ID,
    NULL};

int wena_wekan_sync_boards(sqlite3 *db, const char *actor, WenaWekanBoardTile *tiles, size_t capacity,
                           size_t *count)
{
    char joined[4096], sql[4096];
    sqlite3_stmt *statement = NULL;
    size_t found = 0;
    int step;
    if (db == NULL || actor == NULL || tiles == NULL || count == NULL || !join(boards_query, joined, sizeof(joined)) ||
        !expand(db, joined, sql, sizeof(sql)) || sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(statement, 1, actor, -1, SQLITE_TRANSIENT);
    while ((step = sqlite3_step(statement)) == SQLITE_ROW && found < capacity) {
        WenaWekanBoardTile *tile = &tiles[found];
        const char *id = (const char *)sqlite3_column_text(statement, 0);
        const char *title = (const char *)sqlite3_column_text(statement, 1);
        const char *color = (const char *)sqlite3_column_text(statement, 2);
        if (id == NULL || title == NULL || strlen(id) >= sizeof(tile->id) || strlen(title) >= sizeof(tile->title))
            continue;
        strcpy(tile->id, id);
        strcpy(tile->title, title);
        tile->color[0] = '\0';
        if (color != NULL && strlen(color) < sizeof(tile->color)) strcpy(tile->color, color);
        tile->archived = sqlite3_column_int(statement, 3);
        tile->starred = sqlite3_column_int(statement, 4);
        tile->template_board = sqlite3_column_int(statement, 5);
        tile->openable = sqlite3_column_int(statement, 6);
        ++found;
    }
    sqlite3_finalize(statement);
    if (step != SQLITE_ROW && step != SQLITE_DONE) return 0;
    *count = found;
    return 1;
}

int wena_wekan_sync_star(sqlite3 *db, const char *actor, const char *board, int starred)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY], sql[1024];
    sqlite3_stmt *statement = NULL;
    WenaFerretField field;
    char *value = NULL, *element = NULL;
    int ok = 0;
    if (db == NULL || actor == NULL || board == NULL || !table_of(db, "users", table)) return 0;
    /* The stars without this board, and with it at the end when starring. */
    sprintf(sql, "SELECT json_group_array(value), wena_sjson_element(json_group_array(value)) FROM ("
                 "SELECT s.value FROM " WENA_WEKAN_SCHEMA ".\"%s\" u, json_each(u._ferretdb_sjson, "
                 "'$.profile.starredBoards') s WHERE u._ferretdb_sjson->'_id' = json_quote(?1) AND s.value <> ?2 "
                 "UNION ALL SELECT ?2 WHERE ?3)", table);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, actor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, starred != 0);
    if (sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 0));
        element = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 1));
    }
    sqlite3_finalize(statement);
    if (value != NULL && element != NULL) {
        field.key = "profile.starredBoards";
        field.element = element;
        field.value = value;
        ok = wena_ferretdb_update(db, WENA_WEKAN_SCHEMA, table, actor, &field, 1);
    }
    sqlite3_free(value);
    sqlite3_free(element);
    return ok;
}

static void meteor_id(char out[18])
{
    static const char alphabet[] = "23456789ABCDEFGHJKLMNPQRSTWXYZabcdefghijkmnopqrstuvwxyz";
    unsigned char random[17];
    int index;
    sqlite3_randomness(17, random);
    for (index = 0; index < 17; ++index) out[index] = alphabet[random[index] % (sizeof(alphabet) - 1)];
    out[17] = '\0';
}

static int insert_row(sqlite3 *db, const char *sql, const char *a, const char *b, const char *c)
{
    sqlite3_stmt *statement = NULL;
    int ok = sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK &&
             sqlite3_bind_text(statement, 1, a, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             (b == NULL || sqlite3_bind_text(statement, 2, b, -1, SQLITE_TRANSIENT) == SQLITE_OK) &&
             (c == NULL || sqlite3_bind_text(statement, 3, c, -1, SQLITE_TRANSIENT) == SQLITE_OK) &&
             sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

int wena_wekan_sync_new_board(sqlite3 *db, const char *actor, const char *title, char *board, size_t capacity)
{
    char lane[18], id[18];
    if (db == NULL || actor == NULL || title == NULL || board == NULL || capacity < sizeof(id)) return 0;
    meteor_id(id);
    meteor_id(lane);
    if (!exec(db, "SAVEPOINT wena_new_board")) return 0;
    if (!insert_row(db, "INSERT INTO boards(id, title, version) VALUES (?1, wena_title(?2, 128), 1)", id, title, NULL) ||
        !insert_row(db, "INSERT INTO swimlanes(id, board_id, title, position, version) VALUES (?1, ?2, 'Default', 0, 1)",
                    lane, id, NULL)) {
        exec(db, "ROLLBACK TO wena_new_board");
        exec(db, "RELEASE wena_new_board");
        return 0;
    }
    /* Written with its creator as admin - a board without member rows is
     * exported so - then the membership Wena keeps, which the export merges
     * with that member and so leaves as it is. */
    if (!exec(db, "RELEASE wena_new_board") || wena_wekan_sync_export(db, actor) < 0 ||
        !insert_row(db, "INSERT INTO board_members(board_id, actor_id, active, version, created_at, updated_at) "
                        "VALUES (?1, ?2, 1, 1, 0, 0)", id, actor, NULL)) return 0;
    strcpy(board, id);
    return 1;
}

int wena_wekan_sync_language(sqlite3 *db, const char *actor, char *language, size_t capacity)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY], sql[256];
    sqlite3_stmt *statement = NULL;
    const unsigned char *found;
    int ok = 0;
    if (db == NULL || actor == NULL || language == NULL || capacity == 0 || !table_of(db, "users", table)) return 0;
    language[0] = '\0';
    sprintf(sql, "SELECT _ferretdb_sjson ->> '$.profile.language' FROM " WENA_WEKAN_SCHEMA ".\"%s\" "
                 "WHERE _ferretdb_sjson->'_id' = json_quote(?1)", table);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, actor, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW && (found = sqlite3_column_text(statement, 0)) != NULL &&
        found[0] != '\0' && strlen((const char *)found) < capacity) {
        strcpy(language, (const char *)found);
        ok = 1;
    }
    sqlite3_finalize(statement);
    return ok;
}

int wena_wekan_sync_set_language(sqlite3 *db, const char *actor, const char *language)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY];
    WenaFerretField field;
    char *value;
    int ok;
    if (db == NULL || actor == NULL || language == NULL || !table_of(db, "users", table) ||
        (value = json_string(db, language)) == NULL) return 0;
    field.key = "profile.language";
    field.element = WENA_FERRET_STRING;
    field.value = value;
    ok = wena_ferretdb_update(db, WENA_WEKAN_SCHEMA, table, actor, &field, 1);
    sqlite3_free(value);
    return ok;
}

static int profile_field(const char *field)
{
    return field != NULL && (!strcmp(field, "collapsedLists") || !strcmp(field, "collapsedSwimlanes") ||
                             !strcmp(field, "swimlaneHeights"));
}

int wena_wekan_sync_profile_board_map(sqlite3 *db, const char *actor, const char *field, const char *board,
                                      void (*entry)(void *context, const char *id, int value), void *context)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY], sql[512];
    sqlite3_stmt *statement = NULL;
    if (db == NULL || actor == NULL || board == NULL || entry == NULL || !profile_field(field) ||
        !table_of(db, "users", table)) return 0;
    sprintf(sql, "SELECT m.key, CAST(m.value AS INTEGER) FROM " WENA_WEKAN_SCHEMA ".\"%s\" u, json_each(u._ferretdb_sjson, "
                 "'$.profile.%s.\"' || ?2 || '\"') m WHERE u._ferretdb_sjson->'_id' = json_quote(?1)", table, field);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, actor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(statement, 0);
        if (id != NULL) entry(context, id, sqlite3_column_int(statement, 1));
    }
    sqlite3_finalize(statement);
    return 1;
}

int wena_wekan_sync_set_profile_board_map(sqlite3 *db, const char *actor, const char *field, const char *board,
                                          const char *map)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY], sql[640], key[64];
    sqlite3_stmt *statement = NULL;
    WenaFerretField update;
    char *value = NULL, *element = NULL;
    int ok = 0;
    if (db == NULL || actor == NULL || board == NULL || map == NULL || !profile_field(field) ||
        !table_of(db, "users", table)) return 0;
    /* The whole map, with this board's entry replaced (or added). */
    sprintf(sql, "SELECT json_set(coalesce(u._ferretdb_sjson -> '$.profile.%s', '{}'), '$.\"' || ?2 || '\"', json(?3)) "
                 "FROM " WENA_WEKAN_SCHEMA ".\"%s\" u WHERE u._ferretdb_sjson->'_id' = json_quote(?1)", field, table);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(statement, 1, actor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, map, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_text(statement, 0) != NULL)
        value = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 0));
    sqlite3_finalize(statement);
    statement = NULL;
    if (value != NULL && sqlite3_prepare_v2(db, "SELECT wena_sjson_element(?1)", -1, &statement, NULL) == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, value, -1, SQLITE_STATIC);
        if (sqlite3_step(statement) == SQLITE_ROW)
            element = sqlite3_mprintf("%s", (const char *)sqlite3_column_text(statement, 0));
    }
    sqlite3_finalize(statement);
    if (value != NULL && element != NULL) {
        sprintf(key, "profile.%s", field);
        update.key = key;
        update.element = element;
        update.value = value;
        ok = wena_ferretdb_update(db, WENA_WEKAN_SCHEMA, table, actor, &update, 1);
    }
    sqlite3_free(value);
    sqlite3_free(element);
    return ok;
}
