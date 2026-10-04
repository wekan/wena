#ifndef WENA_MODELS_VIEW_ROWS_H
#define WENA_MODELS_VIEW_ROWS_H
/* What WeKan's other board views show, computed as WeKan computes it, apart
 * from drawing it:
 *   Table          client/components/boards/tableView.js, models/lib/tableViewSort.js
 *   Calendar       boardBody.js calendarOptions, models/lib/calendarFilter.js
 *   Time           chartCalculations.js computeTimeByGroup/ByCard/RemainingTimeSum,
 *                  timeHistory.js timeAdjustments
 *   Group by Assignee  chartCalculations.js computeCardsByAssigneeGroup
 *   Timeline       models/lib/boardTimeline.js reconstructBoardStateAt
 *   Gantt          client/components/gantt/gantt.js, frappeGantt.js and
 *                  dhtmlxGantt.js cardsToTasks
 *   Roadmap        chartCalculations.js computeCardsByCustomFieldGroup
 *   Scrum          models/lib/scrumCardOrder.js, scrum.js getCardEstimate,
 *                  scrumReports.js */
#include "view_data.h"

/* Table ------------------------------------------------------------------- */

typedef enum WenaTableField {
    WENA_TABLE_TITLE, WENA_TABLE_LIST, WENA_TABLE_SWIMLANE, WENA_TABLE_ASSIGNEES, WENA_TABLE_MEMBERS,
    WENA_TABLE_LABELS, WENA_TABLE_RECEIVED, WENA_TABLE_START, WENA_TABLE_DUE, WENA_TABLE_END,
    WENA_TABLE_FIELD_COUNT
} WenaTableField;

typedef struct WenaViewTableRow {
    const WenaViewCard *card;     /* NULL: a swimlane's group heading */
    const char *list_title;
    const char *swimlane_title;
    double swimlane_sort;
    const char *swimlane_id;
} WenaViewTableRow;

#define WENA_TABLE_PAGE 25        /* tableView.js rowsPerPage */
/* The rows of the Table view: cards not archived, in a swimlane and a list
 * that are not, matching `query` in title, list, swimlane or label names,
 * sorted by `field` - grouped by swimlane first when `group` - WeKan's
 * compareTableViewRows. Returns how many there are; `rows` gets them all,
 * group headings included when grouping. NULL `rows` only counts. */
size_t wena_view_table_rows(const WenaViewData *data, const char *query, WenaTableField field, int descending,
                            int group, WenaViewTableRow *rows, size_t capacity);

/* Calendar ---------------------------------------------------------------- */

typedef enum WenaCalendarKind {
    WENA_CALENDAR_INTERVAL,   /* startAt to endAt */
    WENA_CALENDAR_RECEIVED, WENA_CALENDAR_DUE, WENA_CALENDAR_END
} WenaCalendarKind;

typedef struct WenaCalendarEvent {
    const WenaViewCard *card;
    WenaCalendarKind kind;
    double start, end;        /* ms */
} WenaCalendarEvent;

/* WeKan's calendar events from `from` to `to` (ms): a card spanning the
 * range by its start and end, and an hour at its received, due and end
 * dates; by card id as WeKan sorts them. Returns how many. */
size_t wena_view_calendar_events(const WenaViewData *data, double from, double to, WenaCalendarEvent *events,
                                 size_t capacity);

/* Time --------------------------------------------------------------------- */

typedef struct WenaTimeGroup {
    char key[WENA_VIEW_ID];
    char label[WENA_VIEW_TITLE];
    double hours;
    long cards;
} WenaTimeGroup;

typedef struct WenaTimeEntry {
    const char *card_id;
    const char *user_id;
    double at, hours, total;
} WenaTimeEntry;

