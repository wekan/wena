#include "view_rows.h"
#include "charts.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DAY_MS 86400000.0

/* Array.prototype.sort is stable; a merge sort is too. */
static void merge_sort(void *items, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    char *temp, *base = (char *)items;
    size_t width, from;
    if (count < 2 || (temp = (char *)malloc(count * size)) == NULL) return;
    for (width = 1; width < count; width *= 2)
        for (from = 0; from + width < count; from += 2 * width) {
            size_t middle = from + width, to = from + 2 * width < count ? from + 2 * width : count;
            size_t i = from, j = middle, k = from;
            while (i < middle && j < to) {
                if (compare(base + j * size, base + i * size) < 0) memcpy(temp + k++ * size, base + j++ * size, size);
                else memcpy(temp + k++ * size, base + i++ * size, size);
            }
            while (i < middle) memcpy(temp + k++ * size, base + i++ * size, size);
            while (j < to) memcpy(temp + k++ * size, base + j++ * size, size);
            memcpy(base + from * size, temp + from * size, (to - from) * size);
        }
    free(temp);
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* localeCompare(b, undefined, {numeric: true, sensitivity: 'base'}) as far
 * as ASCII goes: case does not count, a run of digits compares as a number. */
static int natural(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            const char *x = a, *y = b;
            size_t lx, ly;
            while (*x == '0') ++x;
            while (*y == '0') ++y;
            for (lx = 0; x[lx] >= '0' && x[lx] <= '9'; ++lx) {}
            for (ly = 0; y[ly] >= '0' && y[ly] <= '9'; ++ly) {}
            if (lx != ly) return lx < ly ? -1 : 1;
            {
                int order = strncmp(x, y, lx);
                if (order) return order < 0 ? -1 : 1;
            }
            a = x + lx;
            b = y + ly;
            continue;
        }
        if (lower((unsigned char)*a) != lower((unsigned char)*b))
            return lower((unsigned char)*a) < lower((unsigned char)*b) ? -1 : 1;
        ++a;
        ++b;
    }
    return *a ? 1 : *b ? -1 : 0;
}

/* Table -------------------------------------------------------------------- */

typedef struct TableSort {
    WenaViewTableRow row;
    char assignees[WENA_VIEW_PEOPLE * WENA_VIEW_ID];
    char members[WENA_VIEW_PEOPLE * WENA_VIEW_ID];
    char labels[512];
} TableSort;

static WenaTableField sort_field;
static int sort_descending, sort_group;

static const char *table_text(const TableSort *r, WenaTableField field)
{
    switch (field) {
    case WENA_TABLE_TITLE: return r->row.card->title;
    case WENA_TABLE_LIST: return r->row.list_title;
    case WENA_TABLE_SWIMLANE: return r->row.swimlane_title;
    case WENA_TABLE_ASSIGNEES: return r->assignees;
    case WENA_TABLE_MEMBERS: return r->members;
    case WENA_TABLE_LABELS: return r->labels;
    default: return "";
    }
}

static const WenaViewTime *table_time(const TableSort *r, WenaTableField field)
{
    switch (field) {
    case WENA_TABLE_RECEIVED: return &r->row.card->received_at;
    case WENA_TABLE_START: return &r->row.card->start_at;
    case WENA_TABLE_DUE: return &r->row.card->due_at;
    case WENA_TABLE_END: return &r->row.card->end_at;
    default: return NULL;
    }
}

/* compareTableViewRows: empty values last whatever the direction, the
 * direction only between two values, then by title. */
