#ifndef WENA_BOARD_SIDEBAR_H
#define WENA_BOARD_SIDEBAR_H

struct nk_context;

typedef enum WenaSidebarSection {
    WENA_SIDEBAR_ACTIVITIES = 0,
    WENA_SIDEBAR_MEMBERS,
    WENA_SIDEBAR_LABELS,
    WENA_SIDEBAR_ARCHIVES
} WenaSidebarSection;

typedef struct WenaBoardSidebar {
    int visible;
    WenaSidebarSection section;
} WenaBoardSidebar;

#define WENA_SIDEBAR_NO_ACTION 0u
#define WENA_SIDEBAR_SECTION_CHANGED 1u
#define WENA_SIDEBAR_CLOSED 2u

void wena_board_sidebar_init(WenaBoardSidebar *sidebar);
unsigned int wena_board_sidebar_render(struct nk_context *context,
                                       WenaBoardSidebar *sidebar);

#endif
