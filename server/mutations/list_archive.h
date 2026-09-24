#ifndef WENA_MUTATION_LIST_ARCHIVE_H
#define WENA_MUTATION_LIST_ARCHIVE_H
#include "common.h"
#include <sqlite3.h>
/* Ordinary lists only: child card flags/positions are never changed.
 * Caller owns the guarded transaction. Returns 0 failure, 1 change, 2 no-op. */
int wena_sqlite_list_archive_change(sqlite3 *database,const WenaDomainCommand *command,
    const char *board_id,unsigned long *result_version);
#endif