static int table_compare(const void *pa, const void *pb)
{
    const TableSort *a = (const TableSort *)pa, *b = (const TableSort *)pb;
    int result = 0, a_empty, b_empty;
    if (sort_group) {
        if (a->row.swimlane_sort != b->row.swimlane_sort) return a->row.swimlane_sort < b->row.swimlane_sort ? -1 : 1;
        if ((result = natural(a->row.swimlane_title, b->row.swimlane_title)) != 0) return result;
        if ((result = natural(a->row.swimlane_id, b->row.swimlane_id)) != 0) return result;
    }
    if (table_time(a, sort_field) != NULL) {
        const WenaViewTime *x = table_time(a, sort_field), *y = table_time(b, sort_field);
        a_empty = !x->set;
        b_empty = !y->set;
        if (!a_empty && !b_empty) result = x->ms < y->ms ? -1 : x->ms > y->ms;
    } else {
        const char *x = table_text(a, sort_field), *y = table_text(b, sort_field);
        a_empty = !x[0];
        b_empty = !y[0];
        if (!a_empty && !b_empty) result = natural(x, y);
    }
    if (a_empty) result = b_empty ? 0 : 1;
    else if (b_empty) result = -1;
    else if (result != 0 && sort_descending) result = -result;
    if (result == 0 && sort_field != WENA_TABLE_TITLE) result = natural(a->row.card->title, b->row.card->title);
    return result;
}

static int by_title_bytes(const void *pa, const void *pb)
{
    return strcmp(((const TableSort *)pa)->row.card->title, ((const TableSort *)pb)->row.card->title);
}

static int contains(const char *haystack, const char *needle)
{
    size_t i, j, n = strlen(needle);
    if (n == 0) return 1;
    for (i = 0; haystack[i]; ++i) {
        for (j = 0; j < n && haystack[i + j] && lower((unsigned char)haystack[i + j]) == lower((unsigned char)needle[j]); ++j) {}
        if (j == n) return 1;
    }
    return 0;
}

size_t wena_view_table_rows(const WenaViewData *data, const char *query, WenaTableField field, int descending,
                            int group, WenaViewTableRow *rows, size_t capacity)
{
    TableSort *sorted;
    size_t i, j, count = 0;
    char needle[256];
    size_t start = 0, end;
    if (data == NULL) return 0;
    /* WeKan trims and lower-cases the query. */
    if (query == NULL) query = "";
    while (query[start] == ' ' || query[start] == '\t') ++start;
    end = strlen(query);
    while (end > start && (query[end - 1] == ' ' || query[end - 1] == '\t')) --end;
    if (end - start >= sizeof(needle)) end = start + sizeof(needle) - 1;
    memcpy(needle, query + start, end - start);
    needle[end - start] = '\0';
    sorted = (TableSort *)malloc((data->card_count + 1) * sizeof(*sorted));
    if (sorted == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        const WenaViewList *list = wena_view_list(data, card->list_id);
        const WenaViewSwimlane *lane = NULL;
        TableSort *r;
        char haystack[1024];
        if (card->archived || card->deleted_at.set || list == NULL) continue;
        for (j = 0; j < data->swimlane_count; ++j)
            if (!strcmp(data->swimlanes[j].id, card->swimlane_id)) { lane = &data->swimlanes[j]; break; }
        if (lane == NULL) continue;
        r = &sorted[count];
        memset(r, 0, sizeof(*r));
        r->row.card = card;
        r->row.list_title = list->title;
        r->row.swimlane_title = lane->title;
        r->row.swimlane_sort = lane->sort;
        r->row.swimlane_id = lane->id;
        for (j = 0; j < card->assignee_count; ++j) {
            if (j) strcat(r->assignees, " ");
            strcat(r->assignees, card->assignees[j]);
        }
        for (j = 0; j < card->member_count; ++j) {
            if (j) strcat(r->members, " ");
            strcat(r->members, card->members[j]);
        }
        for (j = 0; j < card->label_count; ++j) {
            const WenaViewLabel *label = wena_view_label(data, card->label_ids[j]);
            if (label == NULL || strlen(r->labels) + strlen(label->name) + 2 >= sizeof(r->labels)) continue;
            if (r->labels[0] || j) strcat(r->labels, " ");
            strcat(r->labels, label->name);
        }
        if (needle[0]) {
            haystack[0] = '\0';
            if (strlen(card->title) + strlen(list->title) + strlen(lane->title) + strlen(r->labels) + 4 < sizeof(haystack)) {
                strcat(haystack, card->title); strcat(haystack, " ");
                strcat(haystack, list->title); strcat(haystack, " ");
                strcat(haystack, lane->title); strcat(haystack, " ");
                strcat(haystack, r->labels);
            }
            if (!contains(haystack, needle)) continue;
        }
        ++count;
    }
    /* WeKan reads the cards by title, then sorts them stably. */
    merge_sort(sorted, count, sizeof(*sorted), by_title_bytes);
    sort_field = field;
    sort_descending = descending;
    sort_group = group;
    merge_sort(sorted, count, sizeof(*sorted), table_compare);
    for (i = 0; rows != NULL && i < count && i < capacity; ++i) rows[i] = sorted[i].row;
    free(sorted);
    return count;
}

