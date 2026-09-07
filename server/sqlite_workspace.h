#ifndef WENA_SQLITE_WORKSPACE_H
#define WENA_SQLITE_WORKSPACE_H

#include <stddef.h>

#define WENA_SQLITE_WORKSPACE_TITLE_CAPACITY 129u

typedef struct WenaSqliteWorkspaceSeed {
    const char *actor_id;
    const char *actor_name;
    const char *board_id;
    const char *board_title;
    const char *swimlane_id;
    const char *swimlane_title;
    const char *list_id;
    const char *list_title;
} WenaSqliteWorkspaceSeed;

/* Explicit local-workspace creation, not server authorization or WeKan import.
 * Requires an absolute destination in a caller-selected trusted directory.
 * IDs: 1..64 ASCII letters/digits/_/-. Actor name: 1..256 UTF-8 bytes.
 * Board/list/swimlane titles: 1..128 UTF-8 bytes, matching native editors
 * and mutation adapters. All text rejects control characters.
 * Uses the existing exact migration gate, verifies integrity, and atomically
 * publishes a mode-0600 complete database without replacing any existing path.
 * POSIX hard-link publication only; unsupported platforms/filesystems fail.
 * Returns 1 only after publication. No database connection is left open.
 * Parent-directory power-loss durability is not a guaranteed capability here. */
int wena_sqlite_workspace_create(const char *path,
                                  const unsigned char *migration, size_t length,
                                  const char *expected_sha256,
                                  const WenaSqliteWorkspaceSeed *seed);
#endif
