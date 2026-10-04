#ifndef WENA_BOARD_VIEWS_H
#define WENA_BOARD_VIEWS_H
/* WeKan's board views (models/lib/boardViewSettings.js BOARD_VIEWS): the
 * Board View menu's entries in WeKan's order, with their names, icons and
 * the separators between the groups, and the drawing of the views that are
 * not the board's lists - the report charts (client/components/boards/
 * charts/boardCharts.jade: the title, the method note, the chart and the
 * data table under it) and the others. */
#include <stddef.h>
#include "../common/wekan_look.h"
#include "../../../models/charts.h"
#include "../../../models/view_rows.h"

struct nk_context;

typedef struct WenaBoardView {
    const char *key;          /* users.profile.boardView: "board-view-table" ... */
    const char *label_key;    /* its name's translation key */
    WenaIcon icon;
    int separator_after;      /* a line under it in the menu */
    const char *chart;        /* a report chart's key (charts.h), else NULL */
} WenaBoardView;

#define WENA_BOARD_VIEW_SWIMLANES 0
#define WENA_BOARD_VIEW_LISTS 1

const WenaBoardView *wena_board_views(size_t *count);
/* The view of a key, WENA_BOARD_VIEW_SWIMLANES for one WeKan does not have
 * (WeKan's resolveBoardView falls back the same way). */
size_t wena_board_view_index(const char *key);
/* A view's name in the user's language. */
const char *wena_board_view_name(size_t index);

/* What a drawn view asks for. */
#define WENA_BOARD_VIEW_NO_ACTION 0u
#define WENA_BOARD_VIEW_OPEN_CARD 1u      /* its id in `card` */

/* A report chart as WeKan draws it, in the window's remaining space: the
 * title, the method note, the chart (and a second one), the note under it,
 * the data table and the details. `scroll` keeps the view's position. */
unsigned int wena_board_chart_render(struct nk_context *context, const char *title, const char *method_note,
                                     const WenaChartResult *result);

/* What a view keeps between frames: the Table's search, sort, page and
 * grouping, the Calendar's day and view, the Timeline's chosen time, the
 * Gantt's scale, the chosen sprint and the Roadmap's field. */
typedef struct WenaBoardViewState {
    char search[129];
    int search_length;
    char query[129];
    WenaTableField sort_field;
    int sort_descending;
    int page;
    int group_by_swimlane;
    long calendar_day;        /* a local day; 0 for today */
    int calendar_mode;        /* 0 day, 1 week, 2 month, 3 list, as WeKan's buttons */
    int has_selected_time;
    double selected_time;
    int gantt_mode;           /* 0 day, 1 week, 2 month */
    char sprint_id[WENA_VIEW_ID];
    char field_id[WENA_VIEW_ID];
} WenaBoardViewState;

/* One of the views that are not the lists or a chart, as WeKan draws it.
 * `all` holds every board's cards for the calendar of every board and
 * Bigboard (wena_wekan_views_load_all). A card clicked: OPEN_CARD with its
 * id in `card`. */
unsigned int wena_board_view_render(struct nk_context *context, size_t index, WenaBoardViewState *state,
                                    const WenaViewData *data, const WenaViewData *all, double now, float height,
                                    char *card, size_t capacity);

#endif