/* Calendar ----------------------------------------------------------------- */

static int by_event_card(const void *pa, const void *pb)
{
    const WenaCalendarEvent *a = (const WenaCalendarEvent *)pa, *b = (const WenaCalendarEvent *)pb;
    /* events.sort((a, b) => a.id > b.id ? 1 : -1) */
    return strcmp(a->card->id, b->card->id) > 0 ? 1 : -1;
}

size_t wena_view_calendar_events(const WenaViewData *data, double from, double to, WenaCalendarEvent *events,
                                 size_t capacity)
{
    size_t i, count = 0;
    if (data == NULL || events == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *c = &data->cards[i];
        if (c->deleted_at.set) continue;
        /* cardsInIntervalSelector */
        if (c->start_at.set && c->end_at.set &&
            ((c->start_at.ms <= from && c->end_at.ms >= from) || (c->start_at.ms <= to && c->end_at.ms >= to) ||
             (c->start_at.ms >= from && c->end_at.ms <= to)) && count < capacity) {
            events[count].card = c; events[count].kind = WENA_CALENDAR_INTERVAL;
            events[count].start = c->start_at.ms; events[count].end = c->end_at.ms; ++count;
        }
        if (c->received_at.set && c->received_at.ms >= from && c->received_at.ms <= to && count < capacity) {
            events[count].card = c; events[count].kind = WENA_CALENDAR_RECEIVED;
            events[count].start = c->received_at.ms; events[count].end = c->received_at.ms + 3600000.0; ++count;
        }
        if (c->due_at.set && c->due_at.ms >= from && c->due_at.ms <= to && count < capacity) {
            events[count].card = c; events[count].kind = WENA_CALENDAR_DUE;
            events[count].start = c->due_at.ms; events[count].end = c->due_at.ms + 3600000.0; ++count;
        }
        if (c->end_at.set && c->end_at.ms >= from && c->end_at.ms <= to && count < capacity) {
            events[count].card = c; events[count].kind = WENA_CALENDAR_END;
            events[count].start = c->end_at.ms; events[count].end = c->end_at.ms + 3600000.0; ++count;
        }
    }
    merge_sort(events, count, sizeof(*events), by_event_card);
    return count;
}

/* Time ------------------------------------------------------------------- */

static int by_hours(const void *pa, const void *pb)
{
    const WenaTimeGroup *a = (const WenaTimeGroup *)pa, *b = (const WenaTimeGroup *)pb;
    return b->hours < a->hours ? -1 : b->hours > a->hours;
}

static int by_card_hours(const void *pa, const void *pb)
{
    const WenaViewCard *a = *(const WenaViewCard *const *)pa, *b = *(const WenaViewCard *const *)pb;
    return b->spent_time < a->spent_time ? -1 : b->spent_time > a->spent_time;
}

static int by_entry_time(const void *pa, const void *pb)
{
    const WenaTimeEntry *a = (const WenaTimeEntry *)pa, *b = (const WenaTimeEntry *)pb;
    return b->at < a->at ? -1 : b->at > a->at;
}

static WenaTimeGroup *time_group(WenaTimeGroup *groups, size_t *count, const char *key, const char *label)
{
    size_t i;
    for (i = 0; i < *count; ++i) if (!strcmp(groups[i].key, key)) return &groups[i];
    memset(&groups[*count], 0, sizeof(groups[*count]));
    strncpy(groups[*count].key, key, sizeof(groups[*count].key) - 1);
    strncpy(groups[*count].label, label, sizeof(groups[*count].label) - 1);
    return &groups[(*count)++];
}

