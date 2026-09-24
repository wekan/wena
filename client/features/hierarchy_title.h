#ifndef WENA_HIERARCHY_TITLE_H
#define WENA_HIERARCHY_TITLE_H

#include "../components/boards/board_layout.h"
#include "card_details.h"

typedef enum WenaHierarchyKind {
    WENA_HIERARCHY_BOARD,
    WENA_HIERARCHY_LIST,
    WENA_HIERARCHY_SWIMLANE
} WenaHierarchyKind;

typedef int (*WenaHierarchyLoadTitle)(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, char *title,
    size_t capacity, unsigned long *version);
typedef int (*WenaHierarchySaveTitle)(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *target_id, unsigned long expected_version,
    const char *title);

typedef int (*WenaHierarchyCreateTitle)(void *context, const char *board_id,
    WenaHierarchyKind kind, const char *title);

typedef int (*WenaHierarchyArchive)(void *context,const char *board_id,
    const char *list_id,unsigned long expected_version);

#define WENA_HIERARCHY_TITLE_MOVE 1u

typedef struct WenaHierarchyTitleState {
    unsigned int requested_action;
    int visible;
    int creating;
    int error;
    int title_length;
    WenaHierarchyKind kind;
    WenaId board_id;
    WenaId target_id;
    char title_input[WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY)];
    unsigned long title_version;
    WenaHierarchyLoadTitle load_title;
    WenaHierarchySaveTitle save_title;
    WenaHierarchyCreateTitle create_title;
    WenaHierarchyArchive archive;
    void *context;
} WenaHierarchyTitleState;

void wena_hierarchy_title_init(WenaHierarchyTitleState *state);
void wena_hierarchy_title_close(WenaHierarchyTitleState *state);
void wena_hierarchy_title_set_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyLoadTitle load, WenaHierarchySaveTitle save, void *context);
void wena_hierarchy_title_set_create_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyCreateTitle create);
void wena_hierarchy_title_set_archive_adapter(WenaHierarchyTitleState *state,
    WenaHierarchyArchive archive);
int wena_hierarchy_title_open_create(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind);
/* Opening loads the current persisted title/version. Rendering closes an invalid
 * or removed selection. Failed saves preserve the draft until cancel/reopen. */
int wena_hierarchy_title_open(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout, WenaHierarchyKind kind, const char *target_id);
int wena_hierarchy_title_render(struct nk_context *context,
    WenaHierarchyTitleState *state, const WenaBoardLayout *layout,
    float width, float height);
#endif
