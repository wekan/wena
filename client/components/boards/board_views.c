#include "board_views.h"
#include "../../../imports/ui/page_contract.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* WeKan's BOARD_VIEWS, in DEFAULT_BOARD_VIEW_ORDER, with SEPARATOR_AFTER and
 * the chart key each report view draws (chartPlaceholderViews.jade). */
static const WenaBoardView views[] = {
    {"board-view-swimlanes", "swimlanes", WENA_ICON_GRID, 0, NULL},
    {"board-view-lists", "board-view-lists", WENA_ICON_LIST, 0, NULL},
    {"board-view-table", "board-view-table", WENA_ICON_TABLE, 1, NULL},
    {"board-view-cal", "board-view-cal", WENA_ICON_CALENDAR, 0, NULL},
    {"board-view-multiboard-cal", "board-view-multiboard-cal", WENA_ICON_CALENDAR, 0, NULL},
    {"board-view-time", "board-view-time", WENA_ICON_CLOCK, 0, NULL},
    {"board-view-timeline", "board-view-timeline", WENA_ICON_HISTORY, 1, NULL},
    {"board-view-stats", "board-view-stats", WENA_ICON_PIE_CHART, 1, NULL},
    {"board-view-group-by-assignee", "board-view-group-by-assignee", WENA_ICON_USERS, 1, NULL},
    {"board-view-gantt", "board-view-gantt", WENA_ICON_BAR_CHART, 0, NULL},
    {"board-view-gantt-frappe", "board-view-gantt-frappe", WENA_ICON_TASKS, 0, NULL},
    {"board-view-gantt-dhtmlx", "board-view-gantt-dhtmlx", WENA_ICON_LIST, 1, NULL},
    {"board-view-product-backlog", "board-view-product-backlog", WENA_ICON_LIST, 0, NULL},
    {"board-view-sprints", "board-view-sprints", WENA_ICON_REFRESH, 0, NULL},
    {"board-view-sprint-report", "board-view-sprint-report", WENA_ICON_BAR_CHART, 0, NULL},
    {"board-view-velocity", "board-view-velocity", WENA_ICON_LINE_CHART, 0, NULL},
    {"board-view-roadmap", "board-view-roadmap", WENA_ICON_ROAD, 0, NULL},
    {"board-view-dashboard", "board-view-dashboard", WENA_ICON_TACHOMETER, 0, "dashboard"},
    {"board-view-bigboard", "board-view-bigboard", WENA_ICON_GRID, 1, NULL},
    {"board-view-burndown", "board-view-burndown", WENA_ICON_LINE_CHART, 0, "burndown"},
    {"board-view-burnup", "board-view-burnup", WENA_ICON_AREA_CHART, 0, "burnup"},
    {"board-view-cumulative-flow", "board-view-cumulative-flow", WENA_ICON_BAR_CHART, 0, "cumulativeFlow"},
    {"board-view-control-chart", "board-view-control-chart", WENA_ICON_LINE_CHART, 0, "controlChart"},
    {"board-view-cycle-time", "board-view-cycle-time", WENA_ICON_REFRESH, 0, "cycleTime"},
    {"board-view-flow-efficiency", "board-view-flow-efficiency", WENA_ICON_PIE_CHART, 0, "flowEfficiency"},
    {"board-view-lead-time", "board-view-lead-time", WENA_ICON_CLOCK, 0, "leadTime"},
    {"board-view-throughput-histogram", "board-view-throughput-histogram", WENA_ICON_BAR_CHART, 0, "throughputHistogram"},
    {"board-view-wip-run", "board-view-wip-run", WENA_ICON_LINE_CHART, 0, "wipRun"},
    {"board-view-pulse", "board-view-pulse", WENA_ICON_HEARTBEAT, 0, "pulse"},
    {"board-view-aging-wip", "board-view-aging-wip", WENA_ICON_CLOCK, 0, "agingWip"},
    {"board-view-blocker-analysis", "board-view-blocker-analysis", WENA_ICON_BAN, 0, "blockerAnalysis"},
    {"board-view-monte-carlo", "board-view-monte-carlo", WENA_ICON_BAR_CHART, 0, "monteCarlo"},
    {"board-view-process-behavior", "board-view-process-behavior", WENA_ICON_LINE_CHART, 0, "processBehavior"},
    {"board-view-size-cycle-time", "board-view-size-cycle-time", WENA_ICON_LINE_CHART, 0, "sizeCycleTime"},
    {"board-view-map", "board-view-map", WENA_ICON_MAP_MARKER, 0, NULL}};

