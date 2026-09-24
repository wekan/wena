#ifndef WENA_JSON_EDIT_H
#define WENA_JSON_EDIT_H
#include "document.h"
#define WENA_JSON_MAX_EDITS 32u
typedef struct WenaJsonEdit {
    size_t node;
    const char *json;
    size_t length;
} WenaJsonEdit;
/* Replace 1..32 disjoint nodes with complete JSON values. Node indexes refer to
 * source, not earlier edits. Each fragment and the entire result must validate.
 * Untouched source bytes are preserved. Output is owned, initialized to NULL;
 * failure preserves it. Source may equal *output; success then replaces it. */
int wena_json_edit(const WenaJsonDocument *source,const WenaJsonEdit *edits,
    size_t count,WenaJsonDocument **output);
#endif