int wena_view_time(const WenaViewData *data, double now, const char *no_assignee, WenaTimeSummary *summary)
{
    size_t i, j;
    double total_ms = 0.0, total_hours, abs_hours;
    if (data == NULL || summary == NULL) return 0;
    memset(summary, 0, sizeof(*summary));
    summary->by_assignee = (WenaTimeGroup *)calloc(data->card_count * WENA_VIEW_PEOPLE + 2, sizeof(WenaTimeGroup));
    summary->by_card = (const WenaViewCard **)calloc(data->card_count + 1, sizeof(*summary->by_card));
    summary->adjustments = (WenaTimeGroup *)calloc(data->change_count + 1, sizeof(WenaTimeGroup));
    summary->entries = (WenaTimeEntry *)calloc(data->change_count + 1, sizeof(WenaTimeEntry));
    if (!summary->by_assignee || !summary->by_card || !summary->adjustments || !summary->entries) {
        wena_view_time_free(summary);
        return 0;
    }
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        if (card->archived || card->deleted_at.set) continue;
        /* computeRemainingTimeSum: open, not completed, with a due date. */
        if (!wena_chart_completion(card, NULL) && card->due_at.set) {
            total_ms += card->due_at.ms - now;
            ++summary->remaining_cards;
        }
        if (!(card->spent_time > 0.0)) continue;
        summary->spent_total += card->spent_time;
        ++summary->cards_with_time;
        if (card->is_overtime) ++summary->overtime_cards;
        summary->by_card[summary->card_count++] = card;
        if (card->assignee_count == 0) {
            WenaTimeGroup *g = time_group(summary->by_assignee, &summary->assignee_count, "__no_assignee__", no_assignee);
            g->hours += card->spent_time;
            ++g->cards;
        }
        for (j = 0; j < card->assignee_count; ++j) {
            WenaTimeGroup *g = time_group(summary->by_assignee, &summary->assignee_count, card->assignees[j],
                                          wena_view_user_name(data, card->assignees[j]));
            g->hours += card->spent_time;
            ++g->cards;
        }
    }
    merge_sort(summary->by_assignee, summary->assignee_count, sizeof(WenaTimeGroup), by_hours);
    for (i = 0; i < summary->assignee_count; ++i)
        summary->by_assignee[i].hours = floor(summary->by_assignee[i].hours * 100.0 + 0.5) / 100.0;
    merge_sort(summary->by_card, summary->card_count, sizeof(*summary->by_card), by_card_hours);
    total_hours = total_ms / 3600000.0;
    summary->remaining_hours = floor(total_hours * 100.0 + 0.5) / 100.0;
    abs_hours = floor(fabs(total_hours) + 0.5);
    summary->remaining_days = (long)floor(abs_hours / 24.0);
    summary->remaining_hour = (long)(abs_hours - (double)summary->remaining_days * 24.0);
    if (summary->remaining_hour == 24) { ++summary->remaining_days; summary->remaining_hour = 0; }
    if (total_hours < 0.0) { summary->remaining_days = -summary->remaining_days; summary->remaining_hour = -summary->remaining_hour; }
    /* timeAdjustments: every spentTime change, by its author. */
    for (i = 0; i < data->change_count; ++i) {
        const WenaViewChange *c = &data->changes[i];
        double before, after, hours;
        WenaTimeGroup *g;
        if (c->kind != WENA_VIEW_CHANGE_FIELD || strcmp(c->field, "spentTime")) continue;
        before = c->has_old ? c->old_value : 0.0;
        after = c->has_new ? c->new_value : 0.0;
        hours = after - before;
        if (hours == 0.0) continue;
        summary->entries[summary->entry_count].card_id = c->card_id;
        summary->entries[summary->entry_count].user_id = c->user_id;
        summary->entries[summary->entry_count].at = c->at.ms;
        summary->entries[summary->entry_count].hours = hours;
        summary->entries[summary->entry_count].total = after;
        ++summary->entry_count;
        g = time_group(summary->adjustments, &summary->adjustment_count, c->user_id, wena_view_user_name(data, c->user_id));
        g->hours += hours;
        ++g->cards;
    }
    merge_sort(summary->entries, summary->entry_count, sizeof(WenaTimeEntry), by_entry_time);
    merge_sort(summary->adjustments, summary->adjustment_count, sizeof(WenaTimeGroup), by_hours);
    return 1;
}