const WenaBoardView *wena_board_views(size_t *count)
{
    if (count != NULL) *count = sizeof(views) / sizeof(views[0]);
    return views;
}

size_t wena_board_view_index(const char *key)
{
    size_t i;
    for (i = 0; key != NULL && i < sizeof(views) / sizeof(views[0]); ++i)
        if (!strcmp(views[i].key, key)) return i;
    return WENA_BOARD_VIEW_SWIMLANES;
}

const char *wena_board_view_name(size_t index)
{
    if (index >= sizeof(views) / sizeof(views[0])) index = WENA_BOARD_VIEW_SWIMLANES;
    return wena_ui_key_text(views[index].label_key, NULL);
}

/* Charts ------------------------------------------------------------------ */

#define PLOT_HEIGHT 260.0f
#define ROW_HEIGHT 22.0f
#define BLUE 0x13498db      /* boardCharts.js BAR_COLOR */
#define FLOW_BLUE 0x12878b5 /* flowChartConfig.js */
#define FLOW_RED 0x1c0392b
#define FLOW_GREEN 0x123834d

static struct nk_color rgb(int color)
{
    return nk_rgb((color >> 16) & 255, (color >> 8) & 255, color & 255);
}

static void text_at(struct nk_context *context, float x, float y, float w, const char *text, int color, int font)
{
    const struct nk_user_font *face = wena_wekan_font(context, (WenaWekanFont)font);
    struct nk_command_buffer *out = nk_window_get_canvas(context);
    if (face == NULL || out == NULL || text == NULL) return;
    nk_draw_text(out, nk_rect(x, y, w, face->height + 2.0f), text, (int)strlen(text), face, nk_rgba(0, 0, 0, 0), rgb(color));
}

/* One chart: the axis from 0 to the largest value, bars or points, the
 * lines over them, labels under them where they fit. */
