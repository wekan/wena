/* tests/test_charts.py's Wena side: reads a board's records as lines on
 * stdin, computes one chart (argv[1]) at the time argv[2] (ms) and prints
 * its table - headers, then rows, tab separated - its note and its plot, the
 * same way the test prints WeKan's. */
#include "../models/charts.h"
#include "../models/view_rows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIELDS 40

static char *fields[FIELDS];

static size_t split(char *line)
{
    size_t count = 0;
    char *at = line;
    fields[count++] = at;
    while (*at && count < FIELDS) {
        if (*at == '|') { *at = '\0'; fields[count++] = at + 1; }
        ++at;
    }
    return count;
}

static void copy(char *out, size_t capacity, const char *text)
{
    strncpy(out, text, capacity - 1);
    out[capacity - 1] = '\0';
}

static WenaViewTime when(const char *text)
{
    WenaViewTime time;
    time.set = text[0] != '\0';
    time.ms = time.set ? strtod(text, NULL) : 0.0;
    return time;
}

static size_t ids(char out[][WENA_VIEW_ID], size_t capacity, char *text)
{
    size_t count = 0;
    char *token = strtok(text, ",");
    while (token != NULL && count < capacity) { copy(out[count++], WENA_VIEW_ID, token); token = strtok(NULL, ","); }
    return count;
}

static size_t dependencies(WenaViewDependency *out, size_t capacity, char *text)
{
    size_t count = 0;
    char *token = strtok(text, ";");
    while (token != NULL && count < capacity) {
        char *colon = strchr(token, ':');
        if (colon != NULL) {
            *colon = '\0';
            copy(out[count].card_id, WENA_VIEW_ID, token);
            out[count].blocks = colon[1] == 'b';
            ++count;
        }
        token = strtok(NULL, ";");
    }
    return count;
}

static void print_plot(const char *name, const WenaChartPlot *plot)
{
    size_t i;
    char value[64];
    for (i = 0; i < plot->count; ++i) {
        wena_chart_number(plot->values[i], value, sizeof(value));
        printf("%s\t%s\t%s\n", name, plot->labels[i], plot->has_value[i] ? value : "-");
    }
}

static void print_table(const char *name, const WenaChartTable *table)
{
    size_t i, c;
    printf("%s", name);
    for (c = 0; c < table->columns; ++c) printf("\t%s", table->headers[c]);
    printf("\n");
    for (i = 0; i < table->row_count; ++i) {
        printf("%s", name);
        for (c = 0; c < table->columns; ++c) printf("\t%s", table->rows[i][c]);
        printf("\n");
    }
}

static void time_text(const WenaViewTime *t)
{
    if (t->set) printf("%.0f", t->ms); else printf("-");
}

