#ifndef WENA_SERVER_WEKAN_SYNC_H
#define WENA_SERVER_WEKAN_SYNC_H
/* WeKan's documents in FerretDB's wekan.sqlite, and Wena's own tables.
 *
 * Wena works on its relational tables (server/migrations) in a working
 * database; the file on disk is only WeKan's: wekan-files/db/wekan.sqlite as
 * FerretDB writes it, attached to the working connection as "fdb". Import
 * reads WeKan's collections into the tables; export writes back what Wena
 * changed since - only the fields that changed, each with its BSON type, so
 * the other fields WeKan keeps in a document stay as they were - and new
 * documents with the fields WeKan requires.
 *
 *   users -> actors         boards -> boards, board_members, labels, settings
 *   swimlanes, lists        with their archive state, color and WIP limit
 *   cards -> cards, descriptions, labels (labelIds), people, archive state
 *   checklists, checklistItems
 */
#include <sqlite3.h>
#include <stddef.h>

#define WENA_WEKAN_SCHEMA "fdb"

/* Registers wena_title() and wena_sjson_element() on the connection. */
int wena_wekan_sync_functions(sqlite3 *db);
/* Attaches the FerretDB file as WENA_WEKAN_SCHEMA and makes its metadata
 * table and WeKan's collections when they are not there yet. */
int wena_wekan_sync_attach(sqlite3 *db, const char *path);
/* Reads WeKan's documents into Wena's empty tables, then remembers what was
 * read, so the first export writes nothing. */
int wena_wekan_sync_import(sqlite3 *db);
/* Writes what changed in Wena's tables since the import or the last export
 * to WeKan's documents, as `actor` (the user of new documents). Returns the
 * number of documents written, -1 on failure (nothing is written then). */
int wena_wekan_sync_export(sqlite3 *db, const char *actor);
/* The user to work as: `wanted` (a user's _id or username) when given and
 * present, else the first admin, else the first user; a file without users
 * gets one, "admin" as WeKan's first registered user is. Writes its _id. */
int wena_wekan_sync_user(sqlite3 *db, const char *wanted, char *id, size_t capacity);

/* What the last failed statement said, for the debug log. */
const char *wena_wekan_sync_error(void);

#endif
