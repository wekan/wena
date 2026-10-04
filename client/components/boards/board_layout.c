#include "../common/color_heading.h"
#include "../common/wekan_look.h"
#include "../../../models/color.h"
#include "board_layout.h"
#include "board_header.h"
#include "../../../imports/ui/page_contract.h"
#include "../lists/list_header.h"
#include "../cards/card_body.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

static int wena_same_id(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

static int wena_collapse_layout_valid(const WenaBoardLayout *layout)
{
    return layout != NULL && layout->board != NULL &&
        !layout->board->archived && layout->board->id[0] != '\0' &&
        (layout->swimlane_count == 0 || layout->swimlanes != NULL) &&
        (layout->list_count == 0 || layout->lists != NULL);
}

static int wena_collapse_active(const WenaBoardLayout *layout,
                                WenaBoardCollapseKind kind, const char *id)
{
    size_t index;
    size_t lane_index;

    if (kind == WENA_COLLAPSE_SWIMLANE) {
        for (index = 0; index < layout->swimlane_count; ++index) {
            const WenaSwimlane *lane = &layout->swimlanes[index];
            if (!lane->archived && wena_same_id(lane->id, id) &&
                wena_same_id(lane->board_id, layout->board->id)) {
                return 1;
            }
        }
    } else if (kind == WENA_COLLAPSE_LIST) {
        for (index = 0; index < layout->list_count; ++index) {
            const WenaList *list = &layout->lists[index];
            if (!list->archived && wena_same_id(list->id, id) &&
                wena_same_id(list->board_id, layout->board->id)) {
                for (lane_index = 0; lane_index < layout->swimlane_count;
                     ++lane_index) {
                    const WenaSwimlane *lane = &layout->swimlanes[lane_index];
                    if (!lane->archived &&
                        wena_same_id(lane->board_id, layout->board->id) &&
                        (list->swimlane_id[0] == '\0' ||
                         wena_same_id(list->swimlane_id, lane->id))) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void wena_board_collapse_init(WenaBoardCollapseState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

static void wena_collapse_prune(WenaId *ids, size_t *count,
                                const WenaBoardLayout *layout,
                                WenaBoardCollapseKind kind)
{
    size_t read_index;
    size_t write_index;

    write_index = 0;
    for (read_index = 0; read_index < *count; ++read_index) {
        if (wena_collapse_active(layout, kind, ids[read_index])) {
            if (write_index != read_index) {
                memcpy(ids[write_index], ids[read_index], sizeof(WenaId));
            }
            ++write_index;
        }
    }
    *count = write_index;
}

int wena_board_collapse_sync(WenaBoardCollapseState *state,
                              const WenaBoardLayout *layout)
{
    if (state == NULL || !wena_collapse_layout_valid(layout)) {
        return 0;
    }
    if (!wena_same_id(state->board_id, layout->board->id)) {
        wena_board_collapse_init(state);
        (void)wena_model_set_required(state->board_id, sizeof(state->board_id),
                                      layout->board->id);
    }
    if (state->swimlane_count > WENA_BOARD_COLLAPSE_CAPACITY ||
        state->list_count > WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    wena_collapse_prune(state->swimlane_ids, &state->swimlane_count, layout,
                        WENA_COLLAPSE_SWIMLANE);
    wena_collapse_prune(state->list_ids, &state->list_count, layout,
                        WENA_COLLAPSE_LIST);
    if (state->height_count > WENA_BOARD_COLLAPSE_CAPACITY) return 0;
    {
        size_t read_index, write_index = 0;
        for (read_index = 0; read_index < state->height_count; ++read_index) {
            if (wena_collapse_active(layout, WENA_COLLAPSE_SWIMLANE,
                                     state->height_ids[read_index])) {
                if (write_index != read_index) {
                    memcpy(state->height_ids[write_index], state->height_ids[read_index],
                           sizeof(WenaId));
                    state->heights[write_index] = state->heights[read_index];
                }
                ++write_index;
            }
        }
        state->height_count = write_index;
    }
    return 1;
}

unsigned int wena_board_swimlane_height_clamp(long height)
{
    if (height < (long)WENA_SWIMLANE_HEIGHT_MIN) return WENA_SWIMLANE_HEIGHT_MIN;
    if (height > (long)WENA_SWIMLANE_HEIGHT_MAX) return WENA_SWIMLANE_HEIGHT_MAX;
    return (unsigned int)height;
}

unsigned int wena_board_swimlane_height(const WenaBoardCollapseState *state,
                                        const char *board_id, const char *swimlane_id)
{
    size_t index;
    if (state == NULL || board_id == NULL || swimlane_id == NULL ||
        !wena_same_id(state->board_id, board_id) ||
        state->height_count > WENA_BOARD_COLLAPSE_CAPACITY) return WENA_SWIMLANE_HEIGHT_DEFAULT;
    for (index = 0; index < state->height_count; ++index)
        if (wena_same_id(state->height_ids[index], swimlane_id))
            return wena_board_swimlane_height_clamp((long)state->heights[index]);
    return WENA_SWIMLANE_HEIGHT_DEFAULT;
}

int wena_board_swimlane_height_set(WenaBoardCollapseState *state,
                                   const WenaBoardLayout *layout,
                                   const char *swimlane_id, unsigned int height)
{
    WenaId validated;
    size_t index;

    if (state == NULL || !wena_collapse_layout_valid(layout) ||
        !wena_same_id(state->board_id, layout->board->id) ||
        state->height_count > WENA_BOARD_COLLAPSE_CAPACITY ||
        !wena_model_set_required(validated, sizeof(validated), swimlane_id) ||
        !wena_collapse_active(layout, WENA_COLLAPSE_SWIMLANE, validated)) {
        return 0;
    }
    height = wena_board_swimlane_height_clamp((long)height);
    for (index = 0; index < state->height_count; ++index) {
        if (wena_same_id(state->height_ids[index], validated)) {
            if (height == WENA_SWIMLANE_HEIGHT_DEFAULT) {
                --state->height_count;
                if (index < state->height_count) {
                    memmove(state->height_ids[index], state->height_ids[index + 1],
                            (state->height_count - index) * sizeof(WenaId));
                    memmove(&state->heights[index], &state->heights[index + 1],
                            (state->height_count - index) * sizeof(state->heights[0]));
                }
            } else {
                state->heights[index] = height;
            }
            return 1;
        }
    }
    if (height == WENA_SWIMLANE_HEIGHT_DEFAULT) return 1;
    if (state->height_count == WENA_BOARD_COLLAPSE_CAPACITY) return 0;
    strcpy(state->height_ids[state->height_count], validated);
    state->heights[state->height_count++] = height;
    return 1;
}


int wena_board_is_collapsed(const WenaBoardCollapseState *state,
                             const char *board_id, WenaBoardCollapseKind kind,
                             const char *id)
{
    size_t index;
    size_t count;
    const WenaId *ids;

    if (state == NULL || board_id == NULL || id == NULL ||
        !wena_same_id(state->board_id, board_id) ||
        (kind != WENA_COLLAPSE_SWIMLANE && kind != WENA_COLLAPSE_LIST)) {
        return 0;
    }
    count = kind == WENA_COLLAPSE_SWIMLANE ? state->swimlane_count :
        state->list_count;
    ids = kind == WENA_COLLAPSE_SWIMLANE ? state->swimlane_ids : state->list_ids;
    if (count > WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    for (index = 0; index < count; ++index) {
        if (wena_same_id(ids[index], id)) {
            return 1;
        }
    }
    return 0;
}

int wena_board_collapse_set(WenaBoardCollapseState *state,
                             const WenaBoardLayout *layout,
                             WenaBoardCollapseKind kind, const char *id,
                             int collapsed)
{
    WenaId validated;
    WenaId *ids;
    size_t *count;
    size_t index;

    if (state == NULL || !wena_collapse_layout_valid(layout) ||
        !wena_same_id(state->board_id, layout->board->id) ||
        !wena_model_set_required(validated, sizeof(validated), id) ||
        !wena_collapse_active(layout, kind, validated)) {
        return 0;
    }
    ids = kind == WENA_COLLAPSE_SWIMLANE ? state->swimlane_ids : state->list_ids;
    count = kind == WENA_COLLAPSE_SWIMLANE ? &state->swimlane_count :
        &state->list_count;
    if (*count > WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    for (index = 0; index < *count; ++index) {
        if (wena_same_id(ids[index], validated)) {
            if (!collapsed) {
                --*count;
                if (index < *count) {
                    memmove(ids[index], ids[index + 1],
                            (*count - index) * sizeof(WenaId));
                }
            }
            return 1;
        }
    }
    if (!collapsed) {
        return 1;
    }
    if (*count == WENA_BOARD_COLLAPSE_CAPACITY) {
        return 0;
    }
    strcpy(ids[*count], validated);
    ++*count;
    return 1;
}

/* Titles may repeat or change. Nuklear scroll/widget state follows model IDs. */
static int wena_model_group_begin_flags(struct nk_context *context,
                                   const char *prefix, const char *board_id,
                                   const char *lane_id, const char *id, nk_flags flags)
{
    char key[3 * WENA_ID_CAPACITY + 32];

    (void)sprintf(key, "%s%lu:%s/%lu:%s/%s", prefix,
                   (unsigned long)strlen(board_id), board_id,
                   (unsigned long)strlen(lane_id), lane_id, id);
    return nk_group_begin(context, key, flags);
}

/* Each minicard's height, measured as it is drawn and used for its
 * background the next frame: Nuklear draws in order, so the white card has to
 * be filled before what it holds, whose height only drawing tells. */
#define CARD_EXTENT_CAPACITY 1024
static struct { WenaId id; float height; } card_extents[CARD_EXTENT_CAPACITY];
static size_t card_extent_next;
static float card_extent(const char *id)
{
    size_t index;
    for (index = 0; index < CARD_EXTENT_CAPACITY; ++index)
        if (card_extents[index].id[0] && !strcmp(card_extents[index].id, id))
            return card_extents[index].height;
    return 54.0f; /* WeKan's one-line minicard */
}
static void card_extent_set(const char *id, float height)
{
    size_t index;
    for (index = 0; index < CARD_EXTENT_CAPACITY; ++index)
        if (card_extents[index].id[0] && !strcmp(card_extents[index].id, id)) {
            card_extents[index].height = height; return;
        }
    index = card_extent_next++ % CARD_EXTENT_CAPACITY;
    strcpy(card_extents[index].id, id);
    card_extents[index].height = height;
}

/* WeKan's list: 272 pixels unless it has its own width (lists.width), with a
 * line at its left and 24 between lists; WeKan's minicard is 34 narrower,
 * at the list's left margin of 11. */
#define LIST_WIDTH 272.0f
#define MINICARD_MARGIN 11.0f
#define MINICARD_INSET 34.0f

float wena_board_list_width(const WenaBoardLayout *layout, const WenaList *list)
{
    if (layout != NULL && layout->mobile_list_width > 0.0f) return layout->mobile_list_width;
    if (list != NULL && list->width >= 100u && list->width <= 1000u) return (float)list->width;
    if (layout != NULL && layout->default_list_width >= 100.0f && layout->default_list_width <= 1000.0f)
        return layout->default_list_width;
    return LIST_WIDTH;
}
#define MINICARD_GAP 10.0f
static unsigned int wena_render_minicard(struct nk_context *context,
                                         const WenaBoardLayout *layout,
                                         const WenaCard *card, size_t position, float list_width)
{
    unsigned int card_action;
    const float minicard_width = list_width - MINICARD_INSET;
    int collapsed, selected, clicked;
    float height, used, title_height, width;
    struct nk_rect bounds, handle;
    struct nk_panel *panel;
    char key[WENA_ID_CAPACITY + 16];
    const struct nk_user_font *small;

    card_action = WENA_CARD_BODY_NO_ACTION;
    height = card_extent(card->id);
    nk_layout_row_begin(context, NK_STATIC, height, 2);
    nk_layout_row_push(context, MINICARD_MARGIN);
    nk_spacing(context, 1);
    nk_layout_row_push(context, minicard_width);
    bounds = nk_widget_bounds(context);
    selected = layout->card_selected && layout->card_selected(layout->card_selected_context, card);
    wena_wekan_fill(context, bounds.x, bounds.y + 2.0f, bounds.w, bounds.h, WENA_WEKAN_MINICARD_SHADOW, 7.0f);
    wena_wekan_fill(context, bounds.x, bounds.y, bounds.w, bounds.h,
                    selected ? WENA_WEKAN_MINICARD_OPEN : WENA_WEKAN_MINICARD, 7.0f);
    if (selected) wena_wekan_fill(context, bounds.x, bounds.y + 4.0f, 3.0f, bounds.h - 8.0f, WENA_WEKAN_BUTTON, 1.0f);
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgba(0, 0, 0, 0)));
    nk_style_push_vec2(context, &context->style.window.group_padding, nk_vec2(10.0f, 6.0f));
    nk_style_push_vec2(context, &context->style.window.spacing, nk_vec2(4.0f, 2.0f));
    (void)sprintf(key, "card/%s", card->id);
    if (nk_group_begin(context, key, NK_WINDOW_NO_SCROLLBAR)) {
        nk_style_pop_vec2(context);
        nk_style_pop_vec2(context);
        nk_style_pop_style_item(context);
        wena_ui_region("minicard");
        width = minicard_width - 20.0f;
        /* The caret and the Card Actions menu, as WeKan's first minicard row. */
        nk_layout_space_begin(context, NK_STATIC, 14.0f, 2);
        collapsed = 0;
        if (layout->card_collapsed) {
            nk_layout_space_push(context, nk_rect(-2.0f, 0.0f, 14.0f, 14.0f));
            collapsed = layout->card_collapsed(context, layout->card_collapsed_context, card);
        }
        nk_layout_space_push(context, nk_rect(width - 12.0f, 0.0f, 14.0f, 14.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_BARS, wena_ui_text(WENA_UI_TEXT_CARD_ACTIONS), 12.0f,
                                   WENA_WEKAN_ICON))
            card_action |= WENA_CARD_BODY_OPEN_MENU;
        nk_layout_space_end(context);
        /* Older layouts: the labelled drag handle row. */
        if (!layout->card_drag_area && layout->card_drag_handle)
            layout->card_drag_handle(context, layout->card_drag_context, card, position);
        small = wena_wekan_font(context, WENA_WEKAN_FONT_SMALL);
        title_height = small != NULL ? small->height + 6.0f : 18.0f;
        if (small != NULL && small->width(small->userdata, small->height, card->title,
                                          (int)strlen(card->title)) > width)
            title_height *= 1.0f + (float)(int)(small->width(small->userdata, small->height, card->title,
                                                             (int)strlen(card->title)) / width);
        nk_layout_row_dynamic(context, title_height, 1);
        nk_style_push_font(context, small);
        nk_label_colored_wrap(context, card->title, nk_rgb(0x4d, 0x4d, 0x4d));
        nk_style_pop_font(context);
        wena_ui_control_record(NULL, card->title, bounds.x, bounds.y, bounds.w, bounds.h);
        if (!collapsed && layout->card_badges != NULL)
            card_action |= layout->card_badges(context, layout->card_badges_context, card);
        if (layout->card_selection)
            card_action |= layout->card_selection(context, layout->card_selection_context, card);
        if (!collapsed && layout->card_contents != NULL)
            card_action |= layout->card_contents(context, layout->card_contents_context, card);
        /* WeKan's badges strip, 8 below: the description badge, gray. */
        if (!collapsed && card->has_description) {
            struct nk_rect badge;
            nk_layout_row_dynamic(context, 8.0f, 1);
            nk_label(context, "", NK_TEXT_LEFT);
            nk_layout_row_begin(context, NK_STATIC, 16.0f, 1);
            nk_layout_row_push(context, 14.0f);
            badge = nk_widget_bounds(context);
            nk_label(context, "", NK_TEXT_LEFT);
            nk_layout_row_end(context);
            wena_wekan_icon_draw(context, WENA_ICON_FILE_TEXT_O, badge.x, badge.y + 1.0f, 13.0f, WENA_WEKAN_ICON);
            wena_ui_control_record(NULL, wena_ui_text(WENA_UI_TEXT_DESCRIPTION), badge.x, badge.y, badge.w, badge.h);
        }
        panel = context->current->layout;
        used = panel->at_y + panel->row.height - panel->bounds.y + 8.0f;
        if (used < 34.0f) used = 34.0f;
        nk_group_end(context);
        if (used > height + 0.5f || used < height - 0.5f) card_extent_set(card->id, used);
    } else {
        nk_style_pop_vec2(context);
        nk_style_pop_vec2(context);
        nk_style_pop_style_item(context);
    }
    nk_layout_row_end(context);
    if (layout->card_drag_area && layout->drag_handles) {
        /* WeKan's .handle: the arrows at the card's right, 28 down (a 44 wide
         * strip to the bottom in mobile mode); only it drags, and the rest of
         * the card opens it. */
        struct nk_rect body = nk_rect(bounds.x, bounds.y + 18.0f, bounds.w, bounds.h - 18.0f);
        handle = layout->mobile_list_width > 0.0f ?
            nk_rect(bounds.x + bounds.w - 44.0f, bounds.y + 32.0f, 44.0f, bounds.h - 32.0f > 36.0f ? bounds.h - 32.0f : 36.0f) :
            nk_rect(bounds.x + bounds.w - 21.0f, bounds.y + 28.0f, 20.0f, 20.0f);
        wena_wekan_icon_draw(context, WENA_ICON_ARROWS, handle.x + (handle.w - 13.0f) / 2.0f,
                             handle.y + (handle.h > 20.0f ? 3.0f : (handle.h - 13.0f) / 2.0f), 13.0f, WENA_WEKAN_ICON);
        wena_ui_control_record(NULL, wena_ui_key_text("show-desktop-drag-handles", NULL),
                               handle.x, handle.y, handle.w, handle.h);
        clicked = 0;
        layout->card_drag_area(context, layout->card_drag_context, card, position, &handle, &clicked);
        if (nk_input_mouse_clicked(&context->input, NK_BUTTON_LEFT, body) &&
            !nk_input_is_mouse_hovering_rect(&context->input, handle))
            card_action |= WENA_CARD_BODY_OPEN_DETAILS;
    } else if (layout->card_drag_area) {
        /* The card below its caret row is the handle: drag to move, click to open. */
        handle = nk_rect(bounds.x, bounds.y + 18.0f, bounds.w, bounds.h - 18.0f);
        clicked = 0;
        layout->card_drag_area(context, layout->card_drag_context, card, position, &handle, &clicked);
        if (clicked) card_action |= WENA_CARD_BODY_OPEN_DETAILS;
    }
    nk_layout_row_dynamic(context, MINICARD_GAP, 1);
    nk_spacing(context, 1);
    return card_action;
}

static void wena_render_card(struct nk_context *context, const WenaBoardLayout *layout,
                             const WenaList *list, const WenaCard *card, size_t position)
{
    unsigned int card_action;
    if (card->archived ||
        (layout->card_visible != NULL && !layout->card_visible(layout->card_visible_context, card))) return;
    card_action = wena_render_minicard(context, layout, card, position, wena_board_list_width(layout, list));
    if (layout->card_interaction != NULL && card_action != WENA_CARD_BODY_NO_ACTION) {
        layout->card_interaction->actions = card_action;
        (void)wena_model_set_required(layout->card_interaction->card_id,
            sizeof(layout->card_interaction->card_id), card->id);
    }
}

/* WeKan's Sort Cards: a card and its place in the list's own order, which
 * moves and drops keep using. */
typedef struct SortedCard {
    const WenaCard *card;
    size_t position;
} SortedCard;

/* qsort has no context: the sort being drawn. */
static int sort_mode;

/* Mongo's order of a field that may be missing: missing first ascending,
 * last descending. */
static int by_time(int has_a, double a, int has_b, double b, int descending)
{
    int order = has_a != has_b ? (has_a ? 1 : -1) : a < b ? -1 : a > b;
    return descending ? -order : order;
}

/* WeKan's sorts (boardHeader.js cardsSortPopup): Mongo's sort with the card's
 * _id as the tiebreaker (cardSortTiebreaker.js); votes keep the list's own
 * order for a tie (voteSortCards.js). */
static int by_sort(const void *a, const void *b)
{
    const SortedCard *x = (const SortedCard *)a, *y = (const SortedCard *)b;
    int order = 0;
    switch (sort_mode) {
    case WENA_BOARD_SORT_TITLE: order = strcmp(x->card->title, y->card->title); break;
    case WENA_BOARD_SORT_DUE:
        order = by_time(x->card->has_due_at, x->card->due_at, y->card->has_due_at, y->card->due_at, 0); break;
    case WENA_BOARD_SORT_CREATED_NEWEST: case WENA_BOARD_SORT_CREATED_OLDEST:
        order = by_time(x->card->has_created_at, x->card->created_at, y->card->has_created_at, y->card->created_at,
                        sort_mode == WENA_BOARD_SORT_CREATED_NEWEST); break;
    case WENA_BOARD_SORT_VOTES:
        order = y->card->votes - x->card->votes;
        if (order == 0) return x->position < y->position ? -1 : x->position > y->position;
        return order;
    default: break;
    }
    return order != 0 ? order : strcmp(x->card->id, y->card->id);
}

static void wena_render_cards(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaList *list,
                              const WenaSwimlane *swimlane)
{
    size_t index, ordinal;
    SortedCard *sorted;

    if (!layout->card_drop_area && layout->card_drop_target)
        layout->card_drop_target(context, layout->card_drop_context, list, swimlane);
    sorted = layout->card_sort > WENA_BOARD_SORT_NONE && layout->card_sort <= WENA_BOARD_SORT_VOTES &&
             layout->card_count > 0 ?
        (SortedCard *)malloc(layout->card_count * sizeof(*sorted)) : NULL;
    ordinal = 0;
    for (index = 0; index < layout->card_count; ++index) {
        const WenaCard *card = &layout->cards[index];

        if (!wena_same_id(card->board_id, layout->board->id) ||
            !wena_same_id(card->list_id, list->id) ||
            (!layout->lists_view && !wena_same_id(card->swimlane_id, swimlane->id))) continue;
        if (sorted != NULL) {
            sorted[ordinal].card = card;
            sorted[ordinal].position = ordinal;
            ++ordinal;
        } else wena_render_card(context, layout, list, card, ordinal++);
    }
    if (sorted != NULL) {
        sort_mode = layout->card_sort;
        qsort(sorted, ordinal, sizeof(*sorted), by_sort);
        for (index = 0; index < ordinal; ++index)
            wena_render_card(context, layout, list, sorted[index].card, sorted[index].position);
        free(sorted);
    }
}

static void wena_list_interaction(const WenaBoardLayout *layout, unsigned int action,
                                  const WenaSwimlane *swimlane, const WenaList *list)
{
    if (layout->list_interaction == NULL || action == WENA_LIST_HEADER_NO_ACTION) return;
    layout->list_interaction->actions = action;
    (void)wena_model_set_required(layout->list_interaction->board_id,
        sizeof(layout->list_interaction->board_id), layout->board->id);
    (void)wena_model_set_required(layout->list_interaction->swimlane_id,
        sizeof(layout->list_interaction->swimlane_id), swimlane->id);
    (void)wena_model_set_required(layout->list_interaction->list_id,
        sizeof(layout->list_interaction->list_id), list->id);
}

/* A collapsed list is a narrow strip with its title down it, as WeKan's
 * rotated one; Nuklear draws no rotated text, so its letters are stacked. */
static void wena_render_collapsed_title(struct nk_context *context, const char *title)
{
    char letter[5];
    size_t index, length, shown;
    struct nk_rect bounds;
    bounds = nk_widget_bounds(context);
    wena_ui_control_record("list-header", title, bounds.x, bounds.y, bounds.w, 18.0f * 24.0f);
    for (index = 0, shown = 0; title[index] != '\0' && shown < 24; index += length, ++shown) {
        unsigned char lead = (unsigned char)title[index];
        length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (strlen(title + index) < length) break;
        memcpy(letter, title + index, length);
        letter[length] = '\0';
        nk_layout_row_dynamic(context, 18.0f, 1);
        nk_style_push_font(context, wena_wekan_font(context, WENA_WEKAN_FONT_BOLD));
        nk_label_colored(context, letter, NK_TEXT_CENTERED, nk_rgb(0, 0, 0));
        nk_style_pop_font(context);
    }
}

#define LIST_GAP 24.0f
#define LIST_COLLAPSED_WIDTH 44.0f
/* One list: its border, header, cards and composer, in the widget the
 * caller has laid out (side by side, or one under another in mobile mode). */
static void wena_render_list_column(struct nk_context *context, const WenaBoardLayout *layout,
                                    const WenaSwimlane *swimlane, const WenaList *list, size_t index,
                                    int collapsed)
{
    unsigned int list_action;
    size_t card_index, active_count;
    int collapse_clicked, clicked;
    struct nk_rect title_area, column, rest;
    struct nk_panel *panel;

    column = nk_widget_bounds(context);
    wena_wekan_fill(context, column.x, column.y, 1.0f, column.h, WENA_WEKAN_LIST_BORDER, 0.0f);
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(nk_rgba(0, 0, 0, 0)));
    nk_style_push_vec2(context, &context->style.window.group_padding, nk_vec2(1.0f, 0.0f));
    nk_style_push_vec2(context, &context->style.window.spacing, nk_vec2(0.0f, 0.0f));
    /* WeKan's lists scroll with overlay scrollbars that take no room, so
     * the header and minicards keep the list's whole width. */
    nk_style_push_vec2(context, &context->style.window.scrollbar_size, nk_vec2(0.0f, 0.0f));
    if (wena_model_group_begin_flags(context, "list/", layout->board->id,
                                swimlane->id, list->id, 0)) {
        nk_style_pop_vec2(context);
        nk_style_pop_vec2(context);
        nk_style_pop_style_item(context);
        active_count=0;
        for(card_index=0;card_index<layout->card_count;++card_index){
            const WenaCard *card=&layout->cards[card_index];
            if(!card->archived&&wena_same_id(card->board_id,layout->board->id)&&
                wena_same_id(card->list_id,list->id))++active_count;
        }
        list_action = wena_list_header_render_wekan(context, list, active_count, collapsed,
                                                    &collapse_clicked, &title_area);
        if (collapse_clicked && layout->collapse != NULL &&
            wena_board_collapse_set(layout->collapse, layout, WENA_COLLAPSE_LIST, list->id, !collapsed))
            collapsed = !collapsed;
        if (layout->list_drag_area && layout->drag_handles) {
            /* WeKan's .list-header-handle: the arrows before the header's
             * icons; the title is clicked to rename, as without handles. */
            struct nk_rect handle = nk_rect(title_area.x + title_area.w - 132.0f, title_area.y - 23.0f,
                                            22.0f, 22.0f);
            wena_wekan_icon_draw(context, WENA_ICON_ARROWS, handle.x + 4.0f, handle.y + 4.0f, 14.0f,
                                 WENA_WEKAN_ICON);
            wena_ui_control_record(NULL, wena_ui_key_text("moveListPopup-title", "Move List"),
                                   handle.x, handle.y, handle.w, handle.h);
            clicked = 0;
            layout->list_drag_area(context, layout->hierarchy_drag_context, list, index, &handle, &clicked);
            if (nk_input_mouse_clicked(&context->input, NK_BUTTON_LEFT, title_area))
                list_action |= WENA_LIST_HEADER_EDIT_TITLE;
        } else if (layout->list_drag_area) {
            clicked = 0;
            layout->list_drag_area(context, layout->hierarchy_drag_context, list, index, &title_area, &clicked);
            if (clicked) list_action |= WENA_LIST_HEADER_EDIT_TITLE;
        } else if (layout->list_drag_handle) layout->list_drag_handle(context,
            layout->hierarchy_drag_context,list,index);
        wena_list_interaction(layout, list_action, swimlane, list);
        if (collapsed) wena_render_collapsed_title(context, list->title);
        if (!collapsed) {
            nk_layout_row_dynamic(context, 8.0f, 1);
            nk_spacing(context, 1);
            /* WeKan's inline composer: above the cards for "Add Card to
             * Top of List", in place of "+ Add Card" for the bottom. */
            wena_ui_region("composer");
            if (layout->card_composer)
                (void)layout->card_composer(context, layout->card_composer_context, list, swimlane, 0);
            wena_ui_region("minicard");
            wena_render_cards(context, layout, list, swimlane);
            wena_ui_region("composer");
            if (!layout->card_composer ||
                !layout->card_composer(context, layout->card_composer_context, list, swimlane, 1)) {
                wena_ui_region("list");
                nk_layout_row_begin(context, NK_STATIC, 28.0f, 2);
                nk_layout_row_push(context, 21.0f);
                nk_spacing(context, 1);
                nk_layout_row_push(context, 120.0f);
                if (wena_wekan_link(context, WENA_ICON_PLUS, wena_ui_text(WENA_UI_TEXT_ADD_CARD),
                                    WENA_WEKAN_FONT_LINK, WENA_WEKAN_ADD_CARD))
                    wena_list_interaction(layout, WENA_LIST_HEADER_ADD_CARD | WENA_LIST_HEADER_ADD_CARD_BOTTOM,
                                          swimlane, list);
                nk_layout_row_end(context);
            }
            wena_ui_region("list");
            /* Below the cards: a card dropped there goes last in this list. */
            panel = context->current->layout;
            rest = nk_rect(panel->bounds.x, panel->at_y, panel->bounds.w,
                           panel->bounds.y + panel->bounds.h - panel->at_y);
            if (layout->card_drop_area && rest.h > 0.0f)
                layout->card_drop_area(context, layout->card_drop_context, list, swimlane, &rest);
        }
        nk_group_end(context);
    } else {
        nk_style_pop_vec2(context);
        nk_style_pop_vec2(context);
        nk_style_pop_style_item(context);
    }
    nk_style_pop_vec2(context);
}

static int wena_list_in_lane(const WenaBoardLayout *layout, const WenaSwimlane *swimlane,
                             const WenaList *list)
{
    return !list->archived && wena_same_id(list->board_id, layout->board->id) &&
           (layout->lists_view || list->swimlane_id[0] == '\0' ||
            wena_same_id(list->swimlane_id, swimlane->id));
}

/* Mobile mode: a list as high as its header, its cards and "+ Add Card"; a
 * collapsed one is WeKan's 60 pixel .mini-list. */
#define MOBILE_LIST_GAP 8.0f
float wena_board_mobile_list_height(const WenaBoardLayout *layout, const WenaList *list, int collapsed)
{
    size_t index;
    float height;
    if (collapsed) return 60.0f;
    /* The header, the gap above the cards, "+ Add Card" and room below it. */
    height = WENA_LIST_HEADER_HEIGHT + 8.0f + 28.0f + 40.0f;
    for (index = 0; index < layout->card_count; ++index) {
        const WenaCard *card = &layout->cards[index];
        if (card->archived || !wena_same_id(card->board_id, layout->board->id) ||
            !wena_same_id(card->list_id, list->id) ||
            (layout->card_visible != NULL && !layout->card_visible(layout->card_visible_context, card)))
            continue;
        height += card_extent(card->id) + MINICARD_GAP;
    }
    return height;
}

/* The lists of one lane, one under another, and the gaps between them. */
float wena_board_mobile_lane_height(const WenaBoardLayout *layout, const WenaSwimlane *swimlane)
{
    size_t index;
    float height = 0.0f;
    for (index = 0; index < layout->list_count; ++index) {
        const WenaList *list = &layout->lists[index];
        if (!wena_list_in_lane(layout, swimlane, list)) continue;
        height += wena_board_mobile_list_height(layout, list,
            wena_board_is_collapsed(layout->collapse, layout->board->id, WENA_COLLAPSE_LIST, list->id)) +
            MOBILE_LIST_GAP;
    }
    return height;
}

static void wena_render_lists(struct nk_context *context,
                              const WenaBoardLayout *layout,
                              const WenaSwimlane *swimlane,
                              float lane_height)
{
    size_t index;
    size_t visible_count;
    int collapsed;

    visible_count = 0;
    for (index = 0; index < layout->list_count; ++index)
        if (wena_list_in_lane(layout, swimlane, &layout->lists[index])) ++visible_count;
    if (visible_count == 0 || visible_count > (size_t)INT_MAX / 2) {
        return;
    }
    if (layout->mobile_list_width > 0.0f) {
        /* WeKan's mobile mode: every list the whole width, one under another. */
        for (index = 0; index < layout->list_count; ++index) {
            const WenaList *list = &layout->lists[index];
            if (!wena_list_in_lane(layout, swimlane, list)) continue;
            collapsed = wena_board_is_collapsed(layout->collapse, layout->board->id,
                                                WENA_COLLAPSE_LIST, list->id);
            nk_layout_row_begin(context, NK_STATIC, wena_board_mobile_list_height(layout, list, collapsed), 2);
            nk_layout_row_push(context, 12.0f);
            nk_spacing(context, 1);
            nk_layout_row_push(context, layout->mobile_list_width);
            wena_render_list_column(context, layout, swimlane, list, index, collapsed);
            nk_layout_row_end(context);
            nk_layout_row_dynamic(context, MOBILE_LIST_GAP, 1);
            nk_label(context, "", NK_TEXT_LEFT);
        }
        return;
    }
    /* Fixed-width columns stay readable; the containing lane scrolls sideways. */
    nk_layout_row_begin(context, NK_STATIC, lane_height, (int)visible_count * 2 + 1);
    nk_layout_row_push(context, 12.0f);
    nk_spacing(context, 1);
    for (index = 0; index < layout->list_count; ++index) {
        const WenaList *list = &layout->lists[index];

        if (!wena_list_in_lane(layout, swimlane, list)) continue;
        collapsed = wena_board_is_collapsed(layout->collapse, layout->board->id,
                                            WENA_COLLAPSE_LIST, list->id);
        nk_layout_row_push(context, collapsed ? LIST_COLLAPSED_WIDTH : wena_board_list_width(layout, list));
        wena_render_list_column(context, layout, swimlane, list, index, collapsed);
        nk_layout_row_push(context, LIST_GAP);
        nk_spacing(context, 1);
    }
    nk_layout_row_end(context);
}

/* WeKan's swimlane bar: 33 pixels, the caret, Swimlane Actions and Add
 * Swimlane at its left, the title in the middle. The rest drags the lane. */
#define SWIMLANE_HEADER_HEIGHT 33.0f
static int wena_render_swimlane_header(struct nk_context *context,
                                        const WenaBoardLayout *layout,
                                        const WenaSwimlane *swimlane, size_t index,
                                        int collapsed)
{
    struct nk_rect area, handle;
    unsigned int actions;
    int clicked;
    unsigned char rgb[3], foreground[3];
    int title_color;

    actions = 0u;
    wena_ui_region("swimlane-header");
    nk_layout_space_begin(context, NK_STATIC, SWIMLANE_HEADER_HEIGHT, 4);
    wena_wekan_space_area(context, SWIMLANE_HEADER_HEIGHT, &area.x, &area.y, &area.w);
    area.h = SWIMLANE_HEADER_HEIGHT;
    title_color = WENA_WEKAN_TEXT;
    if (swimlane->color[0] && wena_color_rgb(swimlane->color, rgb) &&
        wena_color_foreground(swimlane->color, foreground)) {
        nk_fill_rect(nk_window_get_canvas(context), area, 0.0f, nk_rgb(rgb[0], rgb[1], rgb[2]));
        title_color = foreground[0] > 128 ? WENA_WEKAN_BUTTON_TEXT : WENA_WEKAN_TEXT;
    } else wena_wekan_fill(context, area.x, area.y, area.w, area.h, WENA_WEKAN_SWIMLANE_HEADER, 0.0f);
    nk_layout_space_push(context, nk_rect(0.0f, 0.0f, 21.0f, SWIMLANE_HEADER_HEIGHT));
    if (wena_wekan_icon_button(context, collapsed ? WENA_ICON_CARET_RIGHT : WENA_ICON_CARET_DOWN,
            wena_ui_text(collapsed ? WENA_UI_TEXT_UNCOLLAPSE : WENA_UI_TEXT_COLLAPSE), 14.0f, WENA_WEKAN_ICON) &&
        layout->collapse != NULL &&
        wena_board_collapse_set(layout->collapse, layout, WENA_COLLAPSE_SWIMLANE, swimlane->id, !collapsed))
        collapsed = !collapsed;
    if (layout->swimlane_interaction != NULL) {
        nk_layout_space_push(context, nk_rect(51.0f, 0.0f, 29.0f, SWIMLANE_HEADER_HEIGHT));
        if (wena_wekan_icon_button(context, WENA_ICON_BARS, wena_ui_text(WENA_UI_TEXT_SWIMLANE_ACTIONS),
                                   20.0f, WENA_WEKAN_ICON))
            actions |= WENA_SWIMLANE_OPEN_MENU;
        nk_layout_space_push(context, nk_rect(101.0f, 0.0f, 29.0f, SWIMLANE_HEADER_HEIGHT));
        if (wena_wekan_icon_button(context, WENA_ICON_PLUS, wena_ui_text(WENA_UI_TEXT_ADD_SWIMLANE),
                                   20.0f, WENA_WEKAN_ICON))
            actions |= WENA_SWIMLANE_ADD;
    }
    nk_layout_space_push(context, nk_rect(140.0f, 0.0f, area.w - 280.0f > 40.0f ? area.w - 280.0f : 40.0f,
                                          SWIMLANE_HEADER_HEIGHT));
    wena_wekan_text(context, swimlane->title, WENA_WEKAN_FONT_BOLD, title_color, NK_TEXT_CENTERED);
    nk_layout_space_end(context);
    if (layout->swimlane_drag_area && layout->drag_handles) {
        /* WeKan's .swimlane-header-handle: the arrows after the +; the title
         * is clicked to rename. */
        struct nk_rect title = nk_rect(area.x + 160.0f, area.y, area.w - 160.0f, area.h);
        handle = nk_rect(area.x + 130.0f, area.y, 26.0f, area.h);
        wena_wekan_icon_draw(context, WENA_ICON_ARROWS, handle.x + 5.0f, handle.y + 9.0f, 15.0f, WENA_WEKAN_ICON);
        wena_ui_control_record(NULL, wena_ui_key_text("show-desktop-drag-handles", NULL),
                               handle.x, handle.y, handle.w, handle.h);
        clicked = 0;
        layout->swimlane_drag_area(context, layout->hierarchy_drag_context, swimlane, index, &handle, &clicked);
        if (nk_input_mouse_clicked(&context->input, NK_BUTTON_LEFT, title)) actions |= WENA_SWIMLANE_EDIT_TITLE;
    } else if (layout->swimlane_drag_area) {
        handle = nk_rect(area.x + 135.0f, area.y, area.w - 135.0f, area.h);
        clicked = 0;
        layout->swimlane_drag_area(context, layout->hierarchy_drag_context, swimlane, index, &handle, &clicked);
        if (clicked) actions |= WENA_SWIMLANE_EDIT_TITLE;
    }
    if (actions != 0u && layout->swimlane_interaction != NULL) {
        layout->swimlane_interaction->actions = actions;
        (void)wena_model_set_required(layout->swimlane_interaction->board_id,
            sizeof(layout->swimlane_interaction->board_id), layout->board->id);
        (void)wena_model_set_required(layout->swimlane_interaction->swimlane_id,
            sizeof(layout->swimlane_interaction->swimlane_id), swimlane->id);
    }
    return collapsed;
}

int wena_board_layout_render(struct nk_context *context,
                             const WenaBoardLayout *layout)
{
    size_t index;
    unsigned int header_action;
    WenaBoardHeaderInfo info;

    if (context == NULL || layout == NULL || layout->board == NULL ||
        layout->board->archived ||
        (layout->swimlane_count != 0 && layout->swimlanes == NULL) ||
        (layout->list_count != 0 && layout->lists == NULL) ||
        (layout->card_count != 0 && layout->cards == NULL)) {
        return 0;
    }
    if (layout->list_interaction != NULL) {
        layout->list_interaction->actions = WENA_LIST_HEADER_NO_ACTION;
        layout->list_interaction->list_id[0] = '\0';
        layout->list_interaction->board_id[0] = '\0';
        layout->list_interaction->swimlane_id[0] = '\0';
    }
    if (layout->swimlane_interaction != NULL) {
        layout->swimlane_interaction->actions = 0u;
        layout->swimlane_interaction->board_id[0] = '\0';
        layout->swimlane_interaction->swimlane_id[0] = '\0';
    }
    if (layout->card_interaction != NULL) {
        layout->card_interaction->actions = WENA_CARD_BODY_NO_ACTION;
        layout->card_interaction->card_id[0] = '\0';
    }
    if (layout->header_actions != NULL) *layout->header_actions = WENA_BOARD_HEADER_NO_ACTION;
    if (layout->collapse != NULL &&
        !wena_board_collapse_sync(layout->collapse, layout)) {
        return 0;
    }
    if (layout->swimlane_resize != NULL) layout->swimlane_resize->hovered = 0;
    info.actor_name = layout->header_actor;
    info.filter_active = layout->header_filter_active;
    info.all_boards = layout->header_all_boards;
    info.star = layout->header_star;
    info.starred_count = layout->header_starred_count;
    info.board_stars = layout->header_board_stars;
    info.multi_selection = layout->header_multi_selection;
    info.permission = layout->header_permission;
    info.watch = layout->header_watch;
    info.search = layout->header_search;
    info.sort = layout->header_sort;
    info.view = layout->header_view;
    info.view_name = layout->header_view_name;
    info.view_icon = layout->header_view_icon;
    info.notifications = layout->header_notifications;
    info.add_board = layout->header_add_board;
    info.collapse = layout->header_collapse;
    info.icons_collapsed = layout->header_icons_collapsed;
    info.drag_handles = layout->header_drag_handles;
    info.mobile_mode = layout->header_mobile_mode;
    info.logo = layout->header_logo;
    info.logo_width = layout->header_logo_width;
    info.logo_height = layout->header_logo_height;
    header_action = wena_board_header_render_info(context, layout->board, &info);
    if (layout->header_actions != NULL) *layout->header_actions = header_action;
    if (layout->toolbar != NULL) {
        layout->toolbar(context, layout->toolbar_context);
    }
    if (layout->sidebar != NULL &&
        (header_action & WENA_BOARD_HEADER_OPEN_MENU) != 0u) {
        layout->sidebar->visible = !layout->sidebar->visible;
    }
    if (layout->card_visible != NULL) {
        int matched;
        matched = 0;
        for (index = 0; index < layout->card_count; ++index)
            if (!layout->cards[index].archived &&
                wena_same_id(layout->cards[index].board_id, layout->board->id) &&
                layout->card_visible(layout->card_visible_context, &layout->cards[index])) {
                matched = 1; break;
            }
        if (!matched) {
            nk_layout_row_begin(context, NK_STATIC, 28.0f, 2);
            nk_layout_row_push(context, 16.0f);
            nk_spacing(context, 1);
            nk_layout_row_push(context, 400.0f);
            wena_wekan_text(context, wena_ui_text(WENA_UI_TEXT_NO_CARDS_FOUND), WENA_WEKAN_FONT_BODY,
                            WENA_WEKAN_TEXT, NK_TEXT_LEFT);
            nk_layout_row_end(context);
        }
    }
    if (layout->view_render != NULL) {
        /* Another of WeKan's views: the rest of the window is its. */
        struct nk_panel *page = context->current->layout;
        float rest = page->bounds.y + page->bounds.h - page->at_y - page->row.height - 8.0f;
        layout->view_render(context, layout->view_context, rest > 120.0f ? rest : 120.0f);
        if (!layout->sidebar_as_window) (void)wena_board_sidebar_render(context, layout->sidebar);
        return 1;
    }
    for (index = 0; index < layout->swimlane_count; ++index) {
        const WenaSwimlane *swimlane = &layout->swimlanes[index];
        unsigned int lane_height;
        int lane_collapsed;

        if (swimlane->archived ||
            !wena_same_id(swimlane->board_id, layout->board->id)) {
            continue;
        }
        if (layout->lists_view) {
            /* WeKan's Lists view: the first swimlane's place, no bar, the
             * rest of the page high; new cards go to that swimlane. */
            struct nk_panel *page = context->current->layout;
            float rest = page->bounds.y + page->bounds.h - page->at_y - page->row.height - 8.0f;
            lane_height = rest > 120.0f ? (unsigned int)rest : 120u;
            lane_collapsed = 0;
        } else {
            lane_collapsed = wena_board_is_collapsed(layout->collapse, layout->board->id,
                WENA_COLLAPSE_SWIMLANE, swimlane->id);
            lane_height = wena_board_swimlane_height(layout->collapse, layout->board->id,
                                                     swimlane->id);
            if (layout->swimlane_resize != NULL && layout->swimlane_resize->active &&
                wena_same_id(layout->swimlane_resize->swimlane_id, swimlane->id))
                lane_height = layout->swimlane_resize->height;
            lane_collapsed = wena_render_swimlane_header(context, layout, swimlane, index, lane_collapsed);
            if (!layout->swimlane_drag_area && layout->swimlane_drag_handle)
                layout->swimlane_drag_handle(context, layout->hierarchy_drag_context, swimlane, index);
        }
        if (lane_collapsed) continue;
        /* Mobile mode: the lane as high as its stacked lists; the page scrolls. */
        if (layout->mobile_list_width > 0.0f)
            lane_height = (unsigned int)(wena_board_mobile_lane_height(layout, swimlane) + 30.0f);
        nk_layout_row_dynamic(context, (float)lane_height, 1);
        nk_style_push_style_item(context, &context->style.window.fixed_background,
                                 nk_style_item_color(nk_rgba(0, 0, 0, 0)));
        nk_style_push_vec2(context, &context->style.window.group_padding, nk_vec2(0.0f, 0.0f));
        nk_style_push_vec2(context, &context->style.window.spacing, nk_vec2(0.0f, 0.0f));
        if (wena_model_group_begin_flags(context, "lane/", layout->board->id,
                                    "", swimlane->id, 0)) {
            nk_style_pop_vec2(context);
            nk_style_pop_vec2(context);
            nk_style_pop_style_item(context);
            wena_render_lists(context, layout, swimlane, (float)lane_height - 14.0f);
            nk_group_end(context);
        } else {
            nk_style_pop_vec2(context);
            nk_style_pop_vec2(context);
            nk_style_pop_style_item(context);
        }
        if (layout->lists_view) break;
        if (layout->swimlane_resize_bar != NULL && layout->swimlane_resize != NULL &&
            layout->collapse != NULL)
            layout->swimlane_resize_bar(context, layout, swimlane, lane_height);
    }
    if (!layout->sidebar_as_window)
        (void)wena_board_sidebar_render(context, layout->sidebar);
    return 1;
}
