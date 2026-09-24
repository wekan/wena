#ifndef WENA_CHECKLIST_STORE_H
#define WENA_CHECKLIST_STORE_H
#include "../../models/checklist_item.h"
#define WENA_CARD_CHECKLIST_CAPACITY WENA_CHECKLIST_MAX_PER_CARD
#define WENA_CARD_CHECKLIST_ITEM_CAPACITY WENA_CHECKLIST_MAX_ITEMS

typedef enum WenaChecklistAction {
    WENA_CHECKLIST_CREATE = 1,
    WENA_CHECKLIST_RENAME = 2,
    WENA_CHECKLIST_ADD_ITEM = 3,
    WENA_CHECKLIST_RENAME_ITEM = 4,
    WENA_CHECKLIST_SET_FINISHED = 5,
    WENA_CHECKLIST_SET_FLAGS = 6,
    WENA_CHECKLIST_DELETE = 7,
    WENA_CHECKLIST_DELETE_ITEM = 8,
    WENA_CHECKLIST_ADD_ITEMS = 9,
    WENA_CHECKLIST_REORDER = 10,
    WENA_CHECKLIST_REORDER_ITEM = 11,
    WENA_CHECKLIST_MOVE = 12
} WenaChecklistAction;
typedef struct WenaChecklistEdit {
    WenaChecklistAction action;
    const char *checklist_id;
    const char *item_id;
    unsigned long expected_card_version;
    unsigned long expected_checklist_version;
    unsigned long expected_item_version;
    const char *title;
    int is_finished;
    int hide_checked_items;
    int hide_all_items;
    WenaChecklistMinicard show_on_minicard;
    /* Borrowed raw UTF-8 LF-separated text for ADD_ITEMS only. */
    const char *batch_text;
    size_t batch_length;
    /* Zero-based destination ordinal for same-card ordering only. */
    unsigned long target_position;
    /* MOVE appends the whole checklist to a different active same-board card. */
    const char *target_card_id;
    unsigned long expected_target_card_version;
} WenaChecklistEdit;
typedef struct WenaChecklistSnapshot {
    WenaId board_id;
    WenaId card_id;
    unsigned long card_version;
    size_t checklist_count;
    size_t item_count;
    WenaChecklist checklists[WENA_CARD_CHECKLIST_CAPACITY];
    WenaChecklistItem items[WENA_CARD_CHECKLIST_ITEM_CAPACITY];
    unsigned long checklist_versions[WENA_CARD_CHECKLIST_CAPACITY];
    unsigned long item_versions[WENA_CARD_CHECKLIST_ITEM_CAPACITY];
} WenaChecklistSnapshot;
int wena_checklist_snapshot_valid(const WenaChecklistSnapshot*,const char *board_id,const char *card_id);
#endif
