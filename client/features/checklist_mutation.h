#ifndef WENA_CHECKLIST_MUTATION_H
#define WENA_CHECKLIST_MUTATION_H
#include "checklist_store.h"
#include "../../server/sqlite_persistence.h"
typedef struct WenaChecklistMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
} WenaChecklistMutation;
/* Heap allocation recommended: the bounded snapshot is intentionally large. */
WenaChecklistSnapshot *wena_checklist_snapshot_create(void);
void wena_checklist_snapshot_free(WenaChecklistSnapshot *snapshot);
int wena_checklist_mutation_init(WenaChecklistMutation *adapter,sqlite3 *database,
    const char *actor_id,const char *board_id);
/* Complete active-card snapshot, atomic output; requires schema v3. */
int wena_checklist_mutation_load(void *context,const char *board_id,
    const char *card_id,WenaChecklistSnapshot *snapshot);
/* Every real change increments card.version. Identical values are guarded
 * no-ops without version, timestamp or request writes. Item edits increment their
 * item version; item creation/deletion increments checklist.version.
 * Checklist deletion removes its children first and increments only card.version.
 * Deletes are permanent and missing targets reject (there is no undo API).
 * Replays reject. Reload after success; no snapshot is mutated by save. */
int wena_checklist_mutation_save(void *context,const char *board_id,
    const char *card_id,const WenaChecklistEdit *edit);
int wena_checklist_mutation_save_request(WenaChecklistMutation *adapter,
    const char *board_id,const char *card_id,const WenaChecklistEdit *edit,
    unsigned long request_version);
/* Consume once after drawing: 1 saved, 0 no intent, -1 rejected. Failed saves
 * require refreshing the snapshot; they are never retried by another frame. */
int wena_checklist_mutation_complete(WenaChecklistMutation *adapter,
    WenaChecklistCompletionIntent *intent);
#endif
