#ifndef WENA_BOARD_LAYOUT_H
#define WENA_BOARD_LAYOUT_H

#include "../../../models/wekan_models.h"
#include "../sidebar/board_sidebar.h"

#include <stddef.h>

struct nk_context;
struct nk_rect;

/* Caller-owned presentation state; never persisted as board/card mutations. */
#define WENA_BOARD_COLLAPSE_CAPACITY 64

typedef enum WenaBoardCollapseKind {
    WENA_COLLAPSE_SWIMLANE,
    WENA_COLLAPSE_LIST
} WenaBoardCollapseKind;

/* Swimlane heights in pixels, set by dragging the bar below a lane. A lane
 * without an entry is drawn at the default; setting the default removes it. */
#define WENA_SWIMLANE_HEIGHT_DEFAULT 360u
#define WENA_SWIMLANE_HEIGHT_MIN 160u
#define WENA_SWIMLANE_HEIGHT_MAX 2000u

typedef struct WenaBoardCollapseState {
    WenaId board_id;
    WenaId swimlane_ids[WENA_BOARD_COLLAPSE_CAPACITY];
    size_t swimlane_count;
    WenaId list_ids[WENA_BOARD_COLLAPSE_CAPACITY];
    size_t list_count;
    WenaId height_ids[WENA_BOARD_COLLAPSE_CAPACITY];
    unsigned int heights[WENA_BOARD_COLLAPSE_CAPACITY];
    size_t height_count;
} WenaBoardCollapseState;

/* Caller-owned gesture state for the bar below each swimlane. While active the
 * lane is drawn at `height`; on release the height is stored in the layout's
 * collapse state (and so saved with it). Escape cancels. `hovered` reports a
 * bar under the mouse this frame, for a resize cursor. */
typedef struct WenaSwimlaneResize {
    int active, hovered;
    WenaId swimlane_id;
    float start_y;
    unsigned int start_height, height;
} WenaSwimlaneResize;

typedef struct WenaBoardLayout {
    const WenaBoard *board;
    const WenaSwimlane *swimlanes;
    size_t swimlane_count;
    const WenaList *lists;
    size_t list_count;
    const WenaCard *cards;
    size_t card_count;
    WenaBoardSidebar *sidebar;
    struct WenaListInteraction *list_interaction;
    struct WenaCardInteraction *card_interaction;
    WenaBoardCollapseState *collapse;
    void (*toolbar)(struct nk_context *context, void *user_data);
    void *toolbar_context;
    struct WenaSwimlaneInteraction *swimlane_interaction;
    /* Optional viewport overlay; default zero preserves embedded composition. */
    int sidebar_as_window;
    /* Optional presentation predicate; arrays and mutation scopes stay intact. */
    int (*card_visible)(void *context, const WenaCard *card);
    void *card_visible_context;
    /* Optional cached presentation, called only for a displayed active card.
     * Callers must not query persistence while drawing each frame. */
    unsigned int (*card_badges)(struct nk_context *context, void *user_data,
                        const WenaCard *card);
    void *card_badges_context;
    /* Optional cached selection predicate for shared title highlighting. */
    int (*card_selected)(void *context,const WenaCard *card);
    void *card_selected_context;
    /* Cached selection control returns an intent; caller applies it after draw. */
    unsigned int (*card_selection)(struct nk_context*,void*,const WenaCard*);
    void *card_selection_context;
    /* Shared hierarchy handles; board-wide list ordinals repeat per lane. */
    void (*list_drag_handle)(struct nk_context*,void*,const WenaList*,size_t);
    void (*swimlane_drag_handle)(struct nk_context*,void*,const WenaSwimlane*,size_t);
    void *hierarchy_drag_context;
    /* Optional card reorder handle; ordinal includes archived/filtered siblings. */
    void (*card_drag_handle)(struct nk_context *context,void *user_data,
                        const WenaCard *card,size_t ordinal);
    void *card_drag_context;
    void (*card_drop_target)(struct nk_context *context,void *user_data,
                        const WenaList *list,const WenaSwimlane *lane);
    void *card_drop_context;
    /* Optional fold control, drawn into the 14px caret slot the minicard has
     * already pushed (no layout of its own), as WeKan's minicard caret. The
     * title/actions stay visible; folded cards skip both badge and
     * expanded-content callbacks. No persistence while drawing. */
    int (*card_collapsed)(struct nk_context *context, void *user_data,
                        const WenaCard *card);
    void *card_collapsed_context;
    /* Expanded content follows the card's own title/actions. */
    unsigned int (*card_contents)(struct nk_context *context, void *user_data,
                        const WenaCard *card);
    void *card_contents_context;
    /* Optional: a resize bar below each expanded swimlane (swimlane_resize.h
     * wena_swimlane_resize_bar), with its gesture state. Needs collapse. */
    WenaSwimlaneResize *swimlane_resize;
    void (*swimlane_resize_bar)(struct nk_context *context, const struct WenaBoardLayout *layout,
                                const WenaSwimlane *swimlane, unsigned int height);
    /* WeKan's header bar: the user and filter state it shows, and what was
     * clicked in it this frame (WENA_BOARD_HEADER_*), when not NULL. */
    const char *header_actor;
    int header_all_boards;          /* WeKan's house to All Boards in the header */
    int header_filter_active;
    unsigned int *header_actions;
    /* WeKan's dragging: the whole minicard, list header or swimlane bar is
     * the handle (area, in screen coordinates); *clicked reports a press and
     * release on it without dragging. When set, these replace the labelled
     * handle rows above. */
    void (*card_drag_area)(struct nk_context *context, void *user_data, const WenaCard *card,
                           size_t ordinal, const struct nk_rect *area, int *clicked);
    void (*list_drag_area)(struct nk_context *context, void *user_data, const WenaList *list,
                           size_t ordinal, const struct nk_rect *area, int *clicked);
    void (*swimlane_drag_area)(struct nk_context *context, void *user_data,
                               const WenaSwimlane *swimlane, size_t ordinal,
                               const struct nk_rect *area, int *clicked);
    /* A list's free space below its cards: a card dropped there goes last. */
    void (*card_drop_area)(struct nk_context *context, void *user_data, const WenaList *list,
                           const WenaSwimlane *lane, const struct nk_rect *area);
    /* WeKan's inline Add Card composer, asked for each list twice: above its
     * cards (bottom 0) and below them (bottom 1), where it replaces
     * "+ Add Card". Returns 1 when it drew there. */
    int (*card_composer)(struct nk_context *context, void *user_data, const WenaList *list,
                         const WenaSwimlane *lane, int bottom);
    void *card_composer_context;
} WenaBoardLayout;

