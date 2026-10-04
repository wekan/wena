/* tests/test_charts.py's Wena side: reads a board's records as lines on
 * stdin, computes one chart (argv[1]) at the time argv[2] (ms) and prints
 * its table - headers, then rows, tab separated - its note and its plot, the
 * same way the test prints WeKan's. */
#include "../models/charts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIELDS 24

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
        } else if (!strcmp(fields[0], "A") && n >= 7) {
            WenaViewActivity *a = &data.activities[data.activity_count++];
            copy(a->type, sizeof(a->type), fields[1]);
            copy(a->card_id, sizeof(a->card_id), fields[2]);
            copy(a->list_id, sizeof(a->list_id), fields[3]);
            copy(a->old_list_id, sizeof(a->old_list_id), fields[4]);
            copy(a->user_id, sizeof(a->user_id), fields[5]);
            a->at = when(fields[6]);
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
