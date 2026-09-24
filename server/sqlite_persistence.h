#ifndef WENA_SERVER_SQLITE_PERSISTENCE_H
#define WENA_SERVER_SQLITE_PERSISTENCE_H
#include "domain_operation.h"
#include "sha256.h"
#include "../models/wip_limit.h"
#include <sqlite3.h>
typedef struct WenaSqlitePersistence {
    sqlite3 *database;
    /* Optional read-only staging hook, run inside the transaction just before
     * commit (including no-ops). Failure rolls back; caller publishes staged
     * data only when apply succeeds. Never commit/rollback from this hook. */
    int (*prepare_publish)(void *context,sqlite3 *database);
    void *publish_context;
    /* Result of this call's committed create only; cleared for every apply.
     * Lets native caches publish the exact row without a fallible later query. */
    char created_card_id[65];
    double created_card_position;
    /* Exact committed move position; never requires a post-commit query. */
    double moved_card_position;
    /* Committed list/swimlane creation only; empty on failure/other calls. */
    char created_hierarchy_id[65];
    double created_hierarchy_position;
    /* Exact committed WIP edit, including no-op; zero on failure/other calls. */
    WenaWipLimit list_wip_result;
} WenaSqlitePersistence;
void wena_sqlite_persistence_init(WenaSqlitePersistence *store, sqlite3 *database);
int wena_sqlite_persistence_apply(void *context, const WenaDomainCommand *command,
                                  WenaRegionResponse *response);
/* Shared ordered-ID fingerprint serialization for native hierarchy snapshots.
 * Initialize/finalize with SHA-256; invalid IDs leave the state untouched. */
int wena_sqlite_hierarchy_order_add(WenaSha256 *state, const char *id, size_t length);
/* Card-column fingerprint additionally includes each exact persisted position. */
int wena_sqlite_card_order_add(WenaSha256 *state, const char *id,
    size_t length, unsigned long position);
#endif
