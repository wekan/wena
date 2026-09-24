#ifndef WENA_LABEL_MUTATION_H
#define WENA_LABEL_MUTATION_H
#include "store.h"
#include "../../../server/sqlite_persistence.h"

typedef struct WenaLabelMutation {
    WenaSqlitePersistence persistence;
    WenaId actor_id;
    WenaId board_id;
    char route[257];
} WenaLabelMutation;
int wena_label_mutation_init(WenaLabelMutation *adapter,sqlite3 *database,
    const char *actor_id,const char *board_id);
/* Complete, atomic board catalogue plus optional active-card assignments. */
int wena_label_mutation_load(void *context,const char *board_id,
    const char *card_id,WenaLabelSnapshot *snapshot);
/* Complete bounded board badge data, including archived cards. Atomic output;
 * no renderer SQL is needed. Invalidate cached use visibly after a load failure. */
int wena_label_mutation_load_board(void *context,const char *board_id,
    WenaLabelBoardSnapshot *snapshot);
/* All changes advance board.version. Assign/unassign also advance card.version;
 * deletion unlinks all affected cards, including archived, and advances each.
 * Edit checks/increments label.version. Scope/version-guarded identical edits and
 * assignment states succeed without writes; duplicate new definitions reject.
 * Committed request replays reject. Reload the complete snapshot after success. */
int wena_label_mutation_save(void *context,const char *board_id,
    const char *card_id,const WenaLabelEdit *edit);
int wena_label_mutation_save_request(WenaLabelMutation *adapter,
    const char *board_id,const char *card_id,const WenaLabelEdit *edit,
    unsigned long request_version);
/* Read catalogue, exact active-card revisions and mixed assignment counts in
 * one snapshot. *output starts NULL or is owned by this API; success replaces it,
 * failure preserves pointer and bytes. Release with free when closing the UI. */
int wena_label_mutation_selected_load(void *context,const char *board,const WenaId *ids,
    size_t count,WenaLabelSelectionSnapshot **output);
/* Stage complete badge cache inside the guarded write transaction, publish only
 * after commit. Failure preserves capture/output. No fallible post-commit reload. */
int wena_label_mutation_selected_save_request(WenaLabelMutation *adapter,const char *board,
    const WenaLabelSelectionSnapshot *selection,const char *label,int assign,
    WenaLabelBoardSnapshot *output,unsigned long request);
int wena_label_mutation_selected_save(void *context,const char *board,
    const WenaLabelSelectionSnapshot *selection,const char *label,int assign,WenaLabelBoardSnapshot *output);
#endif
