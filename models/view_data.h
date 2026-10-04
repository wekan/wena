#ifndef WENA_MODELS_VIEW_DATA_H
#define WENA_MODELS_VIEW_DATA_H
/* What WeKan's board views read of a board, as plain records: its cards with
 * the fields the views use, its lists, swimlanes, labels and members, and its
 * activities and change history - the records server/lib/boardChartData.js
 * loads for the charts. Times are milliseconds since 1970 (UTC); a time that
 * is not set is 0 with its has_ flag clear, never a real 0. */
#include <stddef.h>

#define WENA_VIEW_ID 65
#define WENA_VIEW_TITLE 129
#define WENA_VIEW_PEOPLE 8
#define WENA_VIEW_LABELS 8
#define WENA_VIEW_DEPENDENCIES 8
#define WENA_VIEW_FIELDS 8

typedef struct WenaViewTime {
    double ms;
    int set;
} WenaViewTime;

typedef struct WenaViewDependency {
    char card_id[WENA_VIEW_ID];
    int blocks;               /* 1 "blocks", 0 "is-blocked-by" */
} WenaViewDependency;

/* A card's value of a board custom field, as text ("" when not set); a
 * number keeps its number too. */
typedef struct WenaViewFieldValue {
    char field_id[WENA_VIEW_ID];
    char value[WENA_VIEW_TITLE];
    double number;
    int is_number;
} WenaViewFieldValue;

typedef struct WenaViewCard {
    char id[WENA_VIEW_ID];
    char title[WENA_VIEW_TITLE];
    char board_id[WENA_VIEW_ID];
    char list_id[WENA_VIEW_ID];
    char swimlane_id[WENA_VIEW_ID];
    double sort;
    int archived;
    WenaViewTime created_at, archived_at, start_at, end_at, due_at, received_at, deleted_at, modified_at;
    double spent_time;        /* hours */
    int is_overtime;
    int has_spent_time;
    int votes_positive, votes_negative;
    double poker;             /* the poker estimation */
    int has_poker;
    size_t assignee_count, member_count, label_count, dependency_count;
    char assignees[WENA_VIEW_PEOPLE][WENA_VIEW_ID];
    char members[WENA_VIEW_PEOPLE][WENA_VIEW_ID];
    char label_ids[WENA_VIEW_LABELS][WENA_VIEW_ID];
    WenaViewDependency dependencies[WENA_VIEW_DEPENDENCIES];
    char description[256];    /* the start of it, for the Table view */
    char board_title[WENA_VIEW_TITLE];  /* the calendar of several boards names it */
    int card_number;          /* 0 when the board does not number cards */
    WenaViewTime map_x, map_y; /* the Map view's place, percent */
    size_t field_count;
    WenaViewFieldValue fields[WENA_VIEW_FIELDS];
    /* card.scrum */
    char sprint_id[WENA_VIEW_ID];
    char release_id[WENA_VIEW_ID];
    char issue_type[WENA_VIEW_TITLE];
    WenaViewTime backlog_rank; /* not a time: a rank, set or not */
    int due_complete;
} WenaViewCard;

typedef struct WenaViewCustomField {
    char id[WENA_VIEW_ID];
    char name[WENA_VIEW_TITLE];
    char type[24];            /* text, dropdown, number ... */
} WenaViewCustomField;

/* A board's sprint, release or scrum event (scrumSprints, scrumReleases,
 * scrumEvents), with what its views show. */
typedef struct WenaViewSprint {
    char id[WENA_VIEW_ID];
    char name[WENA_VIEW_TITLE];
    char goal[256];
    char state[16];           /* planned, active, closed, cancelled */
    WenaViewTime planned_start, planned_end, closed_at;
    /* sprint.report, as the server stored it on close */
    int has_report;
    char unit[WENA_VIEW_TITLE];
    double totals[5][3];      /* committed, completed, added, removed, incomplete: count, estimate, unknown */
    int working_days;         /* -1 when not known */
} WenaViewSprint;

typedef struct WenaViewRelease {
    char id[WENA_VIEW_ID];
    char name[WENA_VIEW_TITLE];
    char goal[256];
    char state[16];
    char notes[256];
    WenaViewTime planned_start, planned_end, released_at;
} WenaViewRelease;

typedef struct WenaViewEvent {
    char id[WENA_VIEW_ID];
    char sprint_id[WENA_VIEW_ID];
    char kind[16];            /* planning, daily, review, retrospective */
    char name[WENA_VIEW_TITLE];
    WenaViewTime starts_at;
    double timebox_minutes;
    char notes[256];
} WenaViewEvent;

typedef struct WenaViewList {
    char id[WENA_VIEW_ID];
    char board_id[WENA_VIEW_ID];
    char title[WENA_VIEW_TITLE];
    double sort;
    int wip_enabled;
    int wip_value;
} WenaViewList;

