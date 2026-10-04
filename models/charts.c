#include "charts.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DAY_MS 86400000.0

/* Times and numbers -------------------------------------------------------- */

long wena_chart_day(double ms)
{
    return (long)floor(ms / DAY_MS);
}

/* Howard Hinnant's civil_from_days: the proleptic Gregorian date of a day. */
static void civil(long z, long *year, int *month, int *day)
{
    long era, doe, yoe, doy, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *day = (int)(doy - (153 * mp + 2) / 5 + 1);
    *month = (int)(mp < 10 ? mp + 3 : mp - 9);
    *year = yoe + era * 400 + (*month <= 2);
}

void wena_chart_day_key(long day, char out[11])
{
    long year;
    int month, mday;
    civil(day, &year, &month, &mday);
    /* Ranges spelled out so the 11 bytes provably hold any result. */
    sprintf(out, "%04lu-%02u-%02u", (unsigned long)((year % 10000 + 10000) % 10000),
            (unsigned)month % 13u, (unsigned)mday % 32u);
}

/* A Date cell: the UTC day and minute. */
static void date_cell(double ms, char *out, size_t capacity)
{
    char key[11];
    long day = wena_chart_day(ms);
    double minutes = floor((ms - (double)day * DAY_MS) / 60000.0);
    wena_chart_day_key(day, key);
    if (capacity > 18) sprintf(out, "%s %02d:%02d", key, (int)(minutes / 60.0), (int)fmod(minutes, 60.0));
    else if (capacity > 0) out[0] = '\0';
}

void wena_chart_number(double value, char *out, size_t capacity)
{
    char text[64];
    size_t length;
    double rounded = floor(value * 100.0 + 0.5) / 100.0;
    if (out == NULL || capacity == 0) return;
    if (!(rounded == rounded)) { strcpy(text, "NaN"); }
    else {
        sprintf(text, "%.2f", rounded);
        length = strlen(text);
        while (length > 0 && text[length - 1] == '0') text[--length] = '\0';
        if (length > 0 && text[length - 1] == '.') text[--length] = '\0';
        if (!strcmp(text, "-0")) strcpy(text, "0");
    }
    if (strlen(text) >= capacity) text[capacity - 1] = '\0';
    strcpy(out, text);
}

void wena_chart_format(char *out, size_t capacity, const char *format, const char *name1, const char *value1,
                       const char *name2, const char *value2, const char *name3, const char *value3)
{
    const char *names[3], *values[3];
    size_t used = 0, k;
    names[0] = name1; names[1] = name2; names[2] = name3;
    values[0] = value1; values[1] = value2; values[2] = value3;
    if (out == NULL || capacity == 0) return;
    while (format != NULL && *format && used + 1 < capacity) {
        int replaced = 0;
        if (format[0] == '_' && format[1] == '_')
            for (k = 0; k < 3; ++k) {
                size_t length = names[k] != NULL ? strlen(names[k]) : 0;
                if (length && !strncmp(format + 2, names[k], length) && !strncmp(format + 2 + length, "__", 2)) {
                    size_t value_length = strlen(values[k]);
                    if (used + value_length >= capacity) value_length = capacity - used - 1;
                    memcpy(out + used, values[k], value_length);
                    used += value_length;
                    format += length + 4;
                    replaced = 1;
                    break;
                }
            }
        if (!replaced) out[used++] = *format++;
    }
    out[used] = '\0';
}

static double round2(double value)
{
    return floor(value * 100.0 + 0.5) / 100.0;
}

int wena_chart_completion(const WenaViewCard *card, double *at)
{
    if (card == NULL) return 0;
    if (card->end_at.set) { if (at) *at = card->end_at.ms; return 1; }
    if (card->archived_at.set) { if (at) *at = card->archived_at.ms; return 1; }
    return 0;
}

/* The table and the plot -------------------------------------------------- */

static void copy(char *out, size_t capacity, const char *text)
{
    size_t length = text != NULL ? strlen(text) : 0;
    if (capacity == 0) return;
    if (length >= capacity) {
        length = capacity - 1;
        while (length > 0 && ((unsigned char)text[length] & 0xC0u) == 0x80u) --length;
    }
    if (length) memcpy(out, text, length);
    out[length] = '\0';
}

static void headers(WenaChartTable *table, size_t count, const char *const *names)
{
    size_t i;
    table->columns = count > WENA_CHART_COLUMNS ? WENA_CHART_COLUMNS : count;
    for (i = 0; i < table->columns; ++i) copy(table->headers[i], WENA_CHART_CELL, names[i]);
}

/* A new row, empty; NULL when out of memory. */
static WenaChartRow *row(WenaChartTable *table)
{
    if (table->row_count == table->row_capacity) {
        size_t capacity = table->row_capacity ? table->row_capacity * 2 : 32;
        WenaChartRow *rows = (WenaChartRow *)realloc(table->rows, capacity * sizeof(*rows));
        if (rows == NULL) return NULL;
        table->rows = rows;
        table->row_capacity = capacity;
    }
    memset(table->rows[table->row_count], 0, sizeof(WenaChartRow));
    return &table->rows[table->row_count++];
}

static void cell_text(WenaChartRow *r, size_t column, const char *text)
{
    if (r != NULL && column < WENA_CHART_COLUMNS) copy((*r)[column], WENA_CHART_CELL, text);
}

static void cell_number(WenaChartRow *r, size_t column, double value)
{
    char text[64];
    wena_chart_number(value, text, sizeof(text));
    cell_text(r, column, text);
}

static void cell_int(WenaChartRow *r, size_t column, long value)
{
    char text[32];
    sprintf(text, "%ld", value);
    cell_text(r, column, text);
}

/* A point of the plot, keeping the last `cap` as WeKan keeps the most recent
 * bars (boardCharts.js barsFromSeries). */
static size_t point(WenaChartPlot *plot, size_t cap, const char *label, double value, int has_value)
{
    size_t index;
    if (cap > WENA_CHART_POINTS) cap = WENA_CHART_POINTS;
    if (plot->count == cap) {
        size_t line;
        memmove(plot->labels, plot->labels + 1, (cap - 1) * sizeof(plot->labels[0]));
        memmove(plot->values, plot->values + 1, (cap - 1) * sizeof(plot->values[0]));
        memmove(plot->has_value, plot->has_value + 1, (cap - 1) * sizeof(plot->has_value[0]));
        memmove(plot->signal, plot->signal + 1, (cap - 1) * sizeof(plot->signal[0]));
        memmove(plot->xs, plot->xs + 1, (cap - 1) * sizeof(plot->xs[0]));
        for (line = 0; line < WENA_CHART_LINES; ++line) {
            memmove(plot->lines[line], plot->lines[line] + 1, (cap - 1) * sizeof(double));
            memmove(plot->line_set[line], plot->line_set[line] + 1, (cap - 1) * sizeof(int));
        }
        --plot->count;
    }
    index = plot->count++;
    copy(plot->labels[index], sizeof(plot->labels[index]), label);
    plot->values[index] = round2(value);
    plot->has_value[index] = has_value;
    plot->signal[index] = 0;
    plot->xs[index] = 0.0;
    {
        size_t line;
        for (line = 0; line < WENA_CHART_LINES; ++line) plot->line_set[line][index] = 0;
    }
    return index;
}

static void line(WenaChartPlot *plot, size_t which, size_t index, double value)
{
    plot->lines[which][index] = value;
    plot->line_set[which][index] = 1;
}

#define T(key, fallback) (text != NULL ? text(key, fallback) : (fallback))

/* Array.prototype.sort is stable: equal keys keep their order. A merge sort
 * does the same. */
static void merge_pass(char *base, char *temp, size_t size, size_t from, size_t middle, size_t to,
                       int (*compare)(const void *, const void *))
{
    size_t i = from, j = middle, k = from;
    while (i < middle && j < to) {
        if (compare(base + j * size, base + i * size) < 0) { memcpy(temp + k * size, base + j * size, size); ++j; }
        else { memcpy(temp + k * size, base + i * size, size); ++i; }
        ++k;
    }
    while (i < middle) { memcpy(temp + k * size, base + i * size, size); ++i; ++k; }
    while (j < to) { memcpy(temp + k * size, base + j * size, size); ++j; ++k; }
    memcpy(base + from * size, temp + from * size, (to - from) * size);
}

