#ifndef WENA_COLLAPSE_PREFERENCES_H
#define WENA_COLLAPSE_PREFERENCES_H
#include "../../client/components/boards/board_layout.h"
#include "../../server/executable_path.h"
#define WENA_COLLAPSE_PREFS_ERROR 0
#define WENA_COLLAPSE_PREFS_OK 1
#define WENA_COLLAPSE_PREFS_MISSING 2
/* Database directory + SHA256(length-prefixed workspace/actor/board) filename.
 * Exact workspace spelling binds identity; caller supplies validated DB path.
 * Failure leaves output unchanged. Parent directory must be trusted locally. */
int wena_collapse_preferences_path(const char *workspace, const char *actor,
    const char *board, char *output, size_t capacity);
/* Load is read-only. Missing/error leaves state unchanged. After loading, call
 * board_collapse_sync against the COMPLETE snapshot to prune obsolete IDs. */
int wena_collapse_preferences_load(const char *path, const char *workspace,
    const char *actor, const char *board, WenaBoardCollapseState *state);
/* Existing malformed or differently scoped files are never overwritten.
 * Fixed .tmp collisions (including symlinks) are neither removed nor changed.
 * Atomic last-successful-write semantics; no shared/remote preference claim. */
int wena_collapse_preferences_save(const char *path, const char *workspace,
    const char *actor, const WenaBoardCollapseState *state);
/* Write an empty scoped preference atomically; state changes only on success. */
int wena_collapse_preferences_reset(const char *path, const char *workspace,
    const char *actor, const char *board, WenaBoardCollapseState *state);
#endif