void wena_view_time_free(WenaTimeSummary *summary)
{
    if (summary == NULL) return;
    free(summary->by_assignee);
    free((void *)summary->by_card);
    free(summary->adjustments);
    free(summary->entries);
    memset(summary, 0, sizeof(*summary));
}

/* Group by Assignee and Roadmap ------------------------------------------- */

static const char *empty_key;

static int by_group_size(const void *pa, const void *pb)
{
    const WenaAssigneeGroup *a = (const WenaAssigneeGroup *)pa, *b = (const WenaAssigneeGroup *)pb;
    if (!strcmp(a->key, empty_key)) return 1;
    if (!strcmp(b->key, empty_key)) return -1;
    return b->card_count < a->card_count ? -1 : b->card_count > a->card_count;
}

static int by_group_label(const void *pa, const void *pb)
{
    const WenaAssigneeGroup *a = (const WenaAssigneeGroup *)pa, *b = (const WenaAssigneeGroup *)pb;
    if (!strcmp(a->key, empty_key)) return 1;
    if (!strcmp(b->key, empty_key)) return -1;
    return natural(a->label, b->label);
}

static WenaAssigneeGroup *group_of(WenaAssigneeGroup *groups, long *count, const char *key, const char *label,
                                   size_t cards)
{
    long i;
    for (i = 0; i < *count; ++i) if (!strcmp(groups[i].key, key)) return &groups[i];
    memset(&groups[*count], 0, sizeof(groups[*count]));
    strncpy(groups[*count].key, key, sizeof(groups[*count].key) - 1);
    strncpy(groups[*count].label, label, sizeof(groups[*count].label) - 1);
    groups[*count].cards = (const WenaViewCard **)malloc((cards + 1) * sizeof(*groups[*count].cards));
    if (groups[*count].cards == NULL) return NULL;
    return &groups[(*count)++];
}

long wena_view_assignee_groups(const WenaViewData *data, const char *no_assignee, WenaAssigneeGroup **out)
{
    WenaAssigneeGroup *groups;
    long count = 0;
    size_t i, j;
    if (data == NULL || out == NULL) return -1;
    groups = (WenaAssigneeGroup *)calloc(data->card_count * WENA_VIEW_PEOPLE + 2, sizeof(*groups));
    if (groups == NULL) return -1;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        WenaAssigneeGroup *g;
        if (card->archived || card->deleted_at.set) continue;
        if (card->assignee_count == 0) {
            if ((g = group_of(groups, &count, "__no_assignee__", no_assignee, data->card_count)) == NULL) break;
            g->cards[g->card_count++] = card;
        }
        for (j = 0; j < card->assignee_count; ++j) {
            if ((g = group_of(groups, &count, card->assignees[j], wena_view_user_name(data, card->assignees[j]),
                              data->card_count)) == NULL) break;
            g->cards[g->card_count++] = card;
        }
    }
    empty_key = "__no_assignee__";
    merge_sort(groups, (size_t)count, sizeof(*groups), by_group_size);
    *out = groups;
    return count;
}

void wena_view_groups_free(WenaAssigneeGroup *groups, long count)
{
    long i;
    for (i = 0; groups != NULL && i < count; ++i) free((void *)groups[i].cards);
    free(groups);
}

static int by_start_due(const void *pa, const void *pb)
{
    const WenaViewCard *a = *(const WenaViewCard *const *)pa, *b = *(const WenaViewCard *const *)pb;
    /* Mongo's { startAt: 1, dueAt: 1 }: a missing date first. */
    if (a->start_at.set != b->start_at.set) return a->start_at.set ? 1 : -1;
    if (a->start_at.set && a->start_at.ms != b->start_at.ms) return a->start_at.ms < b->start_at.ms ? -1 : 1;
    if (a->due_at.set != b->due_at.set) return a->due_at.set ? 1 : -1;
    if (a->due_at.set && a->due_at.ms != b->due_at.ms) return a->due_at.ms < b->due_at.ms ? -1 : 1;
    return 0;
}