typedef struct WenaTimeSummary {
    double spent_total;           /* hours, open cards with time spent */
    long cards_with_time, overtime_cards;
    double remaining_hours;       /* computeRemainingTimeSum */
    long remaining_days, remaining_hour;
    long remaining_cards;
    WenaTimeGroup *by_assignee;   /* hours by assignee, most first */
    size_t assignee_count;
    const WenaViewCard **by_card; /* cards with time, most first */
    size_t card_count;
    WenaTimeGroup *adjustments;   /* spentTime changes by author */
    size_t adjustment_count;
    WenaTimeEntry *entries;       /* newest first */
    size_t entry_count;
} WenaTimeSummary;

int wena_view_time(const WenaViewData *data, double now, const char *no_assignee, WenaTimeSummary *summary);
void wena_view_time_free(WenaTimeSummary *summary);

/* Group by Assignee ------------------------------------------------------- */

typedef struct WenaAssigneeGroup {
    char key[WENA_VIEW_ID];
    char label[WENA_VIEW_TITLE];
    const WenaViewCard **cards;
    size_t card_count;
} WenaAssigneeGroup;

/* Cards not archived, under each of their assignees - the no-assignee group
 * last - by card count, most first. Returns the group count, -1 on failure. */
long wena_view_assignee_groups(const WenaViewData *data, const char *no_assignee, WenaAssigneeGroup **groups);
void wena_view_groups_free(WenaAssigneeGroup *groups, long count);

/* Timeline ------------------------------------------------------------------ */

typedef struct WenaTimelineCard {
    const WenaViewCard *card;
    int existed;
    char title[WENA_VIEW_TITLE];
    char description[256];
    char list_id[WENA_VIEW_ID];
    char swimlane_id[WENA_VIEW_ID];
    WenaViewTime due_at;
    int archived;
    size_t label_count, member_count;
    char label_ids[WENA_VIEW_LABELS][WENA_VIEW_ID];
    char members[WENA_VIEW_PEOPLE][WENA_VIEW_ID];
} WenaTimelineCard;

/* The markers: the distinct activity times, at most 50, sampled evenly. */
size_t wena_view_timeline_markers(const WenaViewData *data, double *times, size_t capacity);
/* Every open card as it was at `at` (ms), undoing the activities after it;
 * `live` keeps the cards as they are. `cards` holds data->card_count. */
size_t wena_view_timeline(const WenaViewData *data, int live, double at, WenaTimelineCard *cards);

/* Gantt ------------------------------------------------------------------- */

typedef struct WenaGanttTask {
    const WenaViewCard *card;
    double start, end;        /* ms at UTC midnight */
    int done, overdue;
    int from_received, to_end;
} WenaGanttTask;

/* frappeGantt.js / dhtmlxGantt.js cardsToTasks: a card with a start (else
 * received) date, to its due (else end) date, at least a day. */
size_t wena_view_gantt_tasks(const WenaViewCard *const *cards, size_t count, double now, WenaGanttTask *tasks);

/* The ISO weeks (their Monday, UTC day) that have a card date, in order. */
size_t wena_view_gantt_weeks(const WenaViewData *data, long *weeks, size_t capacity);
/* The ISO week number and year of a UTC day. */
void wena_view_iso_week(long day, long *year, int *week);

/* Roadmap ------------------------------------------------------------------- */

/* Cards grouped by their value of `field_id`, a group per value by name,
 * cards without one last; each group's cards by start then due date.
 * Returns the group count, -1 on failure. */
long wena_view_roadmap_groups(const WenaViewData *data, const char *field_id, const char *no_value,
                              WenaAssigneeGroup **groups);

/* Scrum ------------------------------------------------------------------- */

/* A card's estimate: its Planning Poker estimation or the board's estimate
 * field (getCardEstimate); 0 when it has none. */
int wena_view_card_estimate(const WenaViewData *data, const WenaViewCard *card, double *estimate);
/* The open cards of a sprint - of none for the product backlog - by
 * backlog rank, else sort, then id (compareScrumCards). */
size_t wena_view_scrum_cards(const WenaViewData *data, const char *sprint_id, const WenaViewCard **cards);
/* The closed sprints with a report, by closing time (velocityReports). */
size_t wena_view_velocity(const WenaViewData *data, const WenaViewSprint **sprints);

#endif
