#ifndef WENA_LABEL_STORE_H
#define WENA_LABEL_STORE_H
#include "../../../models/label.h"

typedef enum WenaLabelAction {
    WENA_LABEL_CREATE=1,
    WENA_LABEL_EDIT=2,
    WENA_LABEL_DELETE=3,
    WENA_LABEL_ASSIGN=4,
    WENA_LABEL_UNASSIGN=5
} WenaLabelAction;
typedef struct WenaLabelEdit {
    WenaLabelAction action;
    const char *label_id;
    unsigned long expected_board_version;
    unsigned long expected_label_version;
    unsigned long expected_card_version;
    const char *name;
    const char *color;
} WenaLabelEdit;
typedef struct WenaLabelSnapshot {
    WenaId board_id;
    WenaId card_id;
    unsigned long board_version;
    unsigned long card_version;
    size_t label_count;
    WenaLabel labels[WENA_BOARD_LABEL_CAPACITY];
    unsigned long label_versions[WENA_BOARD_LABEL_CAPACITY];
    int assigned[WENA_BOARD_LABEL_CAPACITY];
    unsigned long assigned_card_counts[WENA_BOARD_LABEL_CAPACITY];
} WenaLabelSnapshot;
#define WENA_LABEL_BOARD_CARD_CAPACITY 2048u
#define WENA_LABEL_ASSIGNMENT_BYTES ((WENA_BOARD_LABEL_CAPACITY+7u)/8u)
typedef struct WenaLabelBoardSnapshot {
    WenaLabelSnapshot catalogue;
    size_t card_count;
    /* Stable IDs in strict BINARY order, including archived cards. */
    WenaId card_ids[WENA_LABEL_BOARD_CARD_CAPACITY];
    unsigned long card_versions[WENA_LABEL_BOARD_CARD_CAPACITY];
    unsigned char assignments[WENA_LABEL_BOARD_CARD_CAPACITY][WENA_LABEL_ASSIGNMENT_BYTES];
} WenaLabelBoardSnapshot;
WenaLabelBoardSnapshot *wena_label_board_snapshot_create(void);
void wena_label_board_snapshot_free(WenaLabelBoardSnapshot *snapshot);
int wena_label_board_snapshot_valid(const WenaLabelBoardSnapshot *snapshot,const char *board_id);
/* Lookup uses binary search on a validated snapshot; absent returns card_count. */
size_t wena_label_board_card_index(const WenaLabelBoardSnapshot *snapshot,const char *card_id);
const unsigned char *wena_label_board_assignments(const WenaLabelBoardSnapshot *snapshot,
    const char *board_id,const char *card_id);
/* NULL or empty card_id denotes the board label manager. */
int wena_label_snapshot_valid(const WenaLabelSnapshot *snapshot,
    const char *board_id,const char *card_id);
WenaLabelSnapshot *wena_label_snapshot_create(void);
void wena_label_snapshot_free(WenaLabelSnapshot *snapshot);
#endif
