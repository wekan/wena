#include "ferretdb_sqlite.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

unsigned long wena_ferretdb_fnv1a(const char *text)
{
    unsigned long hash = 2166136261UL;
    const unsigned char *byte;
    for (byte = (const unsigned char *)text; *byte != '\0'; ++byte) {
        hash ^= *byte;
        hash = (hash * 16777619UL) & 0xffffffffUL;
    }
    return hash;
}

int wena_ferretdb_table_name(const char *collection, unsigned long hash, char *out, size_t capacity)
{
    char lower[128];
    size_t index, length;
    if (collection == NULL || out == NULL || (length = strlen(collection)) == 0 || length >= sizeof(lower))
        return 0;
    for (index = 0; index <= length; ++index)
        lower[index] = (char)(collection[index] >= 'A' && collection[index] <= 'Z' ?
                              collection[index] - 'A' + 'a' : collection[index]);
    /* FerretDB's registry.go: "<lowercased>_<%08x>", "_" before "sqlite_". */
    if (length + 1 + 8 + 2 > capacity) return 0;
    sprintf(out, "%s%s_%08lx", strncmp(lower, "sqlite_", 7) == 0 ? "_" : "", lower, hash & 0xffffffffUL);
    return 1;
}

/* A schema or table name inside double quotes: no quote can be in it. */
static int plain(const char *name)
{
    return name != NULL && name[0] != '\0' && strchr(name, '"') == NULL && strlen(name) < 128;
}

/* A field name used inside a JSON path: a top-level one, or "parent.child"
 * one level into an object. */
static int field_name(const char *key)
{
    const char *c;
    int dots = 0;
    if (key == NULL || key[0] == '\0' || key[0] == '.' || strlen(key) > 100) return 0;
    for (c = key; *c != '\0'; ++c) {
        if (*c == '.') {
            if (++dots > 1 || c[1] == '\0') return 0;
            continue;
        }
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
              *c == '_' || *c == '$' || *c == '-')) return 0;
    }
    return 1;
}