static void stable_sort(void *items, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    char *temp;
    size_t width, from;
    if (count < 2) return;
    temp = (char *)malloc(count * size);
    if (temp == NULL) return;
    for (width = 1; width < count; width *= 2)
        for (from = 0; from + width < count; from += 2 * width)
            merge_pass((char *)items, temp, size, from, from + width,
                       from + 2 * width < count ? from + 2 * width : count, compare);
    free(temp);
}

/* Ids, for replaying events: a sorted index of the ids seen. */
typedef struct Ids {
    const char **ids;
    size_t count;
} Ids;

static int compare_ids(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int ids_build(Ids *ids, const WenaViewData *data)
{
    size_t i, used = 0;
    ids->ids = (const char **)malloc((data->card_count + data->activity_count + data->change_count + 1) *
                                     sizeof(*ids->ids));
    if (ids->ids == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) ids->ids[used++] = data->cards[i].id;
    for (i = 0; i < data->activity_count; ++i)
        if (data->activities[i].card_id[0]) ids->ids[used++] = data->activities[i].card_id;
    for (i = 0; i < data->change_count; ++i)
        if (data->changes[i].card_id[0]) ids->ids[used++] = data->changes[i].card_id;
    qsort(ids->ids, used, sizeof(*ids->ids), compare_ids);
    ids->count = 0;
    for (i = 0; i < used; ++i)
        if (ids->count == 0 || strcmp(ids->ids[ids->count - 1], ids->ids[i])) ids->ids[ids->count++] = ids->ids[i];
    return 1;
}

static long ids_find(const Ids *ids, const char *id)
{
    const char **found = (const char **)bsearch(&id, ids->ids, ids->count, sizeof(*ids->ids), compare_ids);
    return found != NULL ? (long)(found - ids->ids) : -1;
}

static long list_rank(const WenaViewData *data, const char *id)
{
    size_t i;
    for (i = 0; i < data->list_count; ++i) if (!strcmp(data->lists[i].id, id)) return (long)i;
    return -1;
}

/* The days the charts cover: from the board's first card to now. */
static long first_day(const WenaViewData *data, double now)
{
    size_t i;
    double first = 0.0;
    int seen = 0;
    for (i = 0; i < data->card_count; ++i)
        if (data->cards[i].created_at.set && (!seen || data->cards[i].created_at.ms < first)) {
            first = data->cards[i].created_at.ms;
            seen = 1;
        }
    return wena_chart_day(seen ? first : now);
}

/* Cumulative Flow and WIP Run: replayed list of every card ----------------- */

/* Each card's list and whether it is archived after the events up to the end
 * of `day`; `next` is the next event not yet applied. */
typedef struct Replay {
    Ids ids;
    long *list;               /* rank of the list, or -2 when unknown */
    char *archived;
    char *placed;
    size_t next;
} Replay;

static int replay_begin(Replay *replay, const WenaViewData *data)
{
    memset(replay, 0, sizeof(*replay));
    if (!ids_build(&replay->ids, data)) return 0;
    replay->list = (long *)calloc(replay->ids.count + 1, sizeof(long));
    replay->archived = (char *)calloc(replay->ids.count + 1, 1);
    replay->placed = (char *)calloc(replay->ids.count + 1, 1);
    return replay->list != NULL && replay->archived != NULL && replay->placed != NULL;
}

static void replay_end(Replay *replay)
{
    free(replay->ids.ids);
    free(replay->list);
    free(replay->archived);
    free(replay->placed);
}

static void replay_to(Replay *replay, const WenaViewData *data, long day)
{
    double end = ((double)day + 1.0) * DAY_MS - 1.0;
    while (replay->next < data->activity_count) {
        const WenaViewActivity *event = &data->activities[replay->next];
        long card;
        if (!event->at.set || event->at.ms > end) break;
        ++replay->next;
        card = ids_find(&replay->ids, event->card_id);
        if (card < 0) continue;
        if (!strcmp(event->type, "createCard") || !strcmp(event->type, "moveCard")) {
            replay->list[card] = list_rank(data, event->list_id);
            replay->placed[card] = 1;
        } else if (!strcmp(event->type, "archivedCard")) replay->archived[card] = 1;
        else if (!strcmp(event->type, "restoredCard")) replay->archived[card] = 0;
    }
}

static int cumulative_flow(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    Replay replay;
    long day, from = first_day(data, now), to = wena_chart_day(now);
    size_t i, l;
    const char *names[WENA_CHART_COLUMNS];
    long *counts;
    if (!replay_begin(&replay, data)) { replay_end(&replay); return 0; }
    counts = (long *)calloc(data->list_count + 1, sizeof(long));
    if (counts == NULL) { replay_end(&replay); return 0; }
    names[0] = T("date", "Date");
    for (l = 0; l < data->list_count && l + 1 < WENA_CHART_COLUMNS; ++l) names[l + 1] = data->lists[l].title;
    headers(&result->table, l + 1, names);
    for (day = from; day <= to; ++day) {
        WenaChartRow *r;
        char key[11];
        replay_to(&replay, data, day);
        memset(counts, 0, (data->list_count + 1) * sizeof(long));
        for (i = 0; i < replay.ids.count; ++i) {
            if (!replay.placed[i] || replay.archived[i] || replay.list[i] < 0) continue;
            for (l = 0; l < data->list_count; ++l) if ((long)l <= replay.list[i]) ++counts[l];
        }
        if ((r = row(&result->table)) == NULL) break;
        wena_chart_day_key(day, key);
        cell_text(r, 0, key);
        for (l = 0; l < data->list_count && l + 1 < WENA_CHART_COLUMNS; ++l) cell_int(r, l + 1, counts[l]);
    }
    /* The bars: the last day's count per list. */
    for (l = 0; l < data->list_count; ++l) point(&result->plot, WENA_CHART_BARS, data->lists[l].title, (double)counts[l], 1);
    free(counts);
    replay_end(&replay);
    return 1;
}

static int wip_run(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    Replay replay;
    long day, from = first_day(data, now), to = wena_chart_day(now), limit = 0;
    size_t i, l, first = 0, last = data->list_count;
    int has_limit = 0;
    const char *names[3];
    if (data->list_count > 2) {
        first = 1;
        last = data->list_count - 1;
        for (l = first; l < last; ++l) if (data->lists[l].wip_enabled) limit += data->lists[l].wip_value;
        has_limit = limit != 0;
    }
    if (!replay_begin(&replay, data)) { replay_end(&replay); return 0; }
    names[0] = T("date", "Date");
    names[1] = T("board-view-wip-run", "WIP");
    names[2] = T("wipLimit", "Limit");
    headers(&result->table, 3, names);
    for (day = from; day <= to; ++day) {
        WenaChartRow *r;
        char key[11];
        long count = 0;
        replay_to(&replay, data, day);
        for (i = 0; i < replay.ids.count; ++i)
            if (replay.placed[i] && !replay.archived[i] && replay.list[i] >= (long)first && replay.list[i] < (long)last)
                ++count;
        if ((r = row(&result->table)) == NULL) break;
        wena_chart_day_key(day, key);
        cell_text(r, 0, key);
        cell_int(r, 1, count);
        if (has_limit) cell_int(r, 2, limit); else cell_text(r, 2, "-");
        point(&result->plot, WENA_CHART_BARS, key, (double)count, 1);
    }
    replay_end(&replay);
    return 1;
}

/* Cards by completion ------------------------------------------------------ */

typedef struct Done {
    const WenaViewCard *card;
    double at, cycle, lead, average, stddev;
} Done;

static int by_completion(const void *a, const void *b)
{
    const Done *x = (const Done *)a, *y = (const Done *)b;
    return x->at < y->at ? -1 : x->at > y->at;
}

/* Completed cards by completion time, with cycle and lead days. */
static Done *completed(const WenaViewData *data, size_t *count)
{
    Done *done = (Done *)malloc((data->card_count + 1) * sizeof(*done));
    size_t i, n = 0;
    if (done == NULL) return NULL;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        double at, start;
        if (!wena_chart_completion(card, &at)) continue;
        start = card->start_at.set ? card->start_at.ms : card->created_at.ms;
        done[n].card = card;
        done[n].at = at;
        done[n].cycle = (at - start) / DAY_MS;
        if (done[n].cycle < 0.0) done[n].cycle = 0.0;
        done[n].lead = (at - card->created_at.ms) / DAY_MS;
        if (done[n].lead < 0.0) done[n].lead = 0.0;
        ++n;
    }
    stable_sort(done, n, sizeof(*done), by_completion);
    *count = n;
    return done;
}

