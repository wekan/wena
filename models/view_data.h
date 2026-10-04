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

typedef struct WenaViewTime {
    double ms;
    int set;
} WenaViewTime;

typedef struct WenaViewDependency {
    char card_id[WENA_VIEW_ID];
    int blocks;               /* 1 "blocks", 0 "is-blocked-by" */
} WenaViewDependency;

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
} WenaViewCard;

typedef struct WenaViewList {
    char id[WENA_VIEW_ID];
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

#endif