typedef struct WenaListInteraction {
    unsigned int actions;
    WenaId board_id;
    WenaId swimlane_id;
    WenaId list_id;
} WenaListInteraction;

typedef struct WenaCardInteraction {
    unsigned int actions;
    WenaId card_id;
} WenaCardInteraction;

#define WENA_SWIMLANE_EDIT_TITLE 1u
#define WENA_SWIMLANE_OPEN_MENU 2u    /* "Swimlane Actions" */
#define WENA_SWIMLANE_ADD 4u          /* "Add Swimlane" */
typedef struct WenaSwimlaneInteraction {
    unsigned int actions;
    WenaId board_id;
    WenaId swimlane_id;
} WenaSwimlaneInteraction;

void wena_board_collapse_init(WenaBoardCollapseState *state);
/* Bind to the active board and prune absent/archived/out-of-scope IDs. */
int wena_board_collapse_sync(WenaBoardCollapseState *state,
                              const WenaBoardLayout *layout);
/* Failure leaves state unchanged; IDs must name active scoped model objects. */
int wena_board_collapse_set(WenaBoardCollapseState *state,
                             const WenaBoardLayout *layout,
                             WenaBoardCollapseKind kind, const char *id,
                             int collapsed);
int wena_board_is_collapsed(const WenaBoardCollapseState *state,
                             const char *board_id, WenaBoardCollapseKind kind,
                             const char *id);

/* The lane's height: its stored one, or the default. */
unsigned int wena_board_swimlane_height(const WenaBoardCollapseState *state,
                                        const char *board_id, const char *swimlane_id);
/* Store a height for an active lane, clamped to MIN..MAX; the default removes
 * the entry. Failure (inactive lane, full state) leaves state unchanged. */
int wena_board_swimlane_height_set(WenaBoardCollapseState *state,
                                   const WenaBoardLayout *layout,
                                   const char *swimlane_id, unsigned int height);
unsigned int wena_board_swimlane_height_clamp(long height);

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout);

#endif