static const char *title_of(const WenaViewCard *card)
{
    return card->title[0] ? card->title : card->id;
}

static int control_chart(const WenaViewData *data, WenaChartText text, WenaChartResult *result)
{
    size_t count, i, j;
    Done *done = completed(data, &count);
    const char *names[5];
    if (done == NULL) return 0;
    names[0] = T("card", "Card");
    names[1] = T("completed", "Completed");
    names[2] = T("board-view-cycle-time", "Cycle (days)");
    names[3] = "Average";
    names[4] = "StdDev";
    headers(&result->table, 5, names);
    for (i = 0; i < count; ++i) {
        size_t start = i >= 4 ? i - 4 : 0;
        double sum = 0.0, variance = 0.0;
        WenaChartRow *r;
        char when[32];
        for (j = start; j <= i; ++j) sum += done[j].cycle;
        done[i].average = sum / (double)(i - start + 1);
        for (j = start; j <= i; ++j) variance += (done[j].cycle - done[i].average) * (done[j].cycle - done[i].average);
        done[i].stddev = sqrt(variance / (double)(i - start + 1));
        if ((r = row(&result->table)) == NULL) break;
        date_cell(done[i].at, when, sizeof(when));
        cell_text(r, 0, title_of(done[i].card));
        cell_text(r, 1, when);
        cell_number(r, 2, done[i].cycle);
        cell_number(r, 3, done[i].average);
        cell_number(r, 4, done[i].stddev);
        point(&result->plot, WENA_CHART_BARS, title_of(done[i].card), done[i].cycle, 1);
    }
    free(done);
    return 1;
}

static int lead_cycle(const WenaViewData *data, WenaChartText text, WenaChartResult *result)
{
    size_t count, i;
    Done *done = completed(data, &count);
    const char *names[4];
    if (done == NULL) return 0;
    names[0] = T("card", "Card");
    names[1] = T("completed", "Completed");
    names[2] = T("board-view-lead-time", "Lead (days)");
    names[3] = T("board-view-cycle-time", "Cycle (days)");
    headers(&result->table, 4, names);
    for (i = 0; i < count; ++i) {
        WenaChartRow *r = row(&result->table);
        char when[32];
        if (r == NULL) break;
        date_cell(done[i].at, when, sizeof(when));
        cell_text(r, 0, title_of(done[i].card));
        cell_text(r, 1, when);
        cell_number(r, 2, done[i].lead);
        cell_number(r, 3, done[i].cycle);
        /* WeKan's bars are the cycle days for both charts: its points carry
         * cycleDays, which computeBarRows looks for first. */
        point(&result->plot, WENA_CHART_BARS, title_of(done[i].card), done[i].cycle, 1);
    }
    free(done);
    return 1;
}

static int burn(const WenaViewData *data, double now, int down, WenaChartText text, WenaChartResult *result)
{
    long day, from = first_day(data, now), to = wena_chart_day(now), days = to - from + 1, index;
    long initial = 0;
    size_t i;
    const char *names[3];
    names[0] = T("date", "Date");
    if (down) { names[1] = T("board-view-burndown", "Remaining"); names[2] = "Ideal"; }
    else { names[1] = "Total"; names[2] = T("completed", "Completed"); }
    headers(&result->table, 3, names);
    for (i = 0; i < data->card_count; ++i)
        if (data->cards[i].created_at.ms <= (double)from * DAY_MS) ++initial;
    for (day = from, index = 0; day <= to; ++day, ++index) {
        double end = ((double)day + 1.0) * DAY_MS - 1.0, at;
        long remaining = 0, total = 0, done = 0;
        WenaChartRow *r;
        char key[11];
        for (i = 0; i < data->card_count; ++i) {
            const WenaViewCard *card = &data->cards[i];
            int finished = wena_chart_completion(card, &at);
            if (card->created_at.ms <= end) {
                ++total;
                if (!finished || at > end) ++remaining;
            }
            if (finished && at <= end) ++done;
        }
        if ((r = row(&result->table)) == NULL) break;
        wena_chart_day_key(day, key);
        cell_text(r, 0, key);
        if (down) {
            double ideal = days > 1 ? (double)initial - (double)initial * (double)index / (double)(days - 1) : 0.0;
            if (ideal < 0.0) ideal = 0.0;
            cell_int(r, 1, remaining);
            cell_number(r, 2, round2(ideal));
            point(&result->plot, WENA_CHART_BARS, key, (double)remaining, 1);
        } else {
            cell_int(r, 1, total);
            cell_int(r, 2, done);
            point(&result->plot, WENA_CHART_BARS, key, (double)done, 1);
        }
    }
    return 1;
}

/* The Monday of a day's week, as computeThroughput's week buckets. */
static long week_of(long day)
{
    long weekday = ((day % 7) + 7 + 3) % 7;   /* 1970-01-01 was a Thursday: Monday = 0 */
    return day - weekday;
}

typedef struct Bucket {
    long day;
    long count;
} Bucket;

static int by_bucket(const void *a, const void *b)
{
    const Bucket *x = (const Bucket *)a, *y = (const Bucket *)b;
    return x->day < y->day ? -1 : x->day > y->day;
}

static int throughput(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    Bucket *buckets = (Bucket *)malloc((data->card_count + 1) * sizeof(*buckets));
    size_t i, j, count = 0, open = 0, recent;
    long completed_recent = 0;
    const char *names[2];
    double at;
    if (buckets == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        long week;
        if (!wena_chart_completion(&data->cards[i], &at)) { ++open; continue; }
        week = week_of(wena_chart_day(at));
        for (j = 0; j < count && buckets[j].day != week; ++j) {}
        if (j == count) { buckets[count].day = week; buckets[count].count = 0; ++count; }
        ++buckets[j].count;
    }
    qsort(buckets, count, sizeof(*buckets), by_bucket);
    names[0] = T("board-view-throughput-histogram", "Week");
    names[1] = T("cards", "Cards");
    headers(&result->table, 2, names);
    for (i = 0; i < count; ++i) {
        WenaChartRow *r = row(&result->table);
        char key[11];
        if (r == NULL) break;
        wena_chart_day_key(buckets[i].day, key);
        cell_text(r, 0, key);
        cell_int(r, 1, buckets[i].count);
        point(&result->plot, WENA_CHART_BARS, key, (double)buckets[i].count, 1);
    }
    /* computeCompletionForecast: the open cards at the last 4 weeks' rate. */
    recent = count < 4 ? count : 4;
    for (i = count - recent; i < count; ++i) completed_recent += buckets[i].count;
    if (open == 0) copy(result->note, sizeof(result->note), T("chart-forecast-none-remaining", "Nothing left open to project - every card is done."));
    else if (recent == 0 || completed_recent == 0) {
        char remaining[32];
        const char *format = T("chart-forecast-no-velocity", "__remaining__ card(s) still open; no recent completions to project a date from.");
        sprintf(remaining, "%lu", (unsigned long)open);
        wena_chart_format(result->note, sizeof(result->note), format, "remaining", remaining, NULL, NULL, NULL, NULL);
    } else {
        double average = (double)completed_recent / (double)recent;
        long needed = (long)ceil((double)open / average);
        char remaining[32], rate[32], date[11];
        const char *format = T("chart-forecast-projected", "At the recent pace of __average__ card(s)/week, the __remaining__ card(s) still open should be done by __date__.");
        sprintf(remaining, "%lu", (unsigned long)open);
        wena_chart_number(average, rate, sizeof(rate));
        wena_chart_day_key(wena_chart_day(now) + needed * 7, date);
        wena_chart_format(result->note, sizeof(result->note), format, "remaining", remaining, "average", rate,
                          "date", date);
    }
    free(buckets);
    return 1;
}

