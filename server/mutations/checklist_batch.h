#ifndef WENA_SQLITE_CHECKLIST_BATCH_H
#define WENA_SQLITE_CHECKLIST_BATCH_H
#include "../domain_operation.h"
#include <sqlite3.h>
/* Internal guarded transaction operation: the caller owns BEGIN, actor/replay
 * validation, response metadata, COMMIT and full rollback on any failure. */
int wena_sqlite_checklist_batch(sqlite3 *database,
    const WenaDomainCommand *command, const char *board_id,
    unsigned long *result_version);
#endif