static void plot(struct nk_context *context, const WenaChartPlot *p, int flow)
{
    struct nk_rect area;
    struct nk_command_buffer *out;
    double top = 0.0, bottom = 0.0, low_x = 0.0, high_x = 0.0;
    float left, width, height, base, step;
    size_t i, l, label_every;
    char tick[32];
    nk_layout_row_dynamic(context, PLOT_HEIGHT, 1);
    area = nk_widget_bounds(context);
    nk_label(context, "", NK_TEXT_LEFT);
    out = nk_window_get_canvas(context);
    if (out == NULL || p->count == 0) return;
    wena_wekan_fill(context, area.x, area.y, area.w, area.h, 0x1ffffff, 3.0f);
    for (i = 0; i < p->count; ++i) {
        if (p->has_value[i]) { if (p->values[i] > top) top = p->values[i]; if (p->values[i] < bottom) bottom = p->values[i]; }
        for (l = 0; l < p->line_count; ++l)
            if (p->line_set[l][i]) {
                if (p->lines[l][i] > top) top = p->lines[l][i];
                if (p->lines[l][i] < bottom) bottom = p->lines[l][i];
            }
        if (p->kind == WENA_CHART_SCATTER) {
            if (i == 0 || p->xs[i] < low_x) low_x = p->xs[i];
            if (i == 0 || p->xs[i] > high_x) high_x = p->xs[i];
        }
    }
    if (top <= bottom) top = bottom + 1.0;
    left = area.x + 44.0f;
    width = area.w - 56.0f;
    height = area.h - 48.0f;
    base = area.y + 12.0f + height;
    /* The y axis: 0 (or the lowest value) and the largest, beginAtZero. */
    nk_stroke_line(out, left, area.y + 12.0f, left, base, 1.0f, nk_rgb(0xcc, 0xcc, 0xcc));
    nk_stroke_line(out, left, base + (float)(bottom / (top - bottom)) * height, left + width,
                   base + (float)(bottom / (top - bottom)) * height, 1.0f, nk_rgb(0xcc, 0xcc, 0xcc));
    wena_chart_number(top, tick, sizeof(tick));
    text_at(context, area.x + 4.0f, area.y + 6.0f, 38.0f, tick, WENA_WEKAN_ICON, WENA_WEKAN_FONT_SMALL);
    wena_chart_number(bottom, tick, sizeof(tick));
    text_at(context, area.x + 4.0f, base - 8.0f, 38.0f, tick, WENA_WEKAN_ICON, WENA_WEKAN_FONT_SMALL);
#define Y(v) (base - (float)(((v) - bottom) / (top - bottom)) * height)
    step = width / (float)p->count;
    label_every = (size_t)(70.0f / (step > 1.0f ? step : 1.0f)) + 1;
    for (i = 0; i < p->count; ++i) {
        float x = left + step * (float)i, mid = x + step / 2.0f;
        int color = p->signal[i] ? FLOW_RED : p->green ? FLOW_GREEN : flow ? FLOW_BLUE : BLUE;
        if (p->kind == WENA_CHART_SCATTER) {
            float sx = left + (high_x > low_x ? (float)((p->xs[i] - low_x) / (high_x - low_x)) * width : width / 2.0f);
            if (p->has_value[i]) nk_fill_circle(out, nk_rect(sx - 3.0f, Y(p->values[i]) - 3.0f, 6.0f, 6.0f), rgb(color));
            continue;
        }
        if (p->kind == WENA_CHART_LINE) {
            if (p->has_value[i]) {
                if (i > 0 && p->has_value[i - 1])
                    nk_stroke_line(out, mid - step, Y(p->values[i - 1]), mid, Y(p->values[i]), 2.0f, rgb(FLOW_BLUE));
                nk_fill_circle(out, nk_rect(mid - 2.5f, Y(p->values[i]) - 2.5f, 5.0f, 5.0f), rgb(color));
            }
        } else if (p->has_value[i]) {
            float bar = step > 6.0f ? step * 0.8f : step;
            nk_fill_rect(out, nk_rect(mid - bar / 2.0f, Y(p->values[i] > 0.0 ? p->values[i] : 0.0), bar,
                                      (float)(fabs(p->values[i]) / (top - bottom)) * height), 0.0f, rgb(color));
        }
        if (i % label_every == 0)
            text_at(context, x, base + 6.0f, step * (float)label_every, p->labels[i], WENA_WEKAN_ICON, WENA_WEKAN_FONT_SMALL);
    }
    /* The lines over it: a threshold, the mean and the limits. */
    for (l = 0; l < p->line_count; ++l) {
        int color = l == 0 ? FLOW_GREEN : FLOW_RED;
        for (i = 1; i < p->count; ++i)
            if (p->line_set[l][i] && p->line_set[l][i - 1])
                nk_stroke_line(out, left + step * ((float)i - 0.5f), Y(p->lines[l][i - 1]), left + step * ((float)i + 0.5f),
                               Y(p->lines[l][i]), 1.5f, rgb(color));
    }
#undef Y
    /* The legend: what the bars and the lines are. */
    {
        float x = left;
        if (p->value_label[0]) {
            wena_wekan_fill(context, x, area.y + area.h - 16.0f, 10.0f, 10.0f, p->green ? FLOW_GREEN : flow ? FLOW_BLUE : BLUE, 0.0f);
            text_at(context, x + 14.0f, area.y + area.h - 19.0f, 200.0f, p->value_label, WENA_WEKAN_TEXT, WENA_WEKAN_FONT_SMALL);
            x += 220.0f;
        }
        for (l = 0; l < p->line_count && p->line_labels[l][0]; ++l) {
            wena_wekan_fill(context, x, area.y + area.h - 12.0f, 14.0f, 2.0f, l == 0 ? FLOW_GREEN : FLOW_RED, 0.0f);
            text_at(context, x + 18.0f, area.y + area.h - 19.0f, 160.0f, p->line_labels[l], WENA_WEKAN_TEXT,
                    WENA_WEKAN_FONT_SMALL);
            x += 180.0f;
        }
    }
}

