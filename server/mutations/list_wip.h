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
/* Guarded transaction required. Returns 0 failure, 1 changed, 2 unchanged. */
int wena_sqlite_list_wip_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version);
#endif
