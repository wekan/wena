#ifndef WENA_CHECKLISTS_H
#define WENA_CHECKLISTS_H

#include "checklist_store.h"
#include "card_details.h"
#include "../../models/checklist_item_titles.h"

/* Callbacks run synchronously. Load publishes a complete scoped snapshot; save
 * accepts one guarded intent. A NULL save callback opens a read-only panel. */
typedef int (*WenaChecklistsLoad)(void *context, const char *board_id,
    const char *card_id, WenaChecklistSnapshot *snapshot);
typedef int (*WenaChecklistsSave)(void *context, const char *board_id,
    const char *card_id, const WenaChecklistEdit *edit);

typedef struct WenaChecklistsState {
    int visible;
    int error;
    /* A successful write followed by a failed reload disables further edits. */
    int needs_refresh;
    int length;
    int order_position;
    int order_count;
    WenaId board_id;
    WenaId card_id;
    WenaId checklist_id;
    WenaId item_id;
    WenaId target_card_id;
    unsigned long target_card_version;
    WenaChecklistAction action;
    unsigned long card_version;
    unsigned long checklist_version;
    unsigned long item_version;
    int is_finished;
    int hide_checked_items;
    int hide_all_items;
    /* Preserve stored override until native minicard presentation is supported. */
    WenaChecklistMinicard preserved_show_on_minicard;
    /* One full UTF-8 scalar beyond the limit detects overflow even when the
     * next input character needs four bytes; the final byte is a terminator. */
    char input[WENA_NATIVE_EDIT_CAPACITY(WENA_CHECKLIST_BATCH_MAX_BYTES + 1u)];
    WenaChecklistSnapshot *snapshot;
    WenaChecklistsLoad load;
    WenaChecklistsSave save;
    void *context;
} WenaChecklistsState;

/* Initialize once before opening; close releases the owned snapshot and retains
 * callbacks. Close is also required before reinitializing an open state. */
void wena_checklists_init(WenaChecklistsState *state, WenaChecklistsLoad load,
    WenaChecklistsSave save, void *context);
void wena_checklists_close(WenaChecklistsState *state);
int wena_checklists_open(WenaChecklistsState *state, const WenaCard *card);
int wena_checklists_render(struct nk_context *context, WenaChecklistsState *state,
    const WenaCard *cards, size_t card_count, float width, float height);

#endif