/* A table, as WeKan's .stats-view-table: a bold header row, then the rows;
 * only the rows in sight are laid out. */
static void table(struct nk_context *context, const WenaChartTable *t)
{
    struct nk_rect clip, header;
    size_t i, c, first, last;
    float stride, top;
    if (t->columns == 0) return;
    nk_layout_row_dynamic(context, ROW_HEIGHT + 4.0f, (int)t->columns);
    header = nk_widget_bounds(context);
    for (c = 0; c < t->columns; ++c) wena_wekan_text(context, t->headers[c], WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    if (t->row_count == 0) return;
    clip = context->current->layout->clip;
    stride = ROW_HEIGHT + context->style.window.spacing.y;
    top = header.y + header.h + context->style.window.spacing.y;
    first = top < clip.y ? (size_t)((clip.y - top) / stride) : 0;
    last = clip.y + clip.h > top ? (size_t)((clip.y + clip.h - top) / stride) + 2 : 0;
    if (first > t->row_count) first = t->row_count;
    if (last > t->row_count) last = t->row_count;
    if (last < first) last = first;
    /* The rows out of sight are one empty row of their height each side. */
    if (first > 0) { nk_layout_row_dynamic(context, stride * (float)first - context->style.window.spacing.y, 1); nk_label(context, "", NK_TEXT_LEFT); }
    for (i = first; i < last; ++i) {
        nk_layout_row_dynamic(context, ROW_HEIGHT, (int)t->columns);
        for (c = 0; c < t->columns; ++c) wena_wekan_text(context, t->rows[i][c], WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    }
    if (last < t->row_count) {
        nk_layout_row_dynamic(context, stride * (float)(t->row_count - last) - context->style.window.spacing.y, 1);
        nk_label(context, "", NK_TEXT_LEFT);
    }
}

unsigned int wena_board_chart_render(struct nk_context *context, const char *title, const char *method_note,
                                     const WenaChartResult *result)
{
    int flow;
    if (context == NULL || context->current == NULL || result == NULL) return WENA_BOARD_VIEW_NO_ACTION;
    wena_ui_region("chart");
    flow = method_note != NULL;
    nk_layout_row_dynamic(context, 34.0f, 1);
    wena_wekan_text(context, title, WENA_WEKAN_FONT_SECTION, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
    if (method_note != NULL && method_note[0]) {
        nk_layout_row_dynamic(context, 18.0f * (float)wena_wekan_wrapped_lines(context, method_note, WENA_WEKAN_FONT_SMALL,
                                                                                 context->current->layout->bounds.w - 20.0f) + 4.0f, 1);
        wena_wekan_text_wrap(context, method_note, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_ICON);
    }
    if (result->table.row_count == 0) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        wena_wekan_text(context, wena_ui_key_text("no-results", NULL), WENA_WEKAN_FONT_BODY, WENA_WEKAN_ICON, NK_TEXT_LEFT);
        return WENA_BOARD_VIEW_NO_ACTION;
    }
    plot(context, &result->plot, flow);
    if (result->has_second) plot(context, &result->second, 1);
    if (result->note[0]) {
        nk_layout_row_dynamic(context, 18.0f * (float)wena_wekan_wrapped_lines(context, result->note, WENA_WEKAN_FONT_SMALL,
                                                                                 context->current->layout->bounds.w - 20.0f) + 6.0f, 1);
        wena_wekan_text_wrap(context, result->note, WENA_WEKAN_FONT_SMALL, WENA_WEKAN_TEXT);
    }
    table(context, &result->table);
    if (result->detail.row_count > 0) {
        nk_layout_row_dynamic(context, 30.0f, 1);
        wena_wekan_text(context, wena_ui_key_text("flow-details", NULL), WENA_WEKAN_FONT_BOLD, WENA_WEKAN_TEXT, NK_TEXT_LEFT);
        table(context, &result->detail);
    }
    return WENA_BOARD_VIEW_NO_ACTION;
}
