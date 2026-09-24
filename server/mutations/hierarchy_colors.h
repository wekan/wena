#ifndef WENA_MUTATION_HIERARCHY_COLORS_H
#define WENA_MUTATION_HIERARCHY_COLORS_H
#include "common.h"
#include <sqlite3.h>
#include "../../models/color.h"
/* Strict shared reader: exact parent revision; output is unchanged on failure.
 * lists is 1 for lists, 0 for swimlanes. Caller owns snapshot consistency. */
int wena_sqlite_hierarchy_color_read(sqlite3 *database,const char *board_id,
    const char *parent_id,int lists,unsigned long expected_version,
    char color[WENA_COLOR_CAPACITY]);
/* Caller owns the guarded transaction. 0 failure, 1 changed, 2 unchanged. */
int wena_sqlite_hierarchy_color_change(sqlite3 *database,const WenaDomainCommand *command,
    const char *board_id,unsigned long *result_version);
#endif