typedef struct WenaViewSwimlane {
    char id[WENA_VIEW_ID];
    char title[WENA_VIEW_TITLE];
    double sort;
} WenaViewSwimlane;

typedef struct WenaViewLabel {
    char id[WENA_VIEW_ID];
    char name[WENA_VIEW_TITLE];
    char color[33];
} WenaViewLabel;

typedef struct WenaViewUser {
    char id[WENA_VIEW_ID];
    char name[WENA_VIEW_TITLE];   /* full name, else username */
} WenaViewUser;

typedef struct WenaViewActivity {
    char type[40];            /* activityType */
    char card_id[WENA_VIEW_ID];
    char list_id[WENA_VIEW_ID];
    char old_list_id[WENA_VIEW_ID];
    char user_id[WENA_VIEW_ID];
    WenaViewTime at;
    /* What the Timeline undoes (models/lib/boardTimeline.js). */
    char old_swimlane_id[WENA_VIEW_ID];
    char member_id[WENA_VIEW_ID];
    char assignee_id[WENA_VIEW_ID];
    char label_id[WENA_VIEW_ID];
    char old_value[256];
    char time_key[16];
    WenaViewTime time_old;
    int has_old_list, has_old_swimlane;
} WenaViewActivity;

/* One changeHistory row about a card, as the flow charts replay it: a move
 * (group "position"), a lifecycle change, or one field's old and new value. */
typedef enum WenaViewChangeKind {
    WENA_VIEW_CHANGE_POSITION,
    WENA_VIEW_CHANGE_LIFECYCLE,
    WENA_VIEW_CHANGE_FIELD
} WenaViewChangeKind;
typedef struct WenaViewChange {
    WenaViewChangeKind kind;
    char card_id[WENA_VIEW_ID];
    char user_id[WENA_VIEW_ID];
    char field[32];           /* FIELD: endAt, archivedAt, archived, deletedAt, spentTime, cardDependencies */
    char old_list_id[WENA_VIEW_ID], new_list_id[WENA_VIEW_ID];   /* POSITION */
    double old_value, new_value;    /* FIELD: a number or a time */
    int has_old, has_new;
    int removed;              /* LIFECYCLE: the card was removed (no new document) */
    int restored;             /* LIFECYCLE: the card came back */
    /* FIELD cardDependencies: the old and new dependencies, in the data's
     * change_dependencies. */
    size_t old_dependency_start, old_dependency_count, new_dependency_start, new_dependency_count;
    WenaViewTime at;
} WenaViewChange;

typedef struct WenaViewData {
    char board_id[WENA_VIEW_ID];
    char board_title[WENA_VIEW_TITLE];
    char board_color[33];
    int active_members;
    char map_image[WENA_VIEW_ID];   /* board.mapImageAttachmentId */
    /* board.scrum: where estimates come from, and their unit. */
    int estimate_from_field;
    char estimate_field_id[WENA_VIEW_ID];
    char estimate_unit[WENA_VIEW_TITLE];
    struct WenaViewBoard {       /* every board, for Bigboard */
        char id[WENA_VIEW_ID];
        char title[WENA_VIEW_TITLE];
    } *boards;
    size_t board_count;
    WenaViewCustomField *custom_fields;
    size_t custom_field_count;
    struct WenaViewFieldItem {   /* a dropdown field's items */
        char field_id[WENA_VIEW_ID];
        char item_id[WENA_VIEW_ID];
        char name[WENA_VIEW_TITLE];
    } *field_items;
    size_t field_item_count;
    WenaViewSprint *sprints;
    size_t sprint_count;
    WenaViewRelease *releases;
    size_t release_count;
    WenaViewEvent *events;
    size_t event_count;
    WenaViewCard *cards;
    size_t card_count;
    WenaViewList *lists;      /* not archived, by sort: the board left to right */
    size_t list_count;
    WenaViewSwimlane *swimlanes;
    size_t swimlane_count;
    WenaViewLabel *labels;
    size_t label_count;
    WenaViewUser *users;
    size_t user_count;
    WenaViewActivity *activities;   /* by time */
    size_t activity_count;
    WenaViewChange *changes;        /* by time */
    size_t change_count;
    WenaViewDependency *change_dependencies;
    size_t change_dependency_count;
} WenaViewData;

void wena_view_data_free(WenaViewData *data);
/* The user's name, else the id itself. */
const char *wena_view_user_name(const WenaViewData *data, const char *id);
const WenaViewList *wena_view_list(const WenaViewData *data, const char *id);
const WenaViewCard *wena_view_card(const WenaViewData *data, const char *id);
const WenaViewLabel *wena_view_label(const WenaViewData *data, const char *id);
/* A card's value of a custom field as WeKan shows it (customFieldsWD's
 * trueValue): a dropdown's item name, else the value; "" when none. */
const char *wena_view_field_value(const WenaViewData *data, const WenaViewCard *card, const char *field_id);

#endif
