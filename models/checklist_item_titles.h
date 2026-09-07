#ifndef WENA_CHECKLIST_ITEM_TITLES_H
#define WENA_CHECKLIST_ITEM_TITLES_H
#include "checklist.h"
#define WENA_CHECKLIST_ENTRY_MAX_BYTES 16384u
/* Canonical parseChecklistItemTitles semantics on bounded strict UTF-8.
 * Trim ECMAScript whitespace, drop blank candidates, split only at LF when
 * requested, reverse only in split mode. NULL text with length zero is blank.
 * No allocation. Output capacity <= WENA_CHECKLIST_MAX_ITEMS; each title has
 * at most WENA_CHECKLIST_TITLE_CAPACITY-1 bytes. Invalid input/overflow returns
 * zero, leaves titles unchanged, and clears count. Success may produce zero.
 * Input/output/count must not overlap. Parser preserves internal controls;
 * model/persistence validation is still required before any mutation.
 * Parsing multiple titles does not implement an atomic batch transaction. */
int wena_checklist_item_titles_parse(const char *text, size_t length,
    int split_newlines, int reverse,
    char (*titles)[WENA_CHECKLIST_TITLE_CAPACITY], size_t capacity, size_t *count);
#endif
