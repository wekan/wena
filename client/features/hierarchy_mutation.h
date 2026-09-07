#ifndef WENA_HIERARCHY_MUTATION_H
#define WENA_HIERARCHY_MUTATION_H

#include "hierarchy_title.h"
#include "../../server/sqlite_board.h"
#include "../../server/sqlite_persistence.h"

typedef struct WenaHierarchyMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
    WenaSqliteBoardSnapshot *snapshot;
} WenaHierarchyMutation;

/* Caller authenticates actor and authorizes board. No implicit membership grant.
 * The database and caller-owned loader snapshot must outlive this adapter. */
int wena_hierarchy_mutation_init(WenaHierarchyMutation *adapter, sqlite3 *database,
    const char *actor_id, const char *board_id, WenaSqliteBoardSnapshot *snapshot);
int wena_hierarchy_mutation_load(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, char *title,
    size_t capacity, unsigned long *version);
int wena_hierarchy_mutation_save(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, unsigned long expected_version,
    const char *title);
/* A committed replay returns 0. Failed requests leave display models unchanged. */
int wena_hierarchy_mutation_save_request(WenaHierarchyMutation *adapter,
    const char *board_id, WenaHierarchyKind kind, const char *target_id,
    unsigned long expected_version, unsigned long request_version, const char *title);
/* List/swimlane creation preflights snapshot capacity and publishes the exact
 * committed generated ID/position without a fallible post-commit reload. */
int wena_hierarchy_mutation_create(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *title);
int wena_hierarchy_mutation_create_request(WenaHierarchyMutation *adapter,
    const char *board_id, WenaHierarchyKind kind, unsigned long request_version,
    const char *title);
#endif