static int flow_efficiency(const WenaViewData *data, WenaChartText text, WenaChartResult *result)
{
    size_t i;
    const char *names[4];
    names[0] = T("card", "Card");
    names[1] = T("board-view-flow-efficiency", "Efficiency %");
    names[2] = "Active (h)";
    names[3] = "Total (h)";
    headers(&result->table, 4, names);
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        double at, start, total, efficiency;
        WenaChartRow *r;
        if (!wena_chart_completion(card, &at) || !(card->spent_time != 0.0)) continue;
        start = card->start_at.set ? card->start_at.ms : card->created_at.ms;
        total = (at - start) / 3600000.0;
        if (total < card->spent_time) total = card->spent_time;
        efficiency = total > 0.0 ? card->spent_time / total * 100.0 : 0.0;
        if (efficiency > 100.0) efficiency = 100.0;
        if ((r = row(&result->table)) == NULL) break;
        cell_text(r, 0, title_of(card));
        cell_number(r, 1, efficiency);
        cell_number(r, 2, card->spent_time);
        cell_number(r, 3, total);
        point(&result->plot, WENA_CHART_BARS, title_of(card), efficiency, 1);
    }
    return 1;
}

static int pulse(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    long to = wena_chart_day(now), from = to - 29, day;
    long counts[30];
    size_t i;
    const char *names[2];
    double start = (double)from * DAY_MS;
    memset(counts, 0, sizeof(counts));
    for (i = 0; i < data->activity_count; ++i) {
        const WenaViewActivity *event = &data->activities[i];
        long at;
        if (!event->at.set || event->at.ms < start || event->at.ms > now) continue;
        at = wena_chart_day(event->at.ms);
        if (at >= from && at <= to) ++counts[at - from];
    }
    names[0] = T("date", "Date");
    names[1] = T("board-view-pulse", "Pulse");
    headers(&result->table, 2, names);
    for (day = from; day <= to; ++day) {
        WenaChartRow *r = row(&result->table);
        char key[11];
        if (r == NULL) break;
        wena_chart_day_key(day, key);
        cell_text(r, 0, key);
        cell_int(r, 1, counts[day - from]);
        point(&result->plot, WENA_CHART_BARS, key, (double)counts[day - from], 1);
    }
    return 1;
}

/* Dashboard: cards counted by assignee, label and list ---------------------- */

typedef struct Group {
    char key[WENA_VIEW_ID];
    char label[WENA_VIEW_TITLE];
    long count;
} Group;

static int by_count(const void *a, const void *b)
{
    const Group *x = (const Group *)a, *y = (const Group *)b;
    return y->count < x->count ? -1 : y->count > x->count;
}

static void group_add(Group *groups, size_t *count, const char *key, const char *label)
{
    size_t i;
    for (i = 0; i < *count && strcmp(groups[i].key, key); ++i) {}
    if (i == *count) {
        copy(groups[i].key, sizeof(groups[i].key), key);
        copy(groups[i].label, sizeof(groups[i].label), label);
        groups[i].count = 0;
        ++*count;
    }
    ++groups[i].count;
}

static void dashboard_section(WenaChartTable *table, const char *name, Group *groups, size_t count, size_t cards)
{
    WenaChartRow *r;
    size_t i;
    if ((r = row(table)) != NULL) cell_text(r, 0, name);
    for (i = 0; i < count; ++i) {
        char value[64], percent[32];
        if ((r = row(table)) == NULL) return;
        cell_text(r, 0, groups[i].label);
        wena_chart_number(floor((double)groups[i].count / (double)(cards ? cards : 1) * 1000.0 + 0.5) / 10.0,
                          percent, sizeof(percent));
        sprintf(value, "%ld (%s%%)", groups[i].count, percent);
        cell_text(r, 1, value);
    }
    row(table);
}

static int dashboard(const WenaViewData *data, WenaChartText text, WenaChartResult *result)
{
    size_t capacity = data->card_count * (WENA_VIEW_PEOPLE + WENA_VIEW_LABELS) + 2, i, j;
    Group *assignees = (Group *)malloc(capacity * sizeof(Group));
    Group *labels = (Group *)malloc(capacity * sizeof(Group));
    Group *lists = (Group *)malloc(capacity * sizeof(Group));
    size_t assignee_count = 0, label_count = 0, list_count = 0;
    const char *names[2];
    const char *no_assignee = T("no-assignee", "No assignee"), *no_label = T("no-label", "No label");
    if (assignees == NULL || labels == NULL || lists == NULL) { free(assignees); free(labels); free(lists); return 0; }
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        const WenaViewList *list = wena_view_list(data, card->list_id);
        for (j = 0; j < card->assignee_count; ++j)
            group_add(assignees, &assignee_count, card->assignees[j], wena_view_user_name(data, card->assignees[j]));
        if (!card->assignee_count) group_add(assignees, &assignee_count, "__no_assignee__", no_assignee);
        for (j = 0; j < card->label_count; ++j) {
            const WenaViewLabel *label = wena_view_label(data, card->label_ids[j]);
            group_add(labels, &label_count, card->label_ids[j],
                      label == NULL ? card->label_ids[j] : label->name[0] ? label->name : label->color);
        }
        if (!card->label_count) group_add(labels, &label_count, "__no_label__", no_label);
        group_add(lists, &list_count, card->list_id, list != NULL ? list->title : card->list_id);
    }
    stable_sort(assignees, assignee_count, sizeof(Group), by_count);
    stable_sort(labels, label_count, sizeof(Group), by_count);
    stable_sort(lists, list_count, sizeof(Group), by_count);
    names[0] = T("name", "Name");
    names[1] = T("cards", "Cards");
    headers(&result->table, 2, names);
    dashboard_section(&result->table, T("assignees", "Assignees"), assignees, assignee_count, data->card_count);
    dashboard_section(&result->table, T("labels", "Labels"), labels, label_count, data->card_count);
    dashboard_section(&result->table, T("lists", "Lists"), lists, list_count, data->card_count);
    for (i = 0; i < assignee_count; ++i) point(&result->plot, WENA_CHART_BARS, assignees[i].label, (double)assignees[i].count, 1);
    free(assignees);
    free(labels);
    free(lists);
    return 1;
}

/* Flow analytics (flowAnalytics.js) ----------------------------------------- */

static int open_card(const WenaViewCard *card)
{
    return !card->archived && !card->deleted_at.set && !wena_chart_completion(card, NULL);
}

static double quantile(double *values, size_t count, double probability)
{
    size_t index, i, j;
    for (i = 1; i < count; ++i) {
        double value = values[i];
        for (j = i; j > 0 && values[j - 1] > value; --j) values[j] = values[j - 1];
        values[j] = value;
    }
    index = (size_t)ceil(probability * (double)count);
    return values[index > 0 ? index - 1 : 0];
}

/* The events of stage history: activities and position changes, by time. */
typedef struct StageEvent {
    const char *type;
    const char *card_id, *list_id, *old_list_id;
    double at;
} StageEvent;

static int by_event_time(const void *a, const void *b)
{
    const StageEvent *x = (const StageEvent *)a, *y = (const StageEvent *)b;
    return x->at < y->at ? -1 : x->at > y->at;
}

typedef struct AgingPoint {
    const WenaViewCard *card;
    const char *list;
    double age, threshold;
    int has_age, has_threshold, unusual;
    size_t samples;
} AgingPoint;

static int by_age(const void *a, const void *b)
{
    const AgingPoint *x = (const AgingPoint *)a, *y = (const AgingPoint *)b;
    double ax = x->has_age ? x->age : -1.0, ay = y->has_age ? y->age : -1.0;
    return ay < ax ? -1 : ay > ax;
}

