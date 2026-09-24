#include "directory_picker.h"
#include "../../imports/ui/page_contract.h"
#include <nuklear.h>
#include <string.h>

int wena_directory_picker_init(WenaDirectoryPicker *state,size_t page_size,
    WenaDirectoryLoad load,void *context)
{
    if (!state || !load || !page_size || page_size>WENA_DIRECTORY_PAGE_CAPACITY) return 0;
    memset(state,0,sizeof(*state));state->load=load;state->context=context;
    return wena_table_init(&state->table,page_size);
}
void wena_directory_picker_close(WenaDirectoryPicker *state)
{
    if (!state) return;
    state->board_id[0]=0;
    state->open=0;state->loaded=0;state->error=0;state->read_pending=0;state->selection_pending=0;
    memset(&state->selected,0,sizeof(state->selected));
}
int wena_directory_picker_open_scoped(WenaDirectoryPicker *state,WenaDirectoryKind kind,
    const char *board_id)
{
    WenaId scope;
    if (!state || !state->load || (kind!=WENA_DIRECTORY_BOARDS && kind!=WENA_DIRECTORY_ACTORS &&
        kind!=WENA_DIRECTORY_CARDS)) return 0;
    if (kind==WENA_DIRECTORY_CARDS ? !wena_model_identifier_valid(board_id) :
        (board_id && board_id[0])) return 0;
    scope[0]=0;
    if (kind==WENA_DIRECTORY_CARDS) strcpy(scope,board_id);
    wena_directory_picker_close(state);
    strcpy(state->board_id,scope);
    state->kind=kind;state->table.page=0;state->open=1;state->read_pending=1;
    return 1;
}
int wena_directory_picker_open(WenaDirectoryPicker *state,WenaDirectoryKind kind)
{
    return wena_directory_picker_open_scoped(state,kind,NULL);
}
int wena_directory_picker_poll(WenaDirectoryPicker *state)
{
    WenaDirectoryPage candidate;
    size_t expected,last;
    if (!state || !state->open || !state->read_pending) return 0;
    state->read_pending=0;
    memset(&candidate,0,sizeof(candidate));
    if (!state->load || !state->load(state->context,state->kind,state->board_id,state->table.page,
        state->table.page_size,&candidate) || !wena_directory_page_valid(&candidate) ||
        candidate.kind!=state->kind || strcmp(candidate.board_id,state->board_id) || candidate.page_size!=state->table.page_size) {
        state->error=1;return -1;
    }
    last=candidate.total ? (candidate.total-1)/candidate.page_size : 0;
    expected=state->table.page>last ? last : state->table.page;
    if (candidate.page!=expected) {state->error=1;return -1;}
    state->page=candidate;state->table.page=candidate.page;
    state->loaded=1;state->error=0;return 1;
}
static unsigned int directory_row(struct nk_context *context,void *opaque,size_t index)
{
    WenaDirectoryPicker *state;
    const WenaDirectoryRow *row;
    int selected;
    state=(WenaDirectoryPicker *)opaque;
    row=&state->page.rows[index-state->page.first];
    selected=nk_button_label(context,row->title);
    /* Domain IDs disambiguate duplicate titles; revisions stay internal. */
    nk_label(context,row->id,NK_TEXT_LEFT);
    return selected && !state->selection_pending ? 1u : 0u;
}
int wena_directory_picker_render(struct nk_context *context,WenaDirectoryPicker *state)
{
    WenaTableView view;
    WenaTableResult result;
    if (!context || !state || !state->open) return 0;
    memset(&view,0,sizeof(view));
    view.row_count=state->loaded ? state->page.total : 1;
    view.column_count=2;view.row_height=32;view.windowed=1;
    view.available_first=state->loaded ? state->page.first : 0;
    view.available_count=state->loaded ? state->page.count : 0;
    view.render_row=directory_row;view.context=state;
    if (state->error) view.error_text=wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED);
    result=wena_table_render(context,&state->table,&view);
    if (result.needs_rows || result.page_changed) state->read_pending=1;
    if (state->error) {
        nk_layout_row_dynamic(context,28,1);
        if (nk_button_label(context,wena_ui_text(WENA_UI_TEXT_REFRESH))) state->read_pending=1;
    }
    if (result.action && !state->selection_pending) {
        state->selected=state->page.rows[result.row-state->page.first];
        state->selection_pending=1;return 1;
    }
    return 0;
}
