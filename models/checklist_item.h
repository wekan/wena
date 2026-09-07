#ifndef WENA_CHECKLIST_ITEM_H
#define WENA_CHECKLIST_ITEM_H
#include "checklist.h"

typedef struct WenaChecklistItem {
    WenaId id;
    WenaId board_id;
    WenaId card_id;
    WenaId checklist_id;
    char title[WENA_CHECKLIST_TITLE_CAPACITY];
    unsigned long position;
    int is_finished;
} WenaChecklistItem;

int wena_checklist_item_init(WenaChecklistItem *item,const char *id,
    const char *board_id,const char *card_id,const char *checklist_id,
    const char *title,unsigned long position,int is_finished);
int wena_checklist_item_valid(const WenaChecklistItem *item);
int wena_checklist_item_validate_parent(const WenaChecklistItem *item,
    const WenaChecklist *checklist);
int wena_checklist_item_set_finished(WenaChecklistItem *item,
    const WenaChecklist *checklist,int is_finished);
/* qsort-compatible comparator for validated entries: position, then stable ID. */
int wena_checklist_item_compare(const void *first,const void *second);
#endif