long wena_view_roadmap_groups(const WenaViewData *data, const char *field_id, const char *no_value,
                              WenaAssigneeGroup **out)
{
    WenaAssigneeGroup *groups;
    const WenaViewCard **ordered;
    long count = 0;
    size_t i;
    if (data == NULL || field_id == NULL || out == NULL) return -1;
    groups = (WenaAssigneeGroup *)calloc(data->card_count + 2, sizeof(*groups));
    ordered = (const WenaViewCard **)malloc((data->card_count + 1) * sizeof(*ordered));
    if (groups == NULL || ordered == NULL) { free(groups); free((void *)ordered); return -1; }
    for (i = 0; i < data->card_count; ++i) ordered[i] = &data->cards[i];
    merge_sort((void *)ordered, data->card_count, sizeof(*ordered), by_start_due);
    for (i = 0; i < data->card_count; ++i) {
        const char *value = wena_view_field_value(data, ordered[i], field_id);
        WenaAssigneeGroup *group;
        if (ordered[i]->deleted_at.set) continue;
        group = group_of(groups, &count, value[0] ? value : "__no_roadmap_value__", value[0] ? value : no_value,
                         data->card_count);
        if (group == NULL) break;
        group->cards[group->card_count++] = ordered[i];
    }
    free((void *)ordered);
    empty_key = "__no_roadmap_value__";
    merge_sort(groups, (size_t)count, sizeof(*groups), by_group_label);
    *out = groups;
    return count;
}

/* Timeline ------------------------------------------------------------------ */

static int by_double(const void *pa, const void *pb)
{
    double a = *(const double *)pa, b = *(const double *)pb;
    return a < b ? -1 : a > b;
}

size_t wena_view_timeline_markers(const WenaViewData *data, double *out, size_t capacity)
{
    double *times;
    size_t i, count = 0, unique = 0, result;
    if (data == NULL || out == NULL) return 0;
    times = (double *)malloc((data->activity_count + 1) * sizeof(double));
    if (times == NULL) return 0;
    for (i = 0; i < data->activity_count; ++i) if (data->activities[i].at.set && data->activities[i].at.ms != 0.0)
        times[count++] = data->activities[i].at.ms;
    qsort(times, count, sizeof(double), by_double);
    for (i = 0; i < count; ++i) if (unique == 0 || times[unique - 1] != times[i]) times[unique++] = times[i];
    if (unique > 50) {
        double step = (double)unique / 50.0;
        for (i = 0; i < 50 && i < capacity; ++i) out[i] = times[(size_t)floor((double)i * step)];
        result = i;
    } else {
        for (i = 0; i < unique && i < capacity; ++i) out[i] = times[i];
        result = i;
    }
    free(times);
    return result;
}

static void remove_id(char ids[][WENA_VIEW_ID], size_t *count, const char *id)
{
    size_t i;
    for (i = 0; i < *count; ++i)
        if (!strcmp(ids[i], id)) {
            memmove(ids[i], ids[i + 1], (*count - i - 1) * WENA_VIEW_ID);
            --*count;
            return;
        }
}

static void add_id(char ids[][WENA_VIEW_ID], size_t *count, size_t capacity, const char *id)
{
    size_t i;
    if (id == NULL || !id[0]) return;
    for (i = 0; i < *count; ++i) if (!strcmp(ids[i], id)) return;
    if (*count < capacity) { strncpy(ids[*count], id, WENA_VIEW_ID - 1); ids[*count][WENA_VIEW_ID - 1] = '\0'; ++*count; }
}

typedef struct Future {
    const WenaViewActivity *activity;
} Future;

static int by_newest(const void *pa, const void *pb)
{
    const Future *a = (const Future *)pa, *b = (const Future *)pb;
    return b->activity->at.ms < a->activity->at.ms ? -1 : b->activity->at.ms > a->activity->at.ms;
}

