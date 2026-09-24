#ifndef WENA_SQLITE_DIRECTORY_H
#define WENA_SQLITE_DIRECTORY_H
#include "../models/directory.h"
#include <sqlite3.h>
/* Trusted local session: actor must exist; this does not grant remote access.
 * Stable ID order, bounded page only, count/rows in one read transaction. Clamp
 * a stale page to the final page. Success publishes after commit; failures leave
 * output untouched. Validate rows on the selected page, not the entire catalog. */
int wena_sqlite_directory_load(sqlite3 *database,const char *actor,
    WenaDirectoryKind kind,size_t page,size_t page_size,WenaDirectoryPage *output);
int wena_sqlite_directory_load_scoped(sqlite3 *database,const char *actor,
    WenaDirectoryKind kind,const char *board_id,size_t page,size_t page_size,
    WenaDirectoryPage *output);
typedef struct WenaSqliteDirectoryReader {
    sqlite3 *database;
    WenaId actor;
} WenaSqliteDirectoryReader;
int wena_sqlite_directory_reader_init(WenaSqliteDirectoryReader *reader,
    sqlite3 *database,const char *actor);
int wena_sqlite_directory_read(void *context,WenaDirectoryKind kind,const char *board_id,
    size_t page,size_t page_size,WenaDirectoryPage *output);
#endif
