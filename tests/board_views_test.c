/* WeKan's board views (client/components/boards/board_views.c): the menu's
 * entries and the drawing of a report chart. */
#include "../client/components/boards/board_views.h"
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct nk_context context;

static int control(const char *name)
{
    size_t count, i;
    const WenaUiControl *controls = wena_ui_controls(&count);
    for (i = 0; i < count; ++i) if (!strcmp(controls[i].name, name)) return 1;
    return 0;
}

int main(void)
{
    static const char *const separated[] = {"board-view-table", "board-view-timeline", "board-view-stats",
                                            "board-view-group-by-assignee", "board-view-gantt-dhtmlx", "board-view-bigboard"};
    size_t count, i, j, separators = 0, charts = 0;
    const WenaBoardView *views = wena_board_views(&count);
    WenaViewData data;
    WenaChartResult result;
    WenaViewCard cards[2];

    /* WeKan's 35 views in its order, Swimlanes first, Map last, its six
     * separators, every chart a chart WeKan computes. */
    assert(count == 35 && !strcmp(views[0].key, "board-view-swimlanes") && !strcmp(views[1].key, "board-view-lists") &&
           !strcmp(views[34].key, "board-view-map"));
    memset(&data, 0, sizeof(data));
    for (i = 0; i < count; ++i) {
        assert(!strncmp(views[i].key, "board-view-", 11) && wena_board_view_index(views[i].key) == i);
        for (j = 0; j < i; ++j) assert(strcmp(views[i].key, views[j].key));
        if (views[i].separator_after) {
            int listed = 0;
            for (j = 0; j < 6; ++j) listed |= !strcmp(views[i].key, separated[j]);
            assert(listed);
            ++separators;
        }
        if (views[i].chart != NULL) {
            assert(wena_chart_compute(views[i].chart, &data, 1790000000000.0, NULL, NULL, &result));
            wena_chart_result_free(&result);
            ++charts;
        }
    }
    assert(separators == 6 && charts == 16);
    /* Names: Swimlanes is WeKan's "swimlanes"; negative: an unknown key is
     * Swimlanes, as WeKan's resolveBoardView. */
    assert(!strcmp(wena_board_view_name(0), "Swimlanes") && !strcmp(wena_board_view_name(wena_board_view_index("board-view-burndown")), "Burndown"));
    assert(wena_board_view_index("board-view-nothing") == WENA_BOARD_VIEW_SWIMLANES && wena_board_view_index(NULL) == 0);
    assert(!strcmp(wena_board_view_name(1000), "Swimlanes"));

    /* A chart drawn: its title, its note, its table's headers and cells. */
    memset(cards, 0, sizeof(cards));
    strcpy(cards[0].id, "c1"); strcpy(cards[0].title, "Done card");
    cards[0].created_at.set = 1; cards[0].created_at.ms = 1789000000000.0;
    cards[0].end_at.set = 1; cards[0].end_at.ms = 1789500000000.0;
    strcpy(cards[1].id, "c2"); strcpy(cards[1].title, "Open card");
    cards[1].created_at.set = 1; cards[1].created_at.ms = 1789000000000.0;
    data.cards = cards;
    data.card_count = 2;
    assert(wena_chart_compute("throughputHistogram", &data, 1790000000000.0, NULL, NULL, &result));
    context.label_count = 0;
    assert(result.table.row_count == 1 && result.plot.count == 1 && result.note[0]);
    wena_ui_controls_begin();
    assert(nk_begin(&context, "view", nk_rect(0, 0, 1024, 720), 0));
    context.current->layout->clip = nk_rect(0, 0, 1024, 720);
    (void)wena_board_chart_render(&context, "Throughput Histogram", NULL, &result);
    nk_end(&context);
    assert(control("Throughput Histogram"));
    assert(control("Cards"));
    /* The note is wrapped text: a label. */
    for (i = 0, j = 0; i < (size_t)context.label_count; ++i) j |= !strcmp(context.labels[i], result.note);
    assert(j);
    assert(control("1"));
    wena_chart_result_free(&result);
    /* Negative: no rows is WeKan's "no results", and no table. */
    data.card_count = 0;
    assert(wena_chart_compute("controlChart", &data, 1790000000000.0, NULL, NULL, &result));
    context.label_count = 0;
    wena_ui_controls_begin();
    assert(nk_begin(&context, "view", nk_rect(0, 0, 1024, 720), 0));
    (void)wena_board_chart_render(&context, "Control Chart", "A note.", &result);
    nk_end(&context);
    for (i = 0, j = 0; i < (size_t)context.label_count; ++i) j |= !strcmp(context.labels[i], "A note.");
    assert(control("Control Chart") && j && !control("Card"));
    wena_chart_result_free(&result);
    assert(wena_board_chart_render(NULL, "x", NULL, &result) == WENA_BOARD_VIEW_NO_ACTION);
    return 0;
}
