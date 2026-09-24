#ifndef WENA_BOARD_LAYOUT_H
#define WENA_BOARD_LAYOUT_H

#include "../../../models/wekan_models.h"
#include "../sidebar/board_sidebar.h"

#include <stddef.h>

struct nk_context;

/* Caller-owned presentation state; never persisted as board/card mutations. */
#define WENA_BOARD_COLLAPSE_CAPACITY 64

typedef enum WenaBoardCollapseKind {
    WENA_COLLAPSE_SWIMLANE,
    WENA_COLLAPSE_LIST
} WenaBoardCollapseKind;

typedef struct WenaBoardCollapseState {
    WenaId board_id;
    WenaId swimlane_ids[WENA_BOARD_COLLAPSE_CAPACITY];
    size_t swimlane_count;
    WenaId list_ids[WENA_BOARD_COLLAPSE_CAPACITY];
    size_t list_count;
} WenaBoardCollapseState;

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
    /* Optional fold control. The title/actions stay visible; folded cards skip
     * both badge and expanded-content callbacks. No persistence while drawing. */
    int (*card_collapsed)(struct nk_context *context, void *user_data,
                        const WenaCard *card);
    void *card_collapsed_context;
    /* Expanded content follows the card's own title/actions. */
    unsigned int (*card_contents)(struct nk_context *context, void *user_data,
                        const WenaCard *card);
    void *card_contents_context;
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

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout);

#endif