static int aging_wip(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    size_t count = 0, i, j, n_points = 0;
    StageEvent *events = (StageEvent *)malloc((data->activity_count + data->change_count + 1) * sizeof(*events));
    Ids ids;
    const char **entry_list = NULL, **sample_list = NULL;
    double *entry_at = NULL, *samples = NULL;
    size_t sample_count = 0;
    AgingPoint *points = NULL;
    const char *names[6];
    memset(&ids, 0, sizeof(ids));
    if (events == NULL || !ids_build(&ids, data)) { free(events); free(ids.ids); return 0; }
    for (i = 0; i < data->activity_count; ++i) {
        const WenaViewActivity *a = &data->activities[i];
        if (!a->at.set || a->at.ms > now) continue;
        if (strcmp(a->type, "createCard") && strcmp(a->type, "moveCard") && strcmp(a->type, "moveCardBoard") &&
            strcmp(a->type, "archivedCard") && strcmp(a->type, "restoredCard")) continue;
        events[count].type = a->type; events[count].card_id = a->card_id;
        events[count].list_id = a->list_id; events[count].old_list_id = a->old_list_id;
        events[count].at = a->at.ms; ++count;
    }
    /* Universal position history also covers restores; its newer position
     * wins over an earlier activity. */
    for (i = 0; i < data->change_count; ++i) {
        const WenaViewChange *c = &data->changes[i];
        if (c->kind != WENA_VIEW_CHANGE_POSITION || !c->at.set || c->at.ms > now) continue;
        events[count].type = "moveCard"; events[count].card_id = c->card_id;
        events[count].list_id = c->new_list_id; events[count].old_list_id = c->old_list_id;
        events[count].at = c->at.ms; ++count;
    }
    stable_sort(events, count, sizeof(*events), by_event_time);
    entry_list = (const char **)calloc(ids.count + 1, sizeof(*entry_list));
    entry_at = (double *)calloc(ids.count + 1, sizeof(double));
    samples = (double *)malloc((count + 1) * sizeof(double));
    sample_list = (const char **)malloc((count + 1) * sizeof(*sample_list));
    points = (AgingPoint *)malloc((data->card_count + 1) * sizeof(*points));
    if (entry_list == NULL || entry_at == NULL || samples == NULL || sample_list == NULL || points == NULL) {
        free(events); free(ids.ids); free((void *)entry_list); free(entry_at); free(samples);
        free((void *)sample_list); free(points);
        return 0;
    }
    for (i = 0; i < count; ++i) {
        const StageEvent *e = &events[i];
        long card = ids_find(&ids, e->card_id);
        const char *previous;
        if (card < 0) continue;
        previous = entry_list[card];
        if (!strcmp(e->type, "archivedCard") || !strcmp(e->type, "moveCardBoard")) { entry_list[card] = NULL; continue; }
        /* Moving between swimlanes in the SAME list must not reset stage age. */
        if (!strcmp(e->type, "moveCard") && !strcmp(e->old_list_id, e->list_id)) continue;
        if (previous != NULL && !strcmp(previous, e->list_id)) continue;
        if (previous != NULL && !strcmp(e->type, "moveCard") && !strcmp(e->old_list_id, previous)) {
            samples[sample_count] = (e->at - entry_at[card]) / DAY_MS;
            sample_list[sample_count++] = previous;
        }
        entry_list[card] = e->list_id;
        entry_at[card] = e->at;
    }
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        const WenaViewList *list = wena_view_list(data, card->list_id);
        long id = ids_find(&ids, card->id);
        double *values;
        size_t value_count = 0;
        if (!open_card(card) || list == NULL) continue;
        points[n_points].card = card;
        points[n_points].list = list->title;
        points[n_points].has_age = id >= 0 && entry_list[id] != NULL && !strcmp(entry_list[id], card->list_id) &&
                                   entry_at[id] >= card->created_at.ms;
        points[n_points].age = points[n_points].has_age ? (now - entry_at[id]) / DAY_MS : 0.0;
        points[n_points].samples = 0;
        points[n_points].has_threshold = 0;
        points[n_points].threshold = 0.0;
        values = (double *)malloc((sample_count + 1) * sizeof(double));
        if (values != NULL) {
            for (j = 0; j < sample_count; ++j) if (!strcmp(sample_list[j], card->list_id)) values[value_count++] = samples[j];
            points[n_points].samples = value_count;
            points[n_points].has_threshold = value_count >= 5;
            if (value_count >= 5) points[n_points].threshold = quantile(values, value_count, 0.85);
            free(values);
        }
        points[n_points].unusual = points[n_points].has_age && points[n_points].has_threshold &&
                                   points[n_points].age > points[n_points].threshold;
        ++n_points;
    }
    stable_sort(points, n_points, sizeof(*points), by_age);
    names[0] = T("card", "Card");
    names[1] = T("list", "List");
    names[2] = T("flow-age-days", "Age in stage (days)");
    names[3] = T("flow-p85", "Stage 85th percentile (days)");
    names[4] = T("flow-samples", "Samples");
    names[5] = T("flow-signal", "Signal");
    headers(&result->table, 6, names);
    copy(result->plot.value_label, sizeof(result->plot.value_label), names[2]);
    result->plot.line_count = 1;
    copy(result->plot.line_labels[0], sizeof(result->plot.line_labels[0]), names[3]);
    for (i = 0; i < n_points; ++i) {
        WenaChartRow *r = row(&result->table);
        const char *unknown = T("flow-unknown", "Unknown");
        if (r == NULL) break;
        cell_text(r, 0, points[i].card->title);
        cell_text(r, 1, points[i].list);
        if (points[i].has_age) cell_number(r, 2, points[i].age); else cell_text(r, 2, unknown);
        if (points[i].has_threshold) cell_number(r, 3, points[i].threshold); else cell_text(r, 3, unknown);
        cell_int(r, 4, (long)points[i].samples);
        cell_text(r, 5, points[i].unusual ? T("flow-unusual", "Above limit") : "");
        if (points[i].has_age) {
            char label[200];
            size_t at;
            sprintf(label, "%.90s (%.90s)", points[i].card->title, points[i].list);
            at = point(&result->plot, WENA_CHART_POINTS, label, points[i].age, 1);
            result->plot.signal[at] = points[i].unusual;
            if (points[i].has_threshold) line(&result->plot, 0, at, points[i].threshold);
        }
    }
    free(events); free(ids.ids); free((void *)entry_list); free(entry_at); free(samples);
    free((void *)sample_list); free(points);
    return 1;
}

/* cyclePoints: completed by now, start before end, by completion then id. */
typedef struct Cycle {
    const WenaViewCard *card;
    double at, days, moving;
    int has_moving, signal;
} Cycle;

static int by_cycle(const void *a, const void *b)
{
    const Cycle *x = (const Cycle *)a, *y = (const Cycle *)b;
    if (x->at != y->at) return x->at < y->at ? -1 : 1;
    return strcmp(x->card->id, y->card->id);
}

static Cycle *cycle_points(const WenaViewData *data, double now, size_t *count)
{
    Cycle *points = (Cycle *)malloc((data->card_count + 1) * sizeof(*points));
    size_t i, n = 0;
    if (points == NULL) return NULL;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        double end, start;
        if (!wena_chart_completion(card, &end)) continue;
        if (!card->start_at.set && !card->created_at.set) continue;
        start = card->start_at.set ? card->start_at.ms : card->created_at.ms;
        if (end < start || end > now) continue;
        points[n].card = card;
        points[n].at = end;
        points[n].days = (end - start) / DAY_MS;
        points[n].has_moving = 0;
        points[n].signal = 0;
        ++n;
    }
    qsort(points, n, sizeof(*points), by_cycle);
    *count = n;
    return points;
}

