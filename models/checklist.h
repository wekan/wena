#ifndef WENA_CHECKLIST_H
#define WENA_CHECKLIST_H
#include "card.h"

#define WENA_CHECKLIST_TITLE_CAPACITY 129
#define WENA_CHECKLIST_POSITION_MAX 2147483647UL
#define WENA_CHECKLIST_MAX_ITEMS 1024u

typedef enum WenaChecklistMinicard {
    WENA_CHECKLIST_MINICARD_INHERIT = -1,
    WENA_CHECKLIST_MINICARD_HIDE = 0,
    WENA_CHECKLIST_MINICARD_SHOW = 1
} WenaChecklistMinicard;

typedef struct WenaChecklist {
    WenaId id;
    WenaId board_id;
    WenaId card_id;
    char title[WENA_CHECKLIST_TITLE_CAPACITY];
    unsigned long position;
    int hide_checked_items;
    int hide_all_items;
    WenaChecklistMinicard show_on_minicard;
} WenaChecklist;

typedef struct WenaChecklistProgress {
    size_t total;
    size_t finished;
    unsigned int percent;
    int all_items_finished;
    /* WeKan's isFinished also treats hideAllChecklistItems as finished. */
    int is_finished;
} WenaChecklistProgress;

struct WenaChecklistItem;

int wena_checklist_init(WenaChecklist *checklist,const char *id,
    const char *board_id,const char *card_id,const char *title,unsigned long position);
int wena_checklist_valid(const WenaChecklist *checklist);
int wena_checklist_validate_parent(const WenaChecklist *checklist,const WenaCard *card);
/* qsort-compatible comparator for validated entries: position, then stable ID. */
int wena_checklist_compare(const void *first,const void *second);
/* Accept a complete single-checklist item collection (including hidden items).
 * Foreign-scope/duplicate IDs/invalid items/capacity overflow fail atomically. */
int wena_checklist_progress(const WenaChecklist *checklist,
    const struct WenaChecklistItem *items,size_t count,WenaChecklistProgress *progress);
int wena_checklist_shown_at_minicard(const WenaChecklist *checklist,
    int board_default,int *shown);
#endif
