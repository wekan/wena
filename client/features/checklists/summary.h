#ifndef WENA_CHECKLIST_SUMMARY_H
#define WENA_CHECKLIST_SUMMARY_H

#include "../../../models/checklist.h"
#include "../../../server/sqlite_board.h"

typedef struct WenaChecklistCardSummary {
    WenaId card_id;
    unsigned long card_version;
    int archived;
    size_t checklist_count;
    /* Aggregate card counts ignore per-checklist display flags. */
    WenaChecklistProgress progress;
} WenaChecklistCardSummary;

typedef struct WenaChecklistBoardSummary {
    WenaId board_id;
    unsigned long board_version;
    int enabled;
    size_t card_count;
    WenaChecklistCardSummary cards[WENA_SQLITE_BOARD_MAX_CARDS];
} WenaChecklistBoardSummary;

WenaChecklistBoardSummary *wena_checklist_summary_create(void);
void wena_checklist_summary_free(WenaChecklistBoardSummary *summary);
/* Enabled is an explicit 0/1 opt-in; the canonical default is 0. Disabled loads
 * clear the snapshot without SQL or allocation (database may be NULL). Enabled
 * loads own one read transaction, include archived cards, validate all rows
 * reachable from actual board cards/checklists, and publish only after COMMIT.
 * Refuses caller-owned transactions. Any failure leaves output unchanged.
 * Read once at initial enable and after committed edits, never per frame.
 * Caller supplies authorization; actor existence is only a local scope check. */
int wena_checklist_summary_load(sqlite3 *database, const char *actor_id,
    const char *board_id, int enabled, WenaChecklistBoardSummary *output);
/* Snapshot is immutable until refreshed; stored card_version lets callers
 * detect a known stale cache. A card with empty checklist(s) has a 0/0 badge;
 * no checklist means no badge. The lookup performs no database operation. */
const WenaChecklistCardSummary *wena_checklist_summary_find(
    const WenaChecklistBoardSummary *summary, const char *card_id);

#endif
