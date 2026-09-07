#ifndef WENA_HIERARCHY_MOVE_MUTATION_H
#define WENA_HIERARCHY_MOVE_MUTATION_H
#include "hierarchy_title.h"
#include "../../server/sqlite_board.h"
#include "../../server/sqlite_persistence.h"

typedef struct WenaHierarchyMoveMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
    WenaSqliteBoardSnapshot *snapshot;
} WenaHierarchyMoveMutation;

/* Caller authenticates actor/authorizes board. Arrays must contain the complete
 * board hierarchy in contiguous position order 0..count-1. Unknown, oversized,
 * noncontiguous or concurrently reordered data fails closed, never normalized.
 * A same-position move checks scope/version/order but changes no row/key/cache.
 * Actual moves reorder the cache only after commit; other collections retain
 * their order. Database and snapshot must outlive the adapter. */
int wena_hierarchy_move_mutation_init(WenaHierarchyMoveMutation *adapter,
    sqlite3 *database, const char *actor, const char *board,
    WenaSqliteBoardSnapshot *snapshot);
int wena_hierarchy_move_mutation_load(void *context, const char *board,
    WenaHierarchyKind kind, const char *id, unsigned long *version,
    unsigned long *position);
int wena_hierarchy_move_mutation_move(void *context, const char *board,
    WenaHierarchyKind kind, const char *id, unsigned long expected_version,
    unsigned long target_position);
int wena_hierarchy_move_mutation_move_request(WenaHierarchyMoveMutation *adapter,
    const char *board, WenaHierarchyKind kind, const char *id,
    unsigned long expected_version, unsigned long request_version,
    unsigned long target_position);
#endif
