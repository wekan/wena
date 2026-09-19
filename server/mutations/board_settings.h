#ifndef WENA_MUTATION_BOARD_SETTINGS_H
#define WENA_MUTATION_BOARD_SETTINGS_H
#include "common.h"
#include <sqlite3.h>
/* Existing guarded writer transaction owns commit, rollback and idempotency.
 * Returns 0 failure, 1 change or 2 guarded no-op. */
int wena_sqlite_board_settings_change(sqlite3 *database,
    const WenaDomainCommand *command,const char *board_id,
    unsigned long *result_version);
#endif
