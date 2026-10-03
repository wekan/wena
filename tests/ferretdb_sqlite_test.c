/* FerretDB's SQLite storage written directly: table names, the metadata
 * table and collection DDL exactly as FerretDB makes them, and documents whose
 * "$s" type schema stays right through insert, update, unset and delete. */
#include "../server/ferretdb_sqlite.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char text[8192];
static const char *scalar(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *s = NULL;
    const unsigned char *v;
    text[0] = '\0';
    assert(sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK);
    /* The last column: EXPLAIN QUERY PLAN's detail, or the only one. */
    if (sqlite3_step(s) == SQLITE_ROW && (v = sqlite3_column_text(s, sqlite3_column_count(s) - 1)) != NULL)
        strcpy(text, (const char *)v);
    sqlite3_finalize(s);
    return text;
}

int main(int argc, char **argv)
{
    static const char *const expected[][2] = {
        {"boards", "boards_7c666488"}, {"swimlanes", "swimlanes_d9d57a4c"}, {"lists", "lists_61ad04f6"},
        {"cards", "cards_81f16044"}, {"checklists", "checklists_4de64774"},
        {"checklistItems", "checklistitems_ef324221"}, {"users", "users_5e7cc513"},
        {"activities", "activities_69d4f298"}, {"card_comments", "card_comments_fb76f282"}};
    char table[WENA_FERRETDB_TABLE_CAPACITY], path[512], other[WENA_FERRETDB_TABLE_CAPACITY];
    sqlite3 *db;
    size_t index;
    WenaFerretField fields[3];
    const char *removed[1];
    assert(argc == 2);
    /* FerretDB's names for WeKan's collections (FNV-1a 32 of the name). */
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        assert(wena_ferretdb_table_name(expected[index][0], wena_ferretdb_fnv1a(expected[index][0]), table,
                                        sizeof(table)));
        assert(!strcmp(table, expected[index][1]));
    }
    assert(wena_ferretdb_table_name("sqlite_x", 1, table, sizeof(table)) && !strcmp(table, "_sqlite_x_00000001"));
    assert(!wena_ferretdb_table_name("", 1, table, sizeof(table)));

    sprintf(path, "%s/wekan.sqlite", argv[1]);
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    /* Built as the release builds SQLite: no double-quoted strings. */
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DDL, 0, NULL);
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DML, 0, NULL);
    assert(wena_ferretdb_prepare(db, "main") && wena_ferretdb_prepare(db, "main"));
    assert(!strcmp(scalar(db, "SELECT sql FROM sqlite_schema WHERE name='_ferretdb_collections'"),
        "CREATE TABLE \"_ferretdb_collections\" (name TEXT NOT NULL UNIQUE CHECK(name != ''), table_name TEXT "
        "NOT NULL UNIQUE CHECK(table_name != ''), settings TEXT NOT NULL CHECK(settings != '')) STRICT"));
    /* Negative: a missing collection is not made unless asked. */
    assert(!wena_ferretdb_collection(db, "main", "boards", 0, table, sizeof(table)));
    assert(wena_ferretdb_collection(db, "main", "boards", 1, table, sizeof(table)) && !strcmp(table, "boards_7c666488"));
    assert(wena_ferretdb_collection(db, "main", "boards", 0, other, sizeof(other)) && !strcmp(other, table));
    assert(!strcmp(scalar(db, "SELECT sql FROM sqlite_schema WHERE name='boards_7c666488'"),
        "CREATE TABLE \"boards_7c666488\" (_ferretdb_sjson TEXT NOT NULL CHECK(_ferretdb_sjson != '')) STRICT"));
    assert(!strcmp(scalar(db, "SELECT sql FROM sqlite_schema WHERE name='boards_7c666488__id_'"),
        "CREATE UNIQUE INDEX \"boards_7c666488__id_\" ON \"boards_7c666488\" (_ferretdb_sjson->\"_id\")"));
    assert(!strcmp(scalar(db, "SELECT json_extract(settings,'$.indexes[0].name')||json_extract(settings,'$.indexFormat')"
                              "||length(json_extract(settings,'$.uuid')) FROM _ferretdb_collections"), "_id_236"));
    /* A collision: another collection already holding "boards"'s table name
     * moves the hash on by one. */
    assert(sqlite3_exec(db, "INSERT INTO _ferretdb_collections VALUES('x','cards_81f16044','{}')", NULL, NULL, NULL) == SQLITE_OK);
    assert(wena_ferretdb_collection(db, "main", "cards", 1, table, sizeof(table)) && !strcmp(table, "cards_81f16045"));

    /* Documents. */
    fields[0].key = "title"; fields[0].element = WENA_FERRET_STRING; fields[0].value = "\"My \\\"board\\\"\"";
    fields[1].key = "archived"; fields[1].element = WENA_FERRET_BOOL; fields[1].value = "false";
    fields[2].key = "createdAt"; fields[2].element = WENA_FERRET_DATE; fields[2].value = "1700000000000";
    assert(wena_ferretdb_insert(db, "main", "boards_7c666488", "b1", fields, 3));
    assert(!strcmp(scalar(db, "SELECT _ferretdb_sjson FROM boards_7c666488"),
        "{\"$s\":{\"p\":{\"_id\":{\"t\":\"string\"},\"title\":{\"t\":\"string\"},\"archived\":{\"t\":\"bool\"},"
        "\"createdAt\":{\"t\":\"date\"}},\"$k\":[\"_id\",\"title\",\"archived\",\"createdAt\"]},\"_id\":\"b1\","
        "\"title\":\"My \\\"board\\\"\",\"archived\":false,\"createdAt\":1700000000000}"));
    /* Negative: the same _id twice is refused by FerretDB's unique index. */
    assert(!wena_ferretdb_insert(db, "main", "boards_7c666488", "b1", fields, 1));
    /* Update keeps the order and adds new fields at the end of "$k". */
    fields[0].value = "\"Renamed\"";
    fields[1].key = "labels"; fields[1].element = "{\"t\":\"array\",\"i\":[{\"t\":\"string\"}]}"; fields[1].value = "[\"l1\"]";
    assert(wena_ferretdb_update(db, "main", "boards_7c666488", "b1", fields, 2));
    assert(!strcmp(scalar(db, "SELECT _ferretdb_sjson->'$.\"$s\".\"$k\"' FROM boards_7c666488"),
                   "[\"_id\",\"title\",\"archived\",\"createdAt\",\"labels\"]"));
    assert(!strcmp(scalar(db, "SELECT _ferretdb_sjson->>'title' FROM boards_7c666488"), "Renamed"));
    /* Unset removes a field everywhere. */
    removed[0] = "archived";
    assert(wena_ferretdb_unset(db, "main", "boards_7c666488", "b1", removed, 1));
    assert(!strcmp(scalar(db, "SELECT _ferretdb_sjson->'$.\"$s\".\"$k\"' || coalesce(_ferretdb_sjson->'$.\"$s\".p.archived','-')"
                              " || coalesce(_ferretdb_sjson->'archived','-') FROM boards_7c666488"),
                   "[\"_id\",\"title\",\"createdAt\",\"labels\"]--"));
    /* Negative: the _id cannot be unset, odd field names are refused, and a
     * missing document is not written. */
    removed[0] = "_id";
    assert(!wena_ferretdb_unset(db, "main", "boards_7c666488", "b1", removed, 1));
    fields[0].key = "a.b";
    assert(!wena_ferretdb_update(db, "main", "boards_7c666488", "b1", fields, 1));
    fields[0].key = "title";
    assert(!wena_ferretdb_update(db, "main", "boards_7c666488", "missing", fields, 1));
    /* One level into an object: the field and its type go into the object's
     * own "$s" - as WeKan's users.profile.starredBoards. */
    fields[0].key = "profile";
    fields[0].element = "{\"t\":\"object\",\"$s\":{\"p\":{\"fullname\":{\"t\":\"string\"}},\"$k\":[\"fullname\"]}}";
    fields[0].value = "{\"fullname\":\"Ada\"}";
    assert(wena_ferretdb_update(db, "main", "boards_7c666488", "b1", fields, 1));
    fields[0].key = "profile.starredBoards";
    fields[0].element = "{\"t\":\"array\",\"i\":[{\"t\":\"string\"}]}";
    fields[0].value = "[\"b1\"]";
    assert(wena_ferretdb_update(db, "main", "boards_7c666488", "b1", fields, 1));
    assert(!strcmp(scalar(db, "SELECT (_ferretdb_sjson -> 'profile') || (_ferretdb_sjson -> '$.\"$s\".p.profile.\"$s\".\"$k\"') "
                              "|| (_ferretdb_sjson -> '$.\"$s\".p.profile.\"$s\".p.starredBoards.t') FROM boards_7c666488"),
                   "{\"fullname\":\"Ada\",\"starredBoards\":[\"b1\"]}[\"fullname\",\"starredBoards\"]\"array\""));
    /* Negative: no parent object to put it in, two levels, and unset of a nested field. */
    fields[0].key = "missing.child";
    assert(!wena_ferretdb_update(db, "main", "boards_7c666488", "b1", fields, 1));
    fields[0].key = "profile.a.b";
    assert(!wena_ferretdb_update(db, "main", "boards_7c666488", "b1", fields, 1));
    removed[0] = "profile.fullname";
    assert(!wena_ferretdb_unset(db, "main", "boards_7c666488", "b1", removed, 1));
    /* The _id lookup uses FerretDB's index. */
    assert(strstr(scalar(db, "EXPLAIN QUERY PLAN SELECT 1 FROM boards_7c666488 WHERE _ferretdb_sjson->'_id' = json_quote('b1')"),
                  "USING INDEX boards_7c666488__id_") != NULL);
    assert(wena_ferretdb_delete(db, "main", "boards_7c666488", "b1"));
    assert(!wena_ferretdb_delete(db, "main", "boards_7c666488", "b1"));
    assert(!strcmp(scalar(db, "SELECT count(*) FROM boards_7c666488"), "0"));
    assert(sqlite3_close(db) == SQLITE_OK);
    puts("FerretDB SQLite: table names, metadata and collection DDL, documents and their $s passed");
    return 0;
}
