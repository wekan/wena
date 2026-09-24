#ifndef WENA_SQLITE_CHECKLIST_ORDER_H
#define WENA_SQLITE_CHECKLIST_ORDER_H
#include "../domain_operation.h"
#include <sqlite3.h>
/* Existing caller-owned guarded transaction: 0 failure,1 changed,2 no-op.
 * The caller owns actor/replay checks, metadata, commit and complete rollback. */
int wena_sqlite_checklist_order(sqlite3 *database,
    const WenaDomainCommand *command, const char *board_id,
    unsigned long *result_version);
/* Whole-checklist transfer, including every child, appends within the board. */
int wena_sqlite_checklist_move(sqlite3 *database,
    const WenaDomainCommand *command, const char *board_id,
    unsigned long *result_version);
int wena_sqlite_checklist_item_move(sqlite3 *database,
    const WenaDomainCommand *command, const char *board_id,
    unsigned long *result_version);
#endif
