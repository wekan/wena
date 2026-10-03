#ifndef WENA_BOARD_SIDEBAR_H
#define WENA_BOARD_SIDEBAR_H

struct nk_context;

#include <stddef.h>
#include "../common/paginated_table.h"

typedef enum WenaSidebarSection {
    WENA_SIDEBAR_ACTIVITIES = 0,
    WENA_SIDEBAR_MEMBERS,
    WENA_SIDEBAR_LABELS,
    WENA_SIDEBAR_ARCHIVES,
    WENA_SIDEBAR_SETTINGS     /* WeKan's "Board Settings", first in its sidebar */
} WenaSidebarSection;

typedef struct WenaSidebarItems {
    const char *const *activities;
    size_t activity_count;
    const char *const *members;
    size_t member_count;
    const char *const *labels;
    const char *const *label_colors;   /* optional: WeKan's chips in their colors */
    size_t label_count;
    const char *const *archives;
    size_t archive_count;
} WenaSidebarItems;

/* WeKan's foldable sidebar sections (homeSidebar). */
typedef enum WenaSidebarFold {
    WENA_SIDEBAR_FOLD_MEMBERS,
    WENA_SIDEBAR_FOLD_LABELS,
    WENA_SIDEBAR_FOLD_ACTIVITIES,
    WENA_SIDEBAR_FOLD_COUNT
} WenaSidebarFold;
typedef struct WenaBoardSidebar {
    int visible;
    WenaSidebarSection section;
    WenaSidebarItems items;
    WenaTableState table;
    int folded[WENA_SIDEBAR_FOLD_COUNT];
} WenaBoardSidebar;

#define WENA_SIDEBAR_NO_ACTION 0u
#define WENA_SIDEBAR_SECTION_CHANGED 1u
#define WENA_SIDEBAR_CLOSED 2u
#define WENA_SIDEBAR_REFRESH_ACTIVITIES 4u
#define WENA_SIDEBAR_ADD_MEMBER 8u
#define WENA_SIDEBAR_ADD_LABEL 16u
#define WENA_SIDEBAR_RESTORE_ARCHIVE 32u
#define WENA_SIDEBAR_INVALID_STATE 64u

void wena_board_sidebar_init(WenaBoardSidebar *sidebar);
unsigned int wena_board_sidebar_render(struct nk_context *context,
                                       WenaBoardSidebar *sidebar);

#endif