size_t wena_view_timeline(const WenaViewData *data, int live, double at, WenaTimelineCard *out)
{
    size_t i, j, count = 0;
    Future *future;
    if (data == NULL || out == NULL) return 0;
    future = (Future *)malloc((data->activity_count + 1) * sizeof(*future));
    if (future == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *card = &data->cards[i];
        WenaTimelineCard *state = &out[count];
        size_t n = 0;
        if (card->archived || card->deleted_at.set) continue;
        memset(state, 0, sizeof(*state));
        state->card = card;
        state->existed = 1;
        strcpy(state->title, card->title);
        strcpy(state->description, card->description);
        strcpy(state->list_id, card->list_id);
        strcpy(state->swimlane_id, card->swimlane_id);
        state->due_at = card->due_at;
        state->archived = card->archived;
        state->label_count = card->label_count;
        memcpy(state->label_ids, card->label_ids, sizeof(state->label_ids));
        state->member_count = card->member_count;
        memcpy(state->members, card->members, sizeof(state->members));
        ++count;
        if (live) continue;
        /* Undo, newest first, what happened to it after `at`. */
        for (j = 0; j < data->activity_count; ++j)
            if (!strcmp(data->activities[j].card_id, card->id) && data->activities[j].at.ms > at)
                future[n++].activity = &data->activities[j];
        merge_sort(future, n, sizeof(*future), by_newest);
        for (j = 0; j < n; ++j) {
            const WenaViewActivity *a = future[j].activity;
            if (!strcmp(a->type, "createCard")) state->existed = 0;
            else if (!strcmp(a->type, "a-changedTitle")) { strncpy(state->title, a->old_value, sizeof(state->title) - 1); state->title[sizeof(state->title) - 1] = '\0'; }
            else if (!strcmp(a->type, "a-changedDescription")) strcpy(state->description, a->old_value);
            else if (!strcmp(a->type, "a-dueAt")) { if (!strcmp(a->time_key, "dueAt")) state->due_at = a->time_old; }
            else if (!strcmp(a->type, "moveCard")) {
                if (a->has_old_list) strcpy(state->list_id, a->old_list_id);
                if (a->has_old_swimlane) strcpy(state->swimlane_id, a->old_swimlane_id);
            }
            else if (!strcmp(a->type, "archivedCard")) state->archived = 0;
            else if (!strcmp(a->type, "restoredCard")) state->archived = 1;
            else if (!strcmp(a->type, "joinMember")) remove_id(state->members, &state->member_count, a->member_id);
            else if (!strcmp(a->type, "unjoinMember")) add_id(state->members, &state->member_count, WENA_VIEW_PEOPLE, a->member_id);
            else if (!strcmp(a->type, "addedLabel")) remove_id(state->label_ids, &state->label_count, a->label_id);
            else if (!strcmp(a->type, "removedLabel")) add_id(state->label_ids, &state->label_count, WENA_VIEW_LABELS, a->label_id);
        }
    }
    free(future);
    return count;
}

/* Gantt ------------------------------------------------------------------- */

static double day_start(double ms)
{
    return floor(ms / DAY_MS) * DAY_MS;
}

size_t wena_view_gantt_tasks(const WenaViewCard *const *cards, size_t count, double now, WenaGanttTask *tasks)
{
    size_t i, n = 0;
    for (i = 0; i < count; ++i) {
        const WenaViewCard *c = cards[i];
        double start, end;
        if (!c->start_at.set && !c->received_at.set) continue;
        start = day_start(c->start_at.set ? c->start_at.ms : c->received_at.ms);
        end = c->due_at.set ? day_start(c->due_at.ms) : c->end_at.set ? day_start(c->end_at.ms) : 0.0;
        if (end <= start) end = start + DAY_MS;
        tasks[n].card = c;
        tasks[n].start = start;
        tasks[n].end = end;
        tasks[n].done = c->end_at.set;
        tasks[n].overdue = c->due_at.set && !c->end_at.set && day_start(c->due_at.ms) < day_start(now);
        tasks[n].from_received = !c->start_at.set;
        tasks[n].to_end = !c->due_at.set;
        ++n;
    }
    return n;
}

void wena_view_iso_week(long day, long *year, int *week)
{
    long thursday = day - (((day % 7) + 7 + 3) % 7) + 3, jan1;
    char key[11];
    wena_chart_day_key(thursday, key);
    *year = atol(key);
    /* The day number of January 1st of the Thursday's year. */
    jan1 = thursday;
    {
        char k[11];
        while (1) { wena_chart_day_key(jan1 - 1, k); if (atol(k) != *year) break; jan1 -= 1; }
    }
    *week = (int)((thursday - jan1) / 7 + 1);
}

static int by_long(const void *pa, const void *pb)
{
    long a = *(const long *)pa, b = *(const long *)pb;
    return a < b ? -1 : a > b;
}

