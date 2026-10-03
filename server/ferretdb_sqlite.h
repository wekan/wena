#ifndef WENA_SERVER_FERRETDB_SQLITE_H
#define WENA_SERVER_FERRETDB_SQLITE_H
/* FerretDB v1's SQLite storage, written directly with SQL, so a file Wena
 * writes is the one FerretDB - and so WeKan's FerretDB bundles - read, and
 * the other way round (internal/backends/sqlite of the wekan/FerretDB fork):
 *
 *   _ferretdb_collections(name, table_name, settings)    one row per collection
 *   "<name lowercased>_<FNV-1a 32 of name, %08x>"(_ferretdb_sjson TEXT) STRICT
 *   UNIQUE INDEX "<table>__id_" ON (_ferretdb_sjson->"_id")
 *
 * A document is JSON with its BSON types in "$s": {"p": {field: {"t": type}},
 * "$k": [field order]}, e.g. {"$s":{"p":{"_id":{"t":"string"}},"$k":["_id"]},
 * "_id":"abc"}. Dates are milliseconds with {"t":"date"}. Every function works
 * on a schema of the connection ("main", or the name a file is attached as). */
#include <sqlite3.h>
#include <stddef.h>

#define WENA_FERRETDB_TABLE_CAPACITY 192

/* FerretDB's table name for a collection with the hash `hash` (the FNV-1a of
 * the name, one more for each name before it that already had the table). */
unsigned long wena_ferretdb_fnv1a(const char *text);
int wena_ferretdb_table_name(const char *collection, unsigned long hash, char *out, size_t capacity);

/* Creates the metadata table when the database is new. */
int wena_ferretdb_prepare(sqlite3 *db, const char *schema);
/* The table of a collection; with `create`, makes it as FerretDB does (table,
 * _id_ index, settings with a new UUID) when it does not exist yet. Returns 1
 * found or made, 0 missing (without create) or failed. */
int wena_ferretdb_collection(sqlite3 *db, const char *schema, const char *collection, int create,
                             char *table, size_t capacity);

/* One field of a document: its BSON type element of "$s" ({"t":"string"},
 * {"t":"array","i":[...]}, ...) and its value, both as JSON text. */
typedef struct WenaFerretField {
    const char *key;
    const char *element;
    const char *value;
} WenaFerretField;

/* Sets fields of the document with this _id (a string), adding a field that
 * is not there yet to "$s" and the end of "$k"; other fields stay as they
 * are. Returns 1 when the document exists and was written. */
int wena_ferretdb_update(sqlite3 *db, const char *schema, const char *table, const char *id,
                         const WenaFerretField *fields, size_t count);
/* Removes fields of the document, from "$s", "$k" and the values. */
int wena_ferretdb_unset(sqlite3 *db, const char *schema, const char *table, const char *id,
                        const char *const *keys, size_t count);
/* A new document: "_id" (a string) first, then the fields in order. */
int wena_ferretdb_insert(sqlite3 *db, const char *schema, const char *table, const char *id,
                         const WenaFerretField *fields, size_t count);
int wena_ferretdb_delete(sqlite3 *db, const char *schema, const char *table, const char *id);

/* JSON element strings for the common BSON types. */
#define WENA_FERRET_STRING "{\"t\":\"string\"}"
#define WENA_FERRET_BOOL "{\"t\":\"bool\"}"
#define WENA_FERRET_DATE "{\"t\":\"date\"}"
#define WENA_FERRET_INT "{\"t\":\"int\"}"
#define WENA_FERRET_DOUBLE "{\"t\":\"double\"}"
#define WENA_FERRET_NULL "{\"t\":\"null\"}"

/* Milliseconds since 1970, as WeKan's dates are stored. */
sqlite3_int64 wena_ferretdb_now_ms(void);

#endif