static int exec(sqlite3 *db, const char *sql)
{
    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

int wena_ferretdb_prepare(sqlite3 *db, const char *schema)
{
    char sql[512];
    if (db == NULL || !plain(schema)) return 0;
    /* FerretDB's metadata/registry.go, byte for byte. */
    sprintf(sql, "CREATE TABLE IF NOT EXISTS \"%s\".\"_ferretdb_collections\" (name TEXT NOT NULL UNIQUE "
                 "CHECK(name != ''), table_name TEXT NOT NULL UNIQUE CHECK(table_name != ''), settings TEXT "
                 "NOT NULL CHECK(settings != '')) STRICT", schema);
    return exec(db, sql);
}

static int lookup(sqlite3 *db, const char *schema, const char *column, const char *value,
                  char *table, size_t capacity)
{
    char sql[256];
    sqlite3_stmt *statement = NULL;
    const unsigned char *found;
    int ok = 0;
    sprintf(sql, "SELECT table_name FROM \"%s\".\"_ferretdb_collections\" WHERE %s=?1", schema, column);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    if (sqlite3_bind_text(statement, 1, value, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW && (found = sqlite3_column_text(statement, 0)) != NULL &&
        (table == NULL || strlen((const char *)found) < capacity)) {
        if (table != NULL) strcpy(table, (const char *)found);
        ok = 1;
    }
    sqlite3_finalize(statement);
    return ok;
}

static void uuid4(char out[37])
{
    unsigned char bytes[16];
    char hex[33];
    int index;
    sqlite3_randomness(16, bytes);
    bytes[6] = (unsigned char)((bytes[6] & 0x0f) | 0x40);
    bytes[8] = (unsigned char)((bytes[8] & 0x3f) | 0x80);
    for (index = 0; index < 16; ++index) sprintf(hex + index * 2, "%02x", bytes[index]);
    sprintf(out, "%.8s-%.4s-%.4s-%.4s-%.12s", hex, hex + 8, hex + 12, hex + 16, hex + 20);
}

int wena_ferretdb_collection(sqlite3 *db, const char *schema, const char *collection, int create,
                             char *table, size_t capacity)
{
    char name[WENA_FERRETDB_TABLE_CAPACITY], sql[768], uuid[37];
    unsigned long hash;
    int dqs = 1, ok;
    sqlite3_stmt *statement = NULL;
    if (db == NULL || !plain(schema) || collection == NULL || collection[0] == '\0' || table == NULL)
        return 0;
    if (lookup(db, schema, "name", collection, table, capacity)) return 1;
    if (!create) return 0;
    /* A name taken by another collection moves the hash on, as FerretDB does. */
    for (hash = wena_ferretdb_fnv1a(collection);; hash = (hash + 1) & 0xffffffffUL) {
        if (!wena_ferretdb_table_name(collection, hash, name, sizeof(name))) return 0;
        if (!lookup(db, schema, "table_name", name, NULL, 0)) break;
    }
    if (strlen(name) >= capacity) return 0;
    if (!exec(db, "SAVEPOINT wena_ferretdb_collection")) return 0;
    sprintf(sql, "CREATE TABLE IF NOT EXISTS \"%s\".\"%s\" (_ferretdb_sjson TEXT NOT NULL "
                 "CHECK(_ferretdb_sjson != '')) STRICT", schema, name);
    ok = exec(db, sql);
    /* FerretDB's index text has "_id" in double quotes; let this one DDL
     * statement have it even where SQLite is built without them. */
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DDL, -1, &dqs);
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DDL, 1, NULL);
    sprintf(sql, "CREATE UNIQUE INDEX IF NOT EXISTS \"%s\".\"%s__id_\" ON \"%s\" (_ferretdb_sjson->\"_id\")",
            schema, name, name);
    ok = ok && exec(db, sql);
    sqlite3_db_config(db, SQLITE_DBCONFIG_DQS_DDL, dqs, NULL);
    uuid4(uuid);
    sprintf(sql, "INSERT INTO \"%s\".\"_ferretdb_collections\" (name, table_name, settings) VALUES (?1, ?2, ?3)",
            schema);
    if (ok && sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK) {
        char settings[320];
        sprintf(settings, "{\"uuid\":\"%s\",\"indexes\":[{\"name\":\"_id_\",\"key\":[{\"field\":\"_id\","
                          "\"descending\":false}],\"unique\":true}],\"cappedSize\":0,\"cappedDocuments\":0,"
                          "\"indexFormat\":2}", uuid);
        ok = sqlite3_bind_text(statement, 1, collection, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_bind_text(statement, 3, settings, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(statement) == SQLITE_DONE;
    } else ok = 0;
    sqlite3_finalize(statement);
    if (!ok) {
        exec(db, "ROLLBACK TO wena_ferretdb_collection");
        exec(db, "RELEASE wena_ferretdb_collection");
        return 0;
    }
    if (!exec(db, "RELEASE wena_ferretdb_collection")) return 0;
    strcpy(table, name);
    return 1;
}

/* One field set: its type in "$s".p, its value, and its name at the end of
 * "$k" when it is new. "parent.child" does the same in the parent object's
 * own "$s"; the parent must already be an object with fields. */
static int set_field(sqlite3 *db, const char *schema, const char *table, const char *id,
                     const WenaFerretField *field)
{
    char sql[1800], value_path[240], type_path[280], order_path[240], parent[110];
    const char *dot, *name;
    sqlite3_stmt *statement = NULL;
    int ok;
    if (!field_name(field->key) || field->element == NULL || field->value == NULL) return 0;
    dot = strchr(field->key, '.');
    if (dot == NULL) {
        sprintf(value_path, "$.\"%s\"", field->key);
        sprintf(type_path, "$.\"$s\".p.\"%s\"", field->key);
        strcpy(order_path, "$.\"$s\".\"$k\"");
        name = field->key;
    } else {
        memcpy(parent, field->key, (size_t)(dot - field->key));
        parent[dot - field->key] = '\0';
        name = dot + 1;
        sprintf(value_path, "$.\"%s\".\"%s\"", parent, name);
        sprintf(type_path, "$.\"$s\".p.\"%s\".\"$s\".p.\"%s\"", parent, name);
        sprintf(order_path, "$.\"$s\".p.\"%s\".\"$s\".\"$k\"", parent);
    }
    sprintf(sql,
        "UPDATE \"%s\".\"%s\" SET _ferretdb_sjson = CASE "
        "WHEN json_type(_ferretdb_sjson, '%s') IS NULL THEN "
        "json_insert(json_set(_ferretdb_sjson, '%s', json(?2), '%s', json(?3)), '%s[#]', ?4) "
        "ELSE json_set(_ferretdb_sjson, '%s', json(?2), '%s', json(?3)) END "
        "WHERE _ferretdb_sjson->'_id' = json_quote(?1) AND json_type(_ferretdb_sjson, '%s') = 'array'",
        schema, table, type_path, type_path, value_path, order_path, type_path, value_path, order_path);
    if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) return 0;
    ok = sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_bind_text(statement, 2, field->element, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_bind_text(statement, 3, field->value, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_bind_text(statement, 4, name, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    return ok;
}

int wena_ferretdb_update(sqlite3 *db, const char *schema, const char *table, const char *id,
                         const WenaFerretField *fields, size_t count)
{
    size_t index;
    if (db == NULL || !plain(schema) || !plain(table) || id == NULL || (count > 0 && fields == NULL)) return 0;
    if (!exec(db, "SAVEPOINT wena_ferretdb_update")) return 0;
    for (index = 0; index < count; ++index) {
        if (!set_field(db, schema, table, id, &fields[index])) {
            exec(db, "ROLLBACK TO wena_ferretdb_update");
            exec(db, "RELEASE wena_ferretdb_update");
            return 0;
        }
    }
    return exec(db, "RELEASE wena_ferretdb_update");
}

int wena_ferretdb_unset(sqlite3 *db, const char *schema, const char *table, const char *id,
                        const char *const *keys, size_t count)
{
    char sql[1024];
    sqlite3_stmt *statement;
    size_t index;
    int ok = 1;
    if (db == NULL || !plain(schema) || !plain(table) || id == NULL || (count > 0 && keys == NULL)) return 0;
    if (!exec(db, "SAVEPOINT wena_ferretdb_unset")) return 0;
    for (index = 0; index < count && ok; ++index) {
        /* Top-level fields only. */
        if (!field_name(keys[index]) || strchr(keys[index], '.') != NULL || strcmp(keys[index], "_id") == 0) {
            ok = 0;
            break;
        }
        sprintf(sql,
            "UPDATE \"%s\".\"%s\" SET _ferretdb_sjson = json_set(json_remove(_ferretdb_sjson, '$.\"%s\"', "
            "'$.\"$s\".p.\"%s\"'), '$.\"$s\".\"$k\"', json((SELECT json_group_array(value) FROM "
            "json_each(_ferretdb_sjson, '$.\"$s\".\"$k\"') WHERE value <> ?2))) "
            "WHERE _ferretdb_sjson->'_id' = json_quote(?1)", schema, table, keys[index], keys[index]);
        statement = NULL;
        ok = sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK &&
             sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_bind_text(statement, 2, keys[index], -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
        sqlite3_finalize(statement);
    }
    if (!ok) {
        exec(db, "ROLLBACK TO wena_ferretdb_unset");
        exec(db, "RELEASE wena_ferretdb_unset");
        return 0;
    }
    return exec(db, "RELEASE wena_ferretdb_unset");
}

int wena_ferretdb_insert(sqlite3 *db, const char *schema, const char *table, const char *id,
                         const WenaFerretField *fields, size_t count)
{
    char sql[512];
    sqlite3_stmt *statement = NULL;
    int ok;
    if (db == NULL || !plain(schema) || !plain(table) || id == NULL || id[0] == '\0') return 0;
    if (!exec(db, "SAVEPOINT wena_ferretdb_insert")) return 0;
    sprintf(sql, "INSERT INTO \"%s\".\"%s\" (_ferretdb_sjson) VALUES "
                 "('{\"$s\":{\"p\":{\"_id\":{\"t\":\"string\"}},\"$k\":[\"_id\"]},\"_id\":' || json_quote(?1) || '}')",
            schema, table);
    ok = sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK &&
         sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (ok) {
        size_t index;
        for (index = 0; index < count && ok; ++index)
            ok = set_field(db, schema, table, id, &fields[index]);
    }
    if (!ok) {
        exec(db, "ROLLBACK TO wena_ferretdb_insert");
        exec(db, "RELEASE wena_ferretdb_insert");
        return 0;
    }
    return exec(db, "RELEASE wena_ferretdb_insert");
}

int wena_ferretdb_delete(sqlite3 *db, const char *schema, const char *table, const char *id)
{
    char sql[256];
    sqlite3_stmt *statement = NULL;
    int ok;
    if (db == NULL || !plain(schema) || !plain(table) || id == NULL) return 0;
    sprintf(sql, "DELETE FROM \"%s\".\"%s\" WHERE _ferretdb_sjson->'_id' = json_quote(?1)", schema, table);
    ok = sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK &&
         sqlite3_bind_text(statement, 1, id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(db) == 1;
    sqlite3_finalize(statement);
    return ok;
}

sqlite3_int64 wena_ferretdb_now_ms(void)
{
    return (sqlite3_int64)time(NULL) * 1000;
}