static int process_behavior(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    size_t count, i;
    Cycle *points = cycle_points(data, now, &count);
    double average = 0.0, mr = 0.0, lower, upper, mr_upper;
    const char *names[10];
    if (points == NULL) return 0;
    names[0] = T("card", "Card");
    names[1] = T("completed", "Completed");
    names[2] = T("flow-cycle-days", "Cycle time (days)");
    names[3] = T("flow-mean", "Mean");
    names[4] = "LCL";
    names[5] = "UCL";
    names[6] = T("flow-moving-range", "Moving range");
    names[7] = T("flow-mr-mean", "Mean moving range");
    names[8] = "MR UCL";
    names[9] = T("flow-signal", "Signal");
    headers(&result->table, 10, names);
    if (count < 2) { free(points); return 1; }
    for (i = 0; i < count; ++i) average += points[i].days;
    average /= (double)count;
    for (i = 1; i < count; ++i) {
        points[i].moving = fabs(points[i].days - points[i - 1].days);
        points[i].has_moving = 1;
        mr += points[i].moving;
    }
    mr /= (double)(count - 1);
    /* Individuals limits: NIST e-Handbook 6.3.2.2, d2 = 1.128; moving range
     * (n=2): D3 = 0, D4 = 3.267. */
    lower = average - 3.0 * mr / 1.128;
    upper = average + 3.0 * mr / 1.128;
    mr_upper = 3.267 * mr;
    result->has_second = 1;
    result->plot.kind = result->second.kind = WENA_CHART_LINE;
    copy(result->plot.value_label, sizeof(result->plot.value_label), names[2]);
    copy(result->second.value_label, sizeof(result->second.value_label), names[6]);
    result->plot.line_count = result->second.line_count = 3;
    copy(result->plot.line_labels[0], 64, names[3]);
    copy(result->plot.line_labels[1], 64, "UCL");
    copy(result->plot.line_labels[2], 64, "LCL");
    copy(result->second.line_labels[0], 64, names[7]);
    copy(result->second.line_labels[1], 64, "UCL");
    copy(result->second.line_labels[2], 64, "LCL");
    for (i = 0; i < count; ++i) {
        WenaChartRow *r = row(&result->table);
        char when[32], label[160];
        size_t at;
        points[i].signal = points[i].days < lower || points[i].days > upper ||
                           (points[i].has_moving && points[i].moving > mr_upper);
        if (r == NULL) break;
        date_cell(points[i].at, when, sizeof(when));
        cell_text(r, 0, points[i].card->title);
        cell_text(r, 1, when);
        cell_number(r, 2, points[i].days);
        cell_number(r, 3, average);
        cell_number(r, 4, lower);
        cell_number(r, 5, upper);
        if (points[i].has_moving) cell_number(r, 6, points[i].moving); else cell_text(r, 6, T("flow-unknown", "Unknown"));
        cell_number(r, 7, mr);
        cell_number(r, 8, mr_upper);
        cell_text(r, 9, points[i].signal ? T("flow-unusual", "Above limit") : "");
        sprintf(label, "%.10s \xc2\xb7 %.120s", when, points[i].card->title);
        at = point(&result->plot, WENA_CHART_POINTS, label, points[i].days, 1);
        result->plot.signal[at] = points[i].signal;
        line(&result->plot, 0, at, average);
        line(&result->plot, 1, at, upper);
        line(&result->plot, 2, at, lower);
        at = point(&result->second, WENA_CHART_POINTS, label, points[i].moving, points[i].has_moving);
        line(&result->second, 0, at, mr);
        line(&result->second, 1, at, mr_upper);
        line(&result->second, 2, at, 0.0);
    }
    free(points);
    return 1;
}

static int size_cycle_time(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    size_t count, i;
    Cycle *points = cycle_points(data, now, &count);
    const char *names[5];
    if (points == NULL) return 0;
    names[0] = T("card", "Card");
    names[1] = T("flow-size-source", "Size field");
    names[2] = T("flow-size", "Estimate");
    names[3] = T("flow-cycle-days", "Cycle time (days)");
    names[4] = T("completed", "Completed");
    headers(&result->table, 5, names);
    result->plot.kind = WENA_CHART_SCATTER;
    copy(result->plot.value_label, sizeof(result->plot.value_label), names[3]);
    for (i = 0; i < count; ++i) {
        const WenaViewCard *card = points[i].card;
        WenaChartRow *r;
        char when[32];
        size_t at;
        /* The size: the card's Planning Poker estimation, WeKan's default. */
        if (!card->has_poker || card->poker < 0.0) continue;
        if ((r = row(&result->table)) == NULL) break;
        date_cell(points[i].at, when, sizeof(when));
        cell_text(r, 0, card->title);
        cell_text(r, 1, T("poker-question", "Planning Poker"));
        cell_number(r, 2, card->poker);
        cell_number(r, 3, points[i].days);
        cell_text(r, 4, when);
        at = point(&result->plot, WENA_CHART_POINTS, card->title, points[i].days, 1);
        result->plot.xs[at] = card->poker;
    }
    free(points);
    return 1;
}

/* Monte Carlo: a reproducible bootstrap of daily throughput. */
static unsigned long fnv_step(unsigned long seed, unsigned int c)
{
    return ((seed ^ c) * 16777619UL) & 0xFFFFFFFFUL;
}

