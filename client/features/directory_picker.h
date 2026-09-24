#ifndef WENA_DIRECTORY_PICKER_H
#define WENA_DIRECTORY_PICKER_H
#include "../../models/directory.h"
#include "../components/common/paginated_table.h"
typedef int (*WenaDirectoryLoad)(void *context,WenaDirectoryKind kind,
    size_t page,size_t page_size,WenaDirectoryPage *output);
typedef struct WenaDirectoryPicker {
    WenaTableState table;
    WenaDirectoryKind kind;
    WenaDirectoryPage page;
    WenaDirectoryRow selected;
    int open,loaded,error,read_pending,selection_pending;
    WenaDirectoryLoad load;
    void *context;
} WenaDirectoryPicker;
int wena_directory_picker_init(WenaDirectoryPicker *state,size_t page_size,
    WenaDirectoryLoad load,void *context);
int wena_directory_picker_open(WenaDirectoryPicker *state,WenaDirectoryKind kind);
void wena_directory_picker_close(WenaDirectoryPicker *state);
/* Poll outside rendering. One pending read attempt; failure requires Refresh. */
int wena_directory_picker_poll(WenaDirectoryPicker *state);
/* Inside the host window/group: shared table and exact-ID selection only.
 * Consume selection_pending before taking another action; no reads/writes here. */
int wena_directory_picker_render(struct nk_context *context,WenaDirectoryPicker *state);
#endif
