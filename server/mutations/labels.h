#ifndef WENA_MUTATION_LABELS_H
#define WENA_MUTATION_LABELS_H
#include "common.h"
#include <sqlite3.h>
/* Runs within sqlite_persistence's actor/idempotency/writer transaction.
 * Returns 0 failure, 1 changed, 2 guarded no-op. */
int wena_sqlite_labels_change(sqlite3 *database,const WenaDomainCommand *command,
    const char *board_id,unsigned long *result_version);
int wena_sqlite_selected_labels_change(sqlite3 *database,const WenaDomainCommand *command,
    const char *board_id,unsigned long *result_version);
/* Caller-owned guarded cross-board transaction. Map exact nonempty source
 * label names to destination IDs; preserve both catalogues and verify the final
 * assignment set. Caller moves the card and advances card/board revisions,
 * commits on success or rolls back the entire transaction on failure. FK
 * enforcement is deferred until commit, never disabled. 1 success, 0 failure. */
int wena_sqlite_card_labels_reboard(sqlite3 *database,const char *board,
    const char *card,const char *target_board);
#endif