/* The other views' rows, as test_view_rows.py compares them. */
static int views(WenaViewData *data, double now)
{
    static const WenaTableField fields_[] = {WENA_TABLE_TITLE, WENA_TABLE_LIST, WENA_TABLE_SWIMLANE, WENA_TABLE_ASSIGNEES,
                                             WENA_TABLE_LABELS, WENA_TABLE_START, WENA_TABLE_DUE, WENA_TABLE_END};
    static const char *const queries[] = {"", "card 1", "  REVIEW ", "nothing"};
    size_t f, d, g, q, i, count;
    WenaViewTableRow *rows = (WenaViewTableRow *)calloc(data->card_count + 1, sizeof(*rows));
    WenaCalendarEvent *events = (WenaCalendarEvent *)calloc(data->card_count * 4 + 1, sizeof(*events));
    WenaTimelineCard *timeline = (WenaTimelineCard *)calloc(data->card_count + 1, sizeof(*timeline));
    WenaGanttTask *tasks = (WenaGanttTask *)calloc(data->card_count + 1, sizeof(*tasks));
    const WenaViewCard **cards = (const WenaViewCard **)calloc(data->card_count + 1, sizeof(*cards));
    const WenaViewSprint **sprints = (const WenaViewSprint **)calloc(data->sprint_count + 1, sizeof(*sprints));
    WenaTimeSummary summary;
    WenaAssigneeGroup *groups = NULL;
    long group_count;
    double markers[50];
    if (!rows || !events || !timeline || !tasks || !cards || !sprints) return 1;
    for (f = 0; f < sizeof(fields_) / sizeof(fields_[0]); ++f)
        for (d = 0; d < 2; ++d)
            for (g = 0; g < 2; ++g)
                for (q = 0; q < 4; ++q) {
                    count = wena_view_table_rows(data, queries[q], fields_[f], (int)d, (int)g, rows, data->card_count);
                    printf("table\t%d\t%lu\t%lu\t%s", (int)fields_[f], (unsigned long)d, (unsigned long)g, queries[q]);
                    for (i = 0; i < count; ++i) printf("\t%s", rows[i].card->id);
                    printf("\n");
                }
    count = wena_view_calendar_events(data, now - 40.0 * 86400000.0, now - 10.0 * 86400000.0, events, data->card_count * 4);
    for (i = 0; i < count; ++i) printf("calendar\t%s\t%d\t%.0f\t%.0f\n", events[i].card->id, (int)events[i].kind, events[i].start, events[i].end);
    if (!wena_view_time(data, now, "No assignee", &summary)) return 1;
    printf("time\t%s\t%ld\t%ld\t%ld\t%ld\n", "x", summary.remaining_days, summary.remaining_hour, summary.remaining_cards,
           summary.cards_with_time);
    for (i = 0; i < summary.assignee_count; ++i) { char h[32]; wena_chart_number(summary.by_assignee[i].hours, h, sizeof(h));
        printf("time-assignee\t%s\t%s\t%ld\n", summary.by_assignee[i].label, h, summary.by_assignee[i].cards); }
    for (i = 0; i < summary.card_count; ++i) printf("time-card\t%s\n", summary.by_card[i]->id);
    wena_view_time_free(&summary);
    group_count = wena_view_assignee_groups(data, "No assignee", &groups);
    for (i = 0; i < (size_t)group_count; ++i) {
        size_t k;
        printf("group\t%s", groups[i].label);
        for (k = 0; k < groups[i].card_count; ++k) printf("\t%s", groups[i].cards[k]->id);
        printf("\n");
    }
    wena_view_groups_free(groups, group_count);
    count = wena_view_timeline_markers(data, markers, 50);
    for (i = 0; i < count; ++i) printf("marker\t%.0f\n", markers[i]);
    for (q = 0; q < 3; ++q) {
        double at = q == 0 ? 0.0 : now - (double)q * 20.0 * 86400000.0;
        count = wena_view_timeline(data, q == 0, at, timeline);
        for (i = 0; i < count; ++i) {
            size_t k;
            printf("timeline\t%lu\t%s\t%d\t%s\t%s\t%d\t", (unsigned long)q, timeline[i].card->id, timeline[i].existed,
                   timeline[i].title, timeline[i].list_id, timeline[i].archived);
            time_text(&timeline[i].due_at);
            printf("\t");
            for (k = 0; k < timeline[i].label_count; ++k) printf("%s%s", k ? "," : "", timeline[i].label_ids[k]);
            printf("\t");
            for (k = 0; k < timeline[i].member_count; ++k) printf("%s%s", k ? "," : "", timeline[i].members[k]);
            printf("\n");
        }
    }
    for (i = 0; i < data->card_count; ++i) cards[i] = &data->cards[i];
    count = wena_view_gantt_tasks(cards, data->card_count, now, tasks);
    for (i = 0; i < count; ++i) printf("gantt\t%s\t%.0f\t%.0f\t%d\t%d\n", tasks[i].card->id, tasks[i].start, tasks[i].end,
                                       tasks[i].done, tasks[i].overdue);
    for (q = 0; q < 2; ++q) {
        count = wena_view_scrum_cards(data, q ? "sp1" : "", cards);
        printf("scrum\t%lu", (unsigned long)q);
        for (i = 0; i < count; ++i) printf("\t%s", cards[i]->id);
        printf("\n");
    }
    for (i = 0; i < data->card_count; ++i) {
        double estimate;
        if (wena_view_card_estimate(data, &data->cards[i], &estimate)) { char e[32]; wena_chart_number(estimate, e, sizeof(e));
            printf("estimate\t%s\t%s\n", data->cards[i].id, e); }
        else printf("estimate\t%s\t-\n", data->cards[i].id);
    }
    count = wena_view_velocity(data, sprints);
    printf("velocity");
    for (i = 0; i < count; ++i) printf("\t%s", sprints[i]->id);
    printf("\n");
    return 0;
}

