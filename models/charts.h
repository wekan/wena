#ifndef WENA_MODELS_CHARTS_H
#define WENA_MODELS_CHARTS_H
/* WeKan's board report charts, ported from models/lib/chartCalculations.js,
 * models/lib/flowAnalytics.js, models/lib/chartExportRows.js and
 * models/lib/flowAnalyticsRows.js: for each chart its plot - the bars or
 * points the chart draws (client/components/boards/charts/boardCharts.js
 * computeBarRows and flowChartConfig.js) - a note, and the data table WeKan
 * shows under it and exports. The numbers are WeKan's: a card is completed
 * at its endAt, else its archivedAt; days are UTC days. */
#include "view_data.h"

#define WENA_CHART_COLUMNS 12
#define WENA_CHART_CELL 96
#define WENA_CHART_POINTS 200
#define WENA_CHART_BARS 24        /* boardCharts.js MAX_BARS */
#define WENA_CHART_LINES 3

typedef enum WenaChartKind {
    WENA_CHART_BAR,
    WENA_CHART_LINE,
    WENA_CHART_SCATTER
} WenaChartKind;

typedef struct WenaChartPlot {
    WenaChartKind kind;
    size_t count;
    char labels[WENA_CHART_POINTS][64];
    double values[WENA_CHART_POINTS];
    int has_value[WENA_CHART_POINTS];
    int signal[WENA_CHART_POINTS];        /* drawn red: above a limit */
    double xs[WENA_CHART_POINTS];         /* SCATTER: the x value */
    char value_label[64];
    int green;                            /* drawn green: Monte Carlo's capacity */
    size_t line_count;                    /* lines drawn over it: limits, thresholds */
    char line_labels[WENA_CHART_LINES][64];
    double lines[WENA_CHART_LINES][WENA_CHART_POINTS];
    int line_set[WENA_CHART_LINES][WENA_CHART_POINTS];
} WenaChartPlot;

typedef char WenaChartRow[WENA_CHART_COLUMNS][WENA_CHART_CELL];
typedef struct WenaChartTable {
    size_t columns;
    char headers[WENA_CHART_COLUMNS][WENA_CHART_CELL];
    WenaChartRow *rows;
    size_t row_count, row_capacity;
} WenaChartTable;

typedef struct WenaChartResult {
    WenaChartPlot plot;
    WenaChartPlot second;                 /* Monte Carlo capacity, the moving range */
    int has_second;
    char note[512];                       /* the forecast, a method note */
    WenaChartTable table;
    WenaChartTable detail;                /* WeKan's "Details" under it */
} WenaChartResult;

/* Monte Carlo's options (flowAnalytics.js flowOptions): WeKan's defaults are
 * 10 cards, 90 days of history and a target date 30 days on. */
typedef struct WenaChartOptions {
    int target_count;
    int history_days;
    double target_date;                   /* ms at the day's start, UTC; 0 for 30 days on */
} WenaChartOptions;

/* WeKan's translation: the text of `key`, else `fallback`. */
typedef const char *(*WenaChartText)(const char *key, const char *fallback);

/* The chart keys WeKan has: dashboard, burndown, burnup, cumulativeFlow,
 * controlChart, cycleTime, leadTime, flowEfficiency, throughputHistogram,
 * wipRun, pulse, agingWip, blockerAnalysis, monteCarlo, processBehavior,
 * sizeCycleTime. Returns 0 for another key or no memory. */
int wena_chart_compute(const char *chart, const WenaViewData *data, double now,
                       const WenaChartOptions *options, WenaChartText text, WenaChartResult *result);
void wena_chart_result_free(WenaChartResult *result);

/* WeKan's __name__ placeholders replaced with values; unused pairs NULL. */
void wena_chart_format(char *out, size_t capacity, const char *format, const char *name1, const char *value1,
                       const char *name2, const char *value2, const char *name3, const char *value3);

/* Shared with the other views. */
int wena_chart_completion(const WenaViewCard *card, double *at);
/* "YYYY-MM-DD" of a UTC day: ms / 86400000. */
void wena_chart_day_key(long day, char out[11]);
long wena_chart_day(double ms);
/* WeKan's Math.round(value * 100) / 100, printed as JavaScript prints it. */
void wena_chart_number(double value, char *out, size_t capacity);

#endif