size_t wena_view_gantt_weeks(const WenaViewData *data, long *weeks, size_t capacity)
{
    size_t i, k, count = 0;
    if (data == NULL || weeks == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewTime *times[4];
        times[0] = &data->cards[i].received_at;
        times[1] = &data->cards[i].start_at;
        times[2] = &data->cards[i].due_at;
        times[3] = &data->cards[i].end_at;
        if (data->cards[i].deleted_at.set) continue;
        for (k = 0; k < 4; ++k) {
            long monday, j;
            if (!times[k]->set) continue;
            monday = wena_chart_day(times[k]->ms);
            monday -= ((monday % 7) + 7 + 3) % 7;
            for (j = 0; j < (long)count && weeks[j] != monday; ++j) {}
            if (j == (long)count && count < capacity) weeks[count++] = monday;
        }
    }
    qsort(weeks, count, sizeof(long), by_long);
    return count;
}

/* Scrum ------------------------------------------------------------------- */

int wena_view_card_estimate(const WenaViewData *data, const WenaViewCard *card, double *estimate)
{
    size_t i;
    if (data == NULL || card == NULL) return 0;
    if (data->estimate_from_field) {
        for (i = 0; i < card->field_count; ++i)
            if (!strcmp(card->fields[i].field_id, data->estimate_field_id)) {
                const char *v = card->fields[i].value;
                size_t k;
                int dot = 0;
                if (card->fields[i].is_number) {
                    if (card->fields[i].number < 0.0) return 0;
                    *estimate = card->fields[i].number;
                    return 1;
                }
                /* ^\d+(?:\.\d+)?$ */
                if (!(v[0] >= '0' && v[0] <= '9')) return 0;
                for (k = 0; v[k]; ++k) {
                    if (v[k] == '.' && !dot && v[k + 1] >= '0' && v[k + 1] <= '9') { dot = 1; continue; }
                    if (!(v[k] >= '0' && v[k] <= '9')) return 0;
                }
                *estimate = strtod(v, NULL);
                return 1;
            }
        return 0;
    }
    if (!card->has_poker || card->poker < 0.0) return 0;
    *estimate = card->poker;
    return 1;
}

static int by_rank(const void *pa, const void *pb)
{
    const WenaViewCard *a = *(const WenaViewCard *const *)pa, *b = *(const WenaViewCard *const *)pb;
    double ra = a->backlog_rank.set ? a->backlog_rank.ms : a->sort, rb = b->backlog_rank.set ? b->backlog_rank.ms : b->sort;
    if (ra != rb) return ra < rb ? -1 : 1;
    return strcmp(a->id, b->id);
}

size_t wena_view_scrum_cards(const WenaViewData *data, const char *sprint_id, const WenaViewCard **cards)
{
    size_t i, count = 0;
    if (data == NULL || cards == NULL) return 0;
    for (i = 0; i < data->card_count; ++i) {
        const WenaViewCard *c = &data->cards[i];
        if (c->archived || c->deleted_at.set) continue;
        if (sprint_id != NULL && sprint_id[0] ? strcmp(c->sprint_id, sprint_id) != 0 : c->sprint_id[0] != '\0') continue;
        cards[count++] = c;
    }
    qsort((void *)cards, count, sizeof(*cards), by_rank);
    return count;
}

static int by_closed(const void *pa, const void *pb)
{
    const WenaViewSprint *a = *(const WenaViewSprint *const *)pa, *b = *(const WenaViewSprint *const *)pb;
    return a->closed_at.ms < b->closed_at.ms ? -1 : a->closed_at.ms > b->closed_at.ms;
}

size_t wena_view_velocity(const WenaViewData *data, const WenaViewSprint **sprints)
{
    size_t i, count = 0;
    if (data == NULL || sprints == NULL) return 0;
    for (i = 0; i < data->sprint_count; ++i)
        if (!strcmp(data->sprints[i].state, "closed") && data->sprints[i].closed_at.set && data->sprints[i].has_report)
            sprints[count++] = &data->sprints[i];
    merge_sort((void *)sprints, count, sizeof(*sprints), by_closed);
    return count;
}
