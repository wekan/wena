#ifndef WENA_MUTATION_LIST_WIP_H
#define WENA_MUTATION_LIST_WIP_H
#include "common.h"
#include "../../models/wip_limit.h"
#include <sqlite3.h>
/* Caller owns transaction/snapshot consistency. Outputs survive failure. */
int wena_sqlite_list_wip_read(sqlite3 *db,const char *board,const char *list,
    unsigned long expected,WenaWipLimit *output);
int wena_sqlite_list_wip_count(sqlite3 *db,const char *board,const char *list,
    size_t *output);
/* Guard one card's destination within the caller's write transaction.
 * increase is 0 for a move within the same list, 1 for an incoming card.
 * applied is 1 only after that incoming card has been written (create/restore).
 * Legacy schemas without WIP settings retain unlimited behavior. */
int wena_sqlite_list_wip_check(sqlite3 *db,const char *board,const char *list,
    int increase,int applied);
/* Guarded transaction required. Returns 0 failure, 1 changed, 2 unchanged. */
int wena_sqlite_list_wip_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version,WenaWipLimit *result_limit);
#endif
