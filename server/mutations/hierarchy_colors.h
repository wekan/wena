#ifndef WENA_MUTATION_HIERARCHY_COLORS_H
#define WENA_MUTATION_HIERARCHY_COLORS_H
#include "common.h"
#include <sqlite3.h>
/* Caller owns the guarded transaction. 0 failure, 1 changed, 2 unchanged. */
int wena_sqlite_hierarchy_color_change(sqlite3 *database,const WenaDomainCommand *command,
    const char *board_id,unsigned long *result_version);
#endif
