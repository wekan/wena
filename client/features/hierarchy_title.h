#ifndef WENA_HIERARCHY_TITLE_H
#define WENA_HIERARCHY_TITLE_H

#include "../components/boards/board_layout.h"
#include "card_details.h"
#include "../components/forms/color_input.h"

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

typedef int (*WenaHierarchyLoadWip)(void *context,const char *board_id,const char *list_id,
    WenaWipLimit *limit,size_t *count,unsigned long *version);
typedef int (*WenaHierarchySaveWip)(void *context,const char *board_id,const char *list_id,
    unsigned long expected_version,WenaWipEdit edit,size_t value);

typedef int (*WenaHierarchyLoadListCards)(void *context,const char *board,const char *list,
    const char *lane,unsigned long *list_version,unsigned long *lane_version);
typedef int (*WenaHierarchyArchiveListCards)(void *context,const char *board,const char *list,
    const char *lane,unsigned long list_version,unsigned long lane_version);

#define WENA_HIERARCHY_TITLE_MOVE 1u
#define WENA_HIERARCHY_TITLE_SELECT_CARDS 2u

typedef struct WenaHierarchyTitleState {
    unsigned int requested_action;
    int selection_enabled;
    int visible;
    int creating;
    int confirming_cards;
    WenaId scope_lane;
    unsigned long scope_lane_version;
    WenaHierarchyLoadListCards load_list_cards;
    WenaHierarchyArchiveListCards archive_list_cards;
    int editing_wip;
    WenaWipLimit wip_limit;
    size_t wip_count;
    char wip_value[12];
    int wip_length;
    WenaHierarchyLoadWip load_wip;
    WenaHierarchySaveWip save_wip;
    int editing_color;
    WenaColorInput color_input;
    WenaHierarchyLoadTitle load_color;
    WenaHierarchySaveTitle save_color;
    int error;
    int title_length;
    WenaHierarchyKind kind;
    WenaId board_id;
    WenaId target_id;
    WenaTitle original_title;
    char title_input[WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY)];
    unsigned long title_version;
    WenaHierarchyLoadTitle load_title;
    WenaHierarchySaveTitle save_title;
    WenaHierarchyCreateTitle create_title;
    WenaHierarchyArchive archives[3];
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
void wena_hierarchy_title_set_archive_provider(WenaHierarchyTitleState *state,
    WenaHierarchyKind kind,WenaHierarchyArchive archive);
void wena_hierarchy_title_set_color_adapters(WenaHierarchyTitleState *state,
    WenaHierarchyLoadTitle load,WenaHierarchySaveTitle save);
void wena_hierarchy_title_set_list_cards_adapters(WenaHierarchyTitleState *state,
    WenaHierarchyLoadListCards load,WenaHierarchyArchiveListCards archive);
int wena_hierarchy_title_open_list(WenaHierarchyTitleState *state,
    const WenaBoardLayout *layout,const char *list,const char *lane);
void wena_hierarchy_title_set_wip_adapters(WenaHierarchyTitleState *state,
    WenaHierarchyLoadWip load,WenaHierarchySaveWip save);
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
