#ifndef WENA_SQLITE_LIST_STATE_H
#define WENA_SQLITE_LIST_STATE_H
#include <sqlite3.h>
/* Shared strict archive metadata reader. Missing pre-v10 tables are supported;
 * malformed or missing current-schema tables fail. Read inside caller's transaction.
 * A zero version accepts any valid persisted revision. */
int wena_sqlite_list_state_available(sqlite3 *database);
int wena_sqlite_list_state_read(sqlite3 *database,const char *board,const char *list,
    unsigned long version,int *archived,sqlite3_int64 *archived_at);
int wena_sqlite_list_active(sqlite3 *database,const char *board,const char *list);
#endif
