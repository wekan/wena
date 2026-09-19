#ifndef WENA_MUTATION_LABELS_H
#define WENA_MUTATION_LABELS_H
#include "common.h"
#include <sqlite3.h>
/* Runs within sqlite_persistence's actor/idempotency/writer transaction.
 * Returns 0 failure, 1 changed, 2 guarded no-op. */
int wena_sqlite_labels_change(sqlite3 *database,const WenaDomainCommand *command,
    const char *board_id,unsigned long *result_version);
#endif
