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
/* Reboard every checklist/item owned by one card inside the caller's guarded
 * transaction. Preserve content/order/preferences; advance each row revision
 * and monotonic update time. The caller must move the card to target_board
 * before committing, and roll back the entire transaction on any failure.
 * FK enforcement is deferred until commit, never disabled. No actor/replay or
 * card mutation is performed here. Returns 1 success, 0 failure. */
int wena_sqlite_card_checklists_reboard(sqlite3 *database,const char *board,
    const char *card,const char *target_board);
#endif
