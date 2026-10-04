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

#endif
