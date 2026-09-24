#ifndef WENA_MUTATION_SWIMLANE_ARCHIVE_H
#define WENA_MUTATION_SWIMLANE_ARCHIVE_H
#include "common.h"
#include <sqlite3.h>
/* Caller owns guarded transaction. 0 failure, 1 changed, 2 unchanged. */
int wena_sqlite_swimlane_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version);
int wena_sqlite_list_cards_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version);
int wena_sqlite_selected_cards_archive_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version);
/* Caller-owned read/write transaction; strict active-card and parent metadata
 * validation. Failure preserves the output revision. */
int wena_sqlite_card_archive_version(sqlite3 *db,const char *board,const char *id,unsigned long *version);
#endif
