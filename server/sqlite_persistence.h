#ifndef WENA_SERVER_SQLITE_PERSISTENCE_H
#define WENA_SERVER_SQLITE_PERSISTENCE_H
#include "domain_operation.h"
#include <sqlite3.h>
typedef struct WenaSqlitePersistence {
    sqlite3 *database;
    /* Result of this call's committed create only; cleared for every apply.
     * Lets native caches publish the exact row without a fallible later query. */
    char created_card_id[65];
    double created_card_position;
    /* Exact committed move position; never requires a post-commit query. */
    double moved_card_position;
    /* Committed list/swimlane creation only; empty on failure/other calls. */
    char created_hierarchy_id[65];
    double created_hierarchy_position;
} WenaSqlitePersistence;
void wena_sqlite_persistence_init(WenaSqlitePersistence *store, sqlite3 *database);
int wena_sqlite_persistence_apply(void *context, const WenaDomainCommand *command,
                                  WenaRegionResponse *response);
#endif
