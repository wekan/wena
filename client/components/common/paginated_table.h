#ifndef WENA_PAGINATED_TABLE_H
#define WENA_PAGINATED_TABLE_H
#include <stddef.h>
struct nk_context;
#define WENA_TABLE_MAX_PAGE_SIZE 256u
#define WENA_TABLE_MAX_COLUMNS 16u
typedef struct WenaTableState { size_t page, page_size; } WenaTableState;
/* Render exactly column_count widgets. Return an intent, never mutate the data
 * being traversed. The owner resolves result.row against its immutable snapshot
 * to an exact ID/revision before acting. Only the first intent is reported. */
typedef unsigned int (*WenaTableRow)(struct nk_context *, void *, size_t row);
typedef struct WenaTableView {
    size_t row_count;
    unsigned int column_count;
    float row_height;
    const char *const *headings; /* NULL omits the heading row. */
    const char *empty_text;
    const char *error_text; /* Non-NULL hides rows and navigation. */
    WenaTableRow render_row;
    void *context;
} WenaTableView;
typedef struct WenaTableResult {
    int valid;
    int page_changed;
    unsigned int action;
    size_t row; /* Defined only when action != 0. Absolute, not page-relative. */
} WenaTableResult;
int wena_table_init(WenaTableState *state, size_t page_size);
/* Reset page to zero when replacing/filtering/sorting a data set. Shrinking the
 * existing set automatically clamps the page. No allocations, I/O or ownership.
 * Caller supplies a distinct Nuklear window/group for each simultaneous table. */
WenaTableResult wena_table_render(struct nk_context *context,
    WenaTableState *state, const WenaTableView *view);
#endif