int main(int argc, char **argv)
{
    static char line[8192];
    WenaViewData data;
    WenaChartResult result;
    WenaChartOptions options;
    if (argc != 3) return 2;
    memset(&data, 0, sizeof(data));
    memset(&options, 0, sizeof(options));
    data.cards = (WenaViewCard *)calloc(2000, sizeof(WenaViewCard));
    data.lists = (WenaViewList *)calloc(100, sizeof(WenaViewList));
    data.users = (WenaViewUser *)calloc(100, sizeof(WenaViewUser));
    data.labels = (WenaViewLabel *)calloc(100, sizeof(WenaViewLabel));
    data.activities = (WenaViewActivity *)calloc(20000, sizeof(WenaViewActivity));
    data.changes = (WenaViewChange *)calloc(20000, sizeof(WenaViewChange));
    data.change_dependencies = (WenaViewDependency *)calloc(20000, sizeof(WenaViewDependency));
    data.swimlanes = (WenaViewSwimlane *)calloc(100, sizeof(WenaViewSwimlane));
    if (!data.cards || !data.lists || !data.users || !data.labels || !data.activities || !data.changes ||
        !data.change_dependencies) return 1;
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t n;
        line[strcspn(line, "\n")] = '\0';
        n = split(line);
        if (!strcmp(fields[0], "L") && n >= 6) {
            WenaViewList *l = &data.lists[data.list_count++];
            copy(l->id, sizeof(l->id), fields[1]);
            copy(l->title, sizeof(l->title), fields[2]);
            l->sort = atof(fields[3]);
            l->wip_enabled = atoi(fields[4]);
            l->wip_value = atoi(fields[5]);
        } else if (!strcmp(fields[0], "S") && n >= 4) {
            WenaViewSwimlane *s = &data.swimlanes[data.swimlane_count++];
            copy(s->id, sizeof(s->id), fields[1]);
            copy(s->title, sizeof(s->title), fields[2]);
            s->sort = atof(fields[3]);
        } else if (!strcmp(fields[0], "E") && n >= 2) {
            /* The board's Scrum estimate field: "E|fieldId" */
            data.estimate_from_field = 1;
            copy(data.estimate_field_id, sizeof(data.estimate_field_id), fields[1]);
        } else if (!strcmp(fields[0], "P") && n >= 5) {
            /* A sprint: id, name, state, closed at, has report */
            WenaViewSprint *s;
            if (data.sprints == NULL) data.sprints = (WenaViewSprint *)calloc(50, sizeof(WenaViewSprint));
            s = &data.sprints[data.sprint_count++];
            copy(s->id, sizeof(s->id), fields[1]);
            copy(s->name, sizeof(s->name), fields[2]);
            copy(s->state, sizeof(s->state), fields[3]);
            s->closed_at = when(fields[4]);
            s->has_report = n > 5 && atoi(fields[5]);
        } else if (!strcmp(fields[0], "U") && n >= 3) {
            WenaViewUser *u = &data.users[data.user_count++];
            copy(u->id, sizeof(u->id), fields[1]);
            copy(u->name, sizeof(u->name), fields[2]);
        } else if (!strcmp(fields[0], "G") && n >= 4) {
            WenaViewLabel *g = &data.labels[data.label_count++];
            copy(g->id, sizeof(g->id), fields[1]);
            copy(g->name, sizeof(g->name), fields[2]);
            copy(g->color, sizeof(g->color), fields[3]);
        } else if (!strcmp(fields[0], "C") && n >= 21) {
            WenaViewCard *c = &data.cards[data.card_count++];
            copy(c->id, sizeof(c->id), fields[1]);
            copy(c->title, sizeof(c->title), fields[2]);
            copy(c->list_id, sizeof(c->list_id), fields[3]);
            copy(c->swimlane_id, sizeof(c->swimlane_id), fields[4]);
            c->created_at = when(fields[5]);
            c->archived = atoi(fields[6]);
            c->archived_at = when(fields[7]);
            c->start_at = when(fields[8]);
            c->end_at = when(fields[9]);
            c->due_at = when(fields[10]);
            c->has_spent_time = fields[11][0] != '\0';
            c->spent_time = atof(fields[11]);
            c->is_overtime = atoi(fields[12]);
            c->votes_positive = atoi(fields[15]);
            c->votes_negative = atoi(fields[16]);
            c->has_poker = fields[17][0] != '\0';
            c->poker = atof(fields[17]);
            c->deleted_at = when(fields[19]);
            copy(c->board_id, sizeof(c->board_id), fields[20]);
            c->assignee_count = ids(c->assignees, WENA_VIEW_PEOPLE, fields[13]);
            c->label_count = ids(c->label_ids, WENA_VIEW_LABELS, fields[14]);
            c->dependency_count = dependencies(c->dependencies, WENA_VIEW_DEPENDENCIES, fields[18]);
            /* 21: received, members, sprint, rank, release, sort, estimate field value, description */
            if (n >= 29) {
                c->received_at = when(fields[21]);
                c->member_count = ids(c->members, WENA_VIEW_PEOPLE, fields[22]);
                copy(c->sprint_id, sizeof(c->sprint_id), fields[23]);
                c->backlog_rank = when(fields[24]);
                copy(c->release_id, sizeof(c->release_id), fields[25]);
                c->sort = atof(fields[26]);
                if (fields[27][0]) {
                    c->field_count = 1;
                    copy(c->fields[0].field_id, WENA_VIEW_ID, "est");
                    copy(c->fields[0].value, WENA_VIEW_TITLE, fields[27] + 1);
                    c->fields[0].is_number = fields[27][0] == 'n';
                    c->fields[0].number = atof(fields[27] + 1);
                }
                copy(c->description, sizeof(c->description), fields[28]);
            }
        } else if (!strcmp(fields[0], "A") && n >= 7) {
            WenaViewActivity *a = &data.activities[data.activity_count++];
            copy(a->type, sizeof(a->type), fields[1]);
            copy(a->card_id, sizeof(a->card_id), fields[2]);
            copy(a->list_id, sizeof(a->list_id), fields[3]);
            copy(a->old_list_id, sizeof(a->old_list_id), fields[4]);
            copy(a->user_id, sizeof(a->user_id), fields[5]);
            a->at = when(fields[6]);
            /* 7: old swimlane, member, label, old value, time key, time old, has old list, has old swimlane */
            if (n >= 15) {
                copy(a->old_swimlane_id, sizeof(a->old_swimlane_id), fields[7]);
                copy(a->member_id, sizeof(a->member_id), fields[8]);
                copy(a->label_id, sizeof(a->label_id), fields[9]);
                copy(a->old_value, sizeof(a->old_value), fields[10]);
                copy(a->time_key, sizeof(a->time_key), fields[11]);
                a->time_old = when(fields[12]);
                a->has_old_list = atoi(fields[13]);
                a->has_old_swimlane = atoi(fields[14]);
            }
        } else if (!strcmp(fields[0], "H") && n >= 15) {
            WenaViewChange *h = &data.changes[data.change_count++];
            h->kind = !strcmp(fields[1], "position") ? WENA_VIEW_CHANGE_POSITION :
                      !strcmp(fields[1], "lifecycle") ? WENA_VIEW_CHANGE_LIFECYCLE : WENA_VIEW_CHANGE_FIELD;
            copy(h->card_id, sizeof(h->card_id), fields[2]);
            copy(h->user_id, sizeof(h->user_id), fields[3]);
            copy(h->field, sizeof(h->field), fields[4]);
            copy(h->old_list_id, sizeof(h->old_list_id), fields[5]);
            copy(h->new_list_id, sizeof(h->new_list_id), fields[6]);
            h->has_old = fields[7][0] != '\0';
            h->old_value = atof(fields[7]);
            h->has_new = fields[8][0] != '\0';
            h->new_value = atof(fields[8]);
            h->removed = atoi(fields[9]);
            h->old_dependency_start = data.change_dependency_count;
            h->old_dependency_count = dependencies(data.change_dependencies + data.change_dependency_count, 8, fields[10]);
            data.change_dependency_count += h->old_dependency_count;
            h->new_dependency_start = data.change_dependency_count;
            h->new_dependency_count = dependencies(data.change_dependencies + data.change_dependency_count, 8, fields[11]);
            data.change_dependency_count += h->new_dependency_count;
            h->at = when(fields[12]);
        }
    }
    if (!strncmp(argv[1], "views", 5)) return views(&data, strtod(argv[2], NULL));
    if (!wena_chart_compute(argv[1], &data, strtod(argv[2], NULL), &options, NULL, &result)) return 1;
    print_table("table", &result.table);
    print_table("detail", &result.detail);
    if (result.note[0]) printf("note\t%s\n", result.note);
    print_plot("plot", &result.plot);
    if (result.has_second) print_plot("second", &result.second);
    wena_chart_result_free(&result);
    wena_view_data_free(&data);
    return 0;
}