static int monte_carlo(const WenaViewData *data, double now, const WenaChartOptions *options, WenaChartText text,
                       WenaChartResult *result)
{
    long today = wena_chart_day(now), from, first = 0, day, horizon;
    int seen = 0, target_count = options && options->target_count > 0 ? options->target_count : 10;
    int history_days = options && options->history_days > 0 ? options->history_days : 90;
    long target_day = options && options->target_date > 0.0 ? wena_chart_day(options->target_date) : today + 30;
    long *history = NULL, *finish = NULL, *capacity = NULL;
    size_t length = 0, i, trial;
    unsigned long seed = 2166136261UL;
    const int trials = 2000;
    const char *names[7];
    char target_key[11];
    static const double probabilities[4] = {0.5, 0.7, 0.85, 0.95};
    names[0] = T("flow-confidence", "Confidence");
    names[1] = T("flow-target-count", "Target cards");
    names[2] = T("flow-finish-days", "Days to finish");
    names[3] = T("flow-finish-date", "Finish date");
    names[4] = T("flow-target-date", "Target date");
    names[5] = T("flow-capacity", "At least this many cards");
    names[6] = T("flow-history-days", "History days");
    headers(&result->table, 7, names);
    {
        const char *detail[2];
        detail[0] = T("date", "Date");
        detail[1] = T("completed", "Completed");
        headers(&result->detail, 2, detail);
    }
    wena_chart_day_key(target_day, target_key);
    for (i = 0; i < data->card_count; ++i) {
        long created;
        if (!data->cards[i].created_at.set) continue;
        created = wena_chart_day(data->cards[i].created_at.ms);
        if (data->cards[i].created_at.ms > (double)today * DAY_MS) continue;
        if (!seen || created < first) first = created;
        seen = 1;
    }
    if (!seen) return 1;
    from = first > today - history_days ? first : today - history_days;
    if (today > from) length = (size_t)(today - from);
    history = (long *)calloc(length + 1, sizeof(long));
    if (history == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        double at;
        if (!wena_chart_completion(&data->cards[i], &at)) continue;
        if (at >= (double)from * DAY_MS && at < (double)today * DAY_MS && at >= data->cards[i].created_at.ms)
            ++history[wena_chart_day(at) - from];
    }
    for (day = 0; day < (long)length; ++day) {
        WenaChartRow *r = row(&result->detail);
        char key[11];
        if (r == NULL) break;
        wena_chart_day_key(from + day, key);
        cell_text(r, 0, key);
        cell_int(r, 1, history[day]);
    }
    for (i = 0; i < length && history[i] == 0; ++i) {}
    if (length == 0 || i == length) { free(history); return 1; }
    /* The seed: FNV-1a over JSON.stringify([history, targetCount, targetDate]). */
    {
        char piece[64];
        const char *c;
        seed = fnv_step(seed, '[');
        seed = fnv_step(seed, '[');
        for (i = 0; i < length; ++i) {
            char key[11];
            wena_chart_day_key(from + (long)i, key);
            sprintf(piece, "%s{\"day\":\"%s\",\"count\":%ld}", i ? "," : "", key, history[i]);
            for (c = piece; *c; ++c) seed = fnv_step(seed, (unsigned char)*c);
        }
        sprintf(piece, "],%d,\"%s\"]", target_count, target_key);
        for (c = piece; *c; ++c) seed = fnv_step(seed, (unsigned char)*c);
    }
    horizon = target_day - today;
    finish = (long *)malloc((size_t)trials * sizeof(long));
    capacity = (long *)malloc((size_t)trials * sizeof(long));
    if (finish == NULL || capacity == NULL) { free(history); free(finish); free(capacity); return 0; }
    for (trial = 0; trial < (size_t)trials; ++trial) {
        long total = 0, finished = -1, by_date = 0, elapsed;
        for (elapsed = 1; elapsed <= 3650; ++elapsed) {
            double random;
            seed = (seed * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
            random = (double)seed / 4294967296.0;
            total += history[(size_t)floor(random * (double)length)];
            if (elapsed == horizon) by_date = total;
            if (finished < 0 && total >= target_count) finished = elapsed;
            if (elapsed >= horizon && finished >= 0) break;
        }
        finish[trial] = finished;
        capacity[trial] = by_date;
    }
    result->has_second = 1;
    result->second.green = 1;
    copy(result->plot.value_label, 64, names[2]);
    copy(result->second.value_label, 64, names[5]);
    for (i = 0; i < 4; ++i) {
        double *sorted = (double *)malloc((size_t)trials * sizeof(double));
        double *caps = (double *)malloc((size_t)trials * sizeof(double));
        double days, count;
        int finite;
        WenaChartRow *r;
        char label[16], date[11];
        size_t t;
        if (sorted == NULL || caps == NULL) { free(sorted); free(caps); break; }
        for (t = 0; t < (size_t)trials; ++t) {
            sorted[t] = finish[t] < 0 ? 1e300 : (double)finish[t];
            caps[t] = (double)capacity[t];
        }
        days = quantile(sorted, (size_t)trials, probabilities[i]);
        quantile(caps, (size_t)trials, 0.0);
        /* A capacity commitment uses the LOWER tail. */
        count = caps[(size_t)floor((1.0 - probabilities[i]) * (double)trials)];
        finite = days < 1e299;
        free(sorted);
        free(caps);
        sprintf(label, "%g%%", probabilities[i] * 100.0);
        if ((r = row(&result->table)) == NULL) break;
        cell_text(r, 0, label);
        cell_int(r, 1, target_count);
        if (finite) cell_number(r, 2, days); else cell_text(r, 2, T("flow-unknown", "Unknown"));
        if (finite) { wena_chart_day_key(today + (long)days, date); cell_text(r, 3, date); }
        else cell_text(r, 3, T("flow-beyond-horizon", "Beyond simulation horizon"));
        cell_text(r, 4, target_key);
        cell_number(r, 5, count);
        cell_int(r, 6, (long)length);
        point(&result->plot, WENA_CHART_POINTS, label, finite ? days : 0.0, finite);
        point(&result->second, WENA_CHART_POINTS, label, count, 1);
    }
    free(history);
    free(finish);
    free(capacity);
    return 1;
}

/* Blocker analysis: the history of "blocks" dependencies ------------------- */

typedef struct BlockState {
    const WenaViewCard *card;
    char list_id[WENA_VIEW_ID];
    int archived, ended, archived_at, deleted;
    size_t dependency_count;
    WenaViewDependency dependencies[WENA_VIEW_DEPENDENCIES];
} BlockState;

typedef struct Edge {
    long blocker, card;
    char list_id[WENA_VIEW_ID];
} Edge;

typedef struct Episode {
    long blocker, card;
    char list_id[WENA_VIEW_ID];
    double start, end;
    int has_start, has_end;
} Episode;

static int state_open(const BlockState *state)
{
    return !state->archived && !state->deleted && !state->ended && !state->archived_at;
}

static size_t edges(const BlockState *states, size_t count, Edge *out, size_t capacity)
{
    size_t i, j, k, n = 0;
    for (i = 0; i < count; ++i) {
        const BlockState *source = &states[i];
        for (j = 0; j < source->dependency_count; ++j) {
            long target = -1, cause, card;
            for (k = 0; k < count; ++k)
                if (!strcmp(states[k].card->id, source->dependencies[j].card_id)) { target = (long)k; break; }
            if (target < 0 || target == (long)i) continue;
            cause = source->dependencies[j].blocks ? (long)i : target;
            card = source->dependencies[j].blocks ? target : (long)i;
            if (!state_open(&states[cause]) || !state_open(&states[card])) continue;
            for (k = 0; k < n; ++k)
                if (out[k].blocker == cause && out[k].card == card && !strcmp(out[k].list_id, states[card].list_id)) break;
            if (k < n || n == capacity) continue;
            out[n].blocker = cause;
            out[n].card = card;
            copy(out[n].list_id, sizeof(out[n].list_id), states[card].list_id);
            ++n;
        }
    }
    return n;
}

static void apply_change(const WenaViewData *data, BlockState *states, size_t count, const WenaViewChange *change,
                         int forward)
{
    size_t i;
    BlockState *state = NULL;
    for (i = 0; i < count; ++i) if (!strcmp(states[i].card->id, change->card_id)) { state = &states[i]; break; }
    if (state == NULL) return;
    if (change->kind == WENA_VIEW_CHANGE_POSITION) {
        const char *list = forward ? change->new_list_id : change->old_list_id;
        if (list[0]) copy(state->list_id, sizeof(state->list_id), list);
    } else if (change->kind == WENA_VIEW_CHANGE_LIFECYCLE) {
        state->deleted = forward ? change->removed : !change->removed;
    } else {
        int has = forward ? change->has_new : change->has_old;
        double value = forward ? change->new_value : change->old_value;
        if (!strcmp(change->field, "archived")) state->archived = has && value != 0.0;
        else if (!strcmp(change->field, "deletedAt")) state->deleted = has;
        else if (!strcmp(change->field, "endAt")) state->ended = has;
        else if (!strcmp(change->field, "archivedAt")) state->archived_at = has;
        else if (!strcmp(change->field, "cardDependencies")) {
            size_t start = forward ? change->new_dependency_start : change->old_dependency_start;
            size_t n = forward ? change->new_dependency_count : change->old_dependency_count;
            if (n > WENA_VIEW_DEPENDENCIES) n = WENA_VIEW_DEPENDENCIES;
            if (start + n <= data->change_dependency_count) {
                memcpy(state->dependencies, data->change_dependencies + start, n * sizeof(WenaViewDependency));
                state->dependency_count = n;
            }
        }
    }
}

static int blocker_analysis(const WenaViewData *data, double now, WenaChartText text, WenaChartResult *result)
{
    size_t count = data->card_count, i, j, active_count, next_count, episode_count = 0, episode_capacity;
    BlockState *states = (BlockState *)calloc(count + 1, sizeof(*states));
    Edge *active = (Edge *)malloc((count * WENA_VIEW_DEPENDENCIES + 1) * sizeof(Edge));
    Edge *next = (Edge *)malloc((count * WENA_VIEW_DEPENDENCIES + 1) * sizeof(Edge));
    double *active_start;
    int *active_known;
    Episode *episodes;
    const char *names[7];
    if (states == NULL || active == NULL || next == NULL) { free(states); free(active); free(next); return 0; }
    for (i = 0; i < count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        states[i].card = card;
        copy(states[i].list_id, sizeof(states[i].list_id), card->list_id);
        states[i].archived = card->archived;
        states[i].ended = card->end_at.set;
        states[i].archived_at = card->archived_at.set;
        states[i].deleted = card->deleted_at.set;
        states[i].dependency_count = card->dependency_count;
        memcpy(states[i].dependencies, card->dependencies, sizeof(states[i].dependencies));
    }
    /* Rewind to before the history, then replay it forward. */
    for (i = data->change_count; i-- > 0;)
        if (data->changes[i].at.set && data->changes[i].at.ms <= now) apply_change(data, states, count, &data->changes[i], 0);
    active_count = edges(states, count, active, count * WENA_VIEW_DEPENDENCIES);
    episode_capacity = active_count + data->change_count * 4 + 16;
    episodes = (Episode *)malloc(episode_capacity * sizeof(*episodes));
    active_start = (double *)calloc(count * WENA_VIEW_DEPENDENCIES + 1, sizeof(double));
    active_known = (int *)calloc(count * WENA_VIEW_DEPENDENCIES + 1, sizeof(int));
    if (episodes == NULL || active_start == NULL || active_known == NULL) {
        free(states); free(active); free(next); free(episodes); free(active_start); free(active_known);
        return 0;
    }
    for (i = 0; i < data->change_count; ++i) {
        const WenaViewChange *change = &data->changes[i];
        if (!change->at.set || change->at.ms > now) continue;
        apply_change(data, states, count, change, 1);
        next_count = edges(states, count, next, count * WENA_VIEW_DEPENDENCIES);
        for (j = 0; j < active_count;) {
            size_t k;
            for (k = 0; k < next_count; ++k)
                if (next[k].blocker == active[j].blocker && next[k].card == active[j].card &&
                    !strcmp(next[k].list_id, active[j].list_id)) break;
            if (k == next_count) {
                if (episode_count < episode_capacity) {
                    Episode *e = &episodes[episode_count++];
                    e->blocker = active[j].blocker; e->card = active[j].card;
                    copy(e->list_id, sizeof(e->list_id), active[j].list_id);
                    e->has_start = active_known[j]; e->start = active_start[j];
                    e->has_end = 1; e->end = change->at.ms;
                }
                active[j] = active[active_count - 1];
                active_start[j] = active_start[active_count - 1];
                active_known[j] = active_known[active_count - 1];
                --active_count;
            } else ++j;
        }
        for (j = 0; j < next_count; ++j) {
            size_t k;
            for (k = 0; k < active_count; ++k)
                if (next[j].blocker == active[k].blocker && next[j].card == active[k].card &&
                    !strcmp(next[j].list_id, active[k].list_id)) break;
            if (k == active_count) {
                active[active_count] = next[j];
                active_start[active_count] = change->at.ms;
                active_known[active_count] = 1;
                ++active_count;
            }
        }
    }
    for (j = 0; j < active_count && episode_count < episode_capacity; ++j) {
        Episode *e = &episodes[episode_count++];
        e->blocker = active[j].blocker; e->card = active[j].card;
        copy(e->list_id, sizeof(e->list_id), active[j].list_id);
        e->has_start = active_known[j]; e->start = active_start[j];
        e->has_end = 0; e->end = 0.0;
    }
    names[0] = T("flow-blocker", "Blocker");
    names[1] = T("flow-episodes", "Episodes");
    names[2] = T("flow-active", "Open episodes");
    names[3] = T("flow-blocked-days", "Blocked card-days");
    names[4] = T("flow-unknown-start", "Unknown starts");
    names[5] = T("cards", "Cards");
    names[6] = T("lists", "Lists");
    headers(&result->table, 7, names);
    {
        const char *detail[6];
        detail[0] = names[0];
        detail[1] = T("card", "Card");
        detail[2] = T("list", "List");
        detail[3] = T("card-start", "Start");
        detail[4] = T("card-end", "End");
        detail[5] = T("days", "Days");
        headers(&result->detail, 6, detail);
    }
    copy(result->plot.value_label, 64, names[3]);
    /* historicalBlockerGroups: by blocker, most blocked days first. */
    {
        size_t groups = 0;
        long *blocker = (long *)malloc((episode_count + 1) * sizeof(long));
        double *days = (double *)calloc(episode_count + 1, sizeof(double));
        if (blocker != NULL && days != NULL) {
            for (i = 0; i < episode_count; ++i) {
                for (j = 0; j < groups && blocker[j] != episodes[i].blocker; ++j) {}
                if (j == groups) blocker[groups++] = episodes[i].blocker;
                if (episodes[i].has_start)
                    days[j] += ((episodes[i].has_end ? episodes[i].end : now) - episodes[i].start) / DAY_MS;
            }
            /* Most blocked days first, then most episodes: insertion keeps
             * the first-seen order of a tie, as a stable sort does. */
            {
                long *episodes_of = (long *)calloc(groups + 1, sizeof(long));
                if (episodes_of != NULL) {
                    for (i = 0; i < episode_count; ++i)
                        for (j = 0; j < groups; ++j) if (blocker[j] == episodes[i].blocker) { ++episodes_of[j]; break; }
                    for (i = 1; i < groups; ++i) {
                        long b = blocker[i], e = episodes_of[i];
                        double d = days[i];
                        for (j = i; j > 0 && (days[j - 1] < d || (days[j - 1] == d && episodes_of[j - 1] < e)); --j) {
                            blocker[j] = blocker[j - 1]; days[j] = days[j - 1]; episodes_of[j] = episodes_of[j - 1];
                        }
                        blocker[j] = b; days[j] = d; episodes_of[j] = e;
                    }
                    free(episodes_of);
                }
            }
            for (i = 0; i < groups; ++i) {
                WenaChartRow *r = row(&result->table);
                char cards[WENA_CHART_CELL], lists[WENA_CHART_CELL];
                long episodes_n = 0, open = 0, unknown = 0;
                size_t k;
                if (r == NULL) break;
                cards[0] = lists[0] = '\0';
                for (k = 0; k < episode_count; ++k) {
                    const WenaViewList *list;
                    const char *list_title, *card_title;
                    if (episodes[k].blocker != blocker[i]) continue;
                    ++episodes_n;
                    open += !episodes[k].has_end;
                    unknown += !episodes[k].has_start;
                    card_title = states[episodes[k].card].card->title;
                    list = wena_view_list(data, episodes[k].list_id);
                    list_title = list != NULL ? list->title : episodes[k].list_id;
                    if (!strstr(cards, card_title) && strlen(cards) + strlen(card_title) + 3 < sizeof(cards))
                        { if (cards[0]) strcat(cards, ", "); strcat(cards, card_title); }
                    if (!strstr(lists, list_title) && strlen(lists) + strlen(list_title) + 3 < sizeof(lists))
                        { if (lists[0]) strcat(lists, ", "); strcat(lists, list_title); }
                }
                cell_text(r, 0, states[blocker[i]].card->title);
                cell_int(r, 1, episodes_n);
                cell_int(r, 2, open);
                cell_number(r, 3, days[i]);
                cell_int(r, 4, unknown);
                cell_text(r, 5, cards);
                cell_text(r, 6, lists);
                point(&result->plot, WENA_CHART_POINTS, states[blocker[i]].card->title, days[i], 1);
            }
        }
        free(blocker);
        free(days);
    }
    for (i = 0; i < episode_count; ++i) {
        WenaChartRow *r = row(&result->detail);
        const WenaViewList *list = wena_view_list(data, episodes[i].list_id);
        char when[32];
        if (r == NULL) break;
        cell_text(r, 0, states[episodes[i].blocker].card->title);
        cell_text(r, 1, states[episodes[i].card].card->title);
        cell_text(r, 2, list != NULL ? list->title : episodes[i].list_id);
        if (episodes[i].has_start) { date_cell(episodes[i].start, when, sizeof(when)); cell_text(r, 3, when); }
        else cell_text(r, 3, T("flow-unknown", "Unknown"));
        if (episodes[i].has_end) { date_cell(episodes[i].end, when, sizeof(when)); cell_text(r, 4, when); }
        else cell_text(r, 4, T("flow-active", "Open episodes"));
        if (episodes[i].has_start)
            cell_number(r, 5, ((episodes[i].has_end ? episodes[i].end : now) - episodes[i].start) / DAY_MS);
        else cell_text(r, 5, T("flow-unknown", "Unknown"));
    }
    free(states); free(active); free(next); free(episodes); free(active_start); free(active_known);
    return 1;
}

/* The entry ------------------------------------------------------------------ */

int wena_chart_compute(const char *chart, const WenaViewData *data, double now,
                       const WenaChartOptions *options, WenaChartText text, WenaChartResult *result)
{
    if (chart == NULL || data == NULL || result == NULL) return 0;
    memset(result, 0, sizeof(*result));
    if (!strcmp(chart, "cumulativeFlow")) return cumulative_flow(data, now, text, result);
    if (!strcmp(chart, "wipRun")) return wip_run(data, now, text, result);
    if (!strcmp(chart, "controlChart")) return control_chart(data, text, result);
    if (!strcmp(chart, "leadTime") || !strcmp(chart, "cycleTime")) return lead_cycle(data, text, result);
    if (!strcmp(chart, "burndown")) return burn(data, now, 1, text, result);
    if (!strcmp(chart, "burnup")) return burn(data, now, 0, text, result);
    if (!strcmp(chart, "throughputHistogram")) return throughput(data, now, text, result);
    if (!strcmp(chart, "flowEfficiency")) return flow_efficiency(data, text, result);
    if (!strcmp(chart, "pulse")) return pulse(data, now, text, result);
    if (!strcmp(chart, "dashboard")) return dashboard(data, text, result);
    if (!strcmp(chart, "agingWip")) return aging_wip(data, now, text, result);
    if (!strcmp(chart, "blockerAnalysis")) return blocker_analysis(data, now, text, result);
    if (!strcmp(chart, "monteCarlo")) return monte_carlo(data, now, options, text, result);
    if (!strcmp(chart, "processBehavior")) return process_behavior(data, now, text, result);
    if (!strcmp(chart, "sizeCycleTime")) return size_cycle_time(data, now, text, result);
    return 0;
}

void wena_chart_result_free(WenaChartResult *result)
{
    if (result == NULL) return;
    free(result->table.rows);
    free(result->detail.rows);
    memset(result, 0, sizeof(*result));
}
