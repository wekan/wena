#include "paginated_table.h"
#include "../../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>

int wena_table_init(WenaTableState *state, size_t page_size)
{
    if (!state || !page_size || page_size > WENA_TABLE_MAX_PAGE_SIZE) return 0;
    state->page = 0; state->page_size = page_size; return 1;
}
/* C89 has no %zu; do not truncate size_t to unsigned long on Windows. */
static char *decimal(char *end, size_t value)
{
    do { *--end = (char)('0' + value % 10); value /= 10; } while (value);
    return end;
}
WenaTableResult wena_table_render(struct nk_context *context,
    WenaTableState *state, const WenaTableView *view)
{
    WenaTableResult result;
    size_t last, original, first, count, offset;
    unsigned int column, action;
    char text[sizeof(size_t) * 6 + 4], *start;
    memset(&result, 0, sizeof(result));
    if (!context || !state || !view || !state->page_size ||
        state->page_size > WENA_TABLE_MAX_PAGE_SIZE || !view->column_count ||
        view->column_count > WENA_TABLE_MAX_COLUMNS ||
        !(view->row_height > 0.0f && view->row_height <= 4096.0f) ||
        (!view->error_text && view->row_count && !view->render_row)) return result;
    result.valid = 1;
    if (view->error_text) {
        nk_layout_row_dynamic(context, view->row_height, 1);
        nk_label_wrap(context, view->error_text); return result;
    }
    original = state->page;
    last = view->row_count ? (view->row_count - 1) / state->page_size : 0;
    if (state->page > last) state->page = last;
    if (last) {
        nk_layout_row_dynamic(context, 28.0f, 3);
        if (state->page) {
            if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_PREVIOUS_PAGE))) --state->page;
        } else nk_label(context, wena_ui_text(WENA_UI_TEXT_PREVIOUS_PAGE), NK_TEXT_LEFT);
        if (state->page < last) {
            if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_NEXT_PAGE))) ++state->page;
        } else nk_label(context, wena_ui_text(WENA_UI_TEXT_NEXT_PAGE), NK_TEXT_LEFT);
        /* last+1 is representable unless page_size==1 and row_count==SIZE_MAX;
         * even then last==SIZE_MAX-1, so both displayed values fit. */
        text[sizeof(text)-1] = '\0';
        start = decimal(text + sizeof(text)-1, last+1);
        *--start = '/'; start = decimal(start, state->page+1);
        nk_label(context, start, NK_TEXT_LEFT);

    }
    result.page_changed = state->page != original;
    if (!view->row_count) {
        nk_layout_row_dynamic(context, view->row_height, 1);
        nk_label_wrap(context, view->empty_text ? view->empty_text : wena_ui_text(WENA_UI_TEXT_NO_ITEMS));
        return result;
    }
    if (view->headings) {
        nk_layout_row_dynamic(context, view->row_height, (int)view->column_count);
        for (column = 0; column < view->column_count; ++column)
            nk_label(context, view->headings[column] ? view->headings[column] : "", NK_TEXT_LEFT);
    }
    first = state->page * state->page_size;
    count = view->row_count - first;
    if (count > state->page_size) count = state->page_size;
    for (offset = 0; offset < count; ++offset) {
        nk_layout_row_dynamic(context, view->row_height, (int)view->column_count);
        action = view->render_row(context, view->context, first + offset);
        if (action && !result.action) { result.action = action; result.row = first + offset; }
    }
    return result;
}
