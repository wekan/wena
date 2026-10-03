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

/* A board on WeKan's All Boards page, read from WeKan's documents. */
#define WENA_WEKAN_TILE_CAPACITY 512
typedef struct WenaWekanBoardTile {
    char id[65];
    char title[129];
    char color[33];       /* WeKan's board color: belize, nephritis ... */
    int archived;
    int starred;          /* in the user's profile.starredBoards */
    int template_board;   /* WeKan's template container */
    int openable;         /* a board Wena has read in and can show */
} WenaWekanBoardTile;
/* The boards `actor` is an active member of - WeKan's own and template
 * containers, archived or not, without WeKan's helper boards - by title. */
int wena_wekan_sync_boards(sqlite3 *db, const char *actor, WenaWekanBoardTile *tiles, size_t capacity,
                           size_t *count);

/* Stars or unstars a board for `actor`: WeKan's users.profile.starredBoards. */
int wena_wekan_sync_star(sqlite3 *db, const char *actor, const char *board, int starred);
/* WeKan's header star group for `board`: whether `actor` starred it, how
 * many places the user keeps starred (boards, pages, swimlanes, lists and
 * cards, as users.starredCount) and the board's own `stars` counter. */
int wena_wekan_sync_starred(sqlite3 *db, const char *actor, const char *board, int *starred, int *count,
                            int *board_stars);
/* WeKan's board visibility (boards.permission: "private" when not set) and
 * `actor`'s watch level on it (boards.watchers, "muted" when not there), as
 * the header's Private and Muted buttons show them. Each is at most 15
 * characters. */
int wena_wekan_sync_board_state(sqlite3 *db, const char *actor, const char *board, char *permission,
                                char *watch);
/* Sets them as WeKan does: permission "private" or "public"; the watch level
 * "watching", "tracking" or "muted" - muted, WeKan's default, removes the
 * user from boards.watchers. Other values are refused. */
int wena_wekan_sync_set_permission(sqlite3 *db, const char *board, const char *permission);
int wena_wekan_sync_set_watch(sqlite3 *db, const char *actor, const char *board, const char *level);
/* A new board as WeKan makes one - with its "Default" swimlane, `actor` its
 * admin - in Wena's tables and written to WeKan's file. Writes its _id. */
int wena_wekan_sync_new_board(sqlite3 *db, const char *actor, const char *title, char *board, size_t capacity);

/* The user's language as WeKan keeps it (users.profile.language); 0 when
 * not set. Setting writes it there. */
int wena_wekan_sync_language(sqlite3 *db, const char *actor, char *language, size_t capacity);
int wena_wekan_sync_set_language(sqlite3 *db, const char *actor, const char *language);

/* A per-board map in the user's profile, as WeKan keeps collapsed lists and
 * swimlanes and swimlane heights: profile.<field>.<board>.<id> = value.
 * Reading calls `entry` with each id and its value (true is 1); writing
 * replaces this board's map with `map` (a JSON object), the other boards'
 * staying as they are. */
int wena_wekan_sync_profile_board_map(sqlite3 *db, const char *actor, const char *field, const char *board,
                                      void (*entry)(void *context, const char *id, int value), void *context);
int wena_wekan_sync_set_profile_board_map(sqlite3 *db, const char *actor, const char *field, const char *board,
                                          const char *map);

/* What the last failed statement said, for the debug log. */
const char *wena_wekan_sync_error(void);

#endif
