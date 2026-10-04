#include "page_contract.h"
#include <string.h>

static WenaUiTranslator translation_callback;
static void *translation_context;

typedef struct WenaUiTextContract {
    WenaUiTextId id;
    const char *i18n_key;
    const char *fallback_text;
} WenaUiTextContract;

static const WenaUiTextContract texts[] = {
    {WENA_UI_TEXT_ARCHIVE_SELECTION, "archive-selection", "Move selection to Archive"},
    {WENA_UI_TEXT_ADD_CARD_TOP, "add-card-to-top-of-list", "Add Card to Top of List"},
    {WENA_UI_TEXT_ADD_CARD_BOTTOM, "add-card-to-bottom-of-list", "Add Card to Bottom of List"},
    {WENA_UI_TEXT_ADD_LIST, "add-list", "Add List"},
    {WENA_UI_TEXT_LIST_ACTIONS, "listActionPopup-title", "List Actions"},
    {WENA_UI_TEXT_SWIMLANE_ACTIONS, "swimlaneActionPopup-title", "Swimlane Actions"},
    {WENA_UI_TEXT_ADD_SWIMLANE, "add-swimlane", "Add Swimlane"},
    {WENA_UI_TEXT_COLLAPSE, "collapse", "Collapse"},
    {WENA_UI_TEXT_UNCOLLAPSE, "uncollapse", "Uncollapse"},
    {WENA_UI_TEXT_CARD_ACTIONS, "cardDetailsActionsPopup-title", "Card Actions"},
    {WENA_UI_TEXT_ADD_CARD, "add-card", "Add Card"},
    {WENA_UI_TEXT_SET_COLOR, "set-color-list", "Set Color"},
    {WENA_UI_TEXT_SET_WIP_LIMIT, "setWipLimitPopup-title", "Set WIP Limit"},
    {WENA_UI_TEXT_MOVE_LIST, "moveListPopup-title", "Move List"},
    {WENA_UI_TEXT_ARCHIVE_LIST, "archive-list", "Move List to Archive"},
    {WENA_UI_TEXT_MOVE_SWIMLANE, "moveSwimlanePopup-title", "Move Swimlane"},
    {WENA_UI_TEXT_ARCHIVE_SWIMLANE, "archive-swimlane", "Move Swimlane to Archive"},
    {WENA_UI_TEXT_BOARD_SETTINGS, "boardMenuPopup-title", "Board Settings"},
    {WENA_UI_TEXT_CHANGE_LANGUAGE, "changeLanguagePopup-title", "Change Language"},
    {WENA_UI_TEXT_SIDEBAR_OPEN, "sidebar-open", "Open Sidebar"},
    {WENA_UI_TEXT_SIDEBAR_CLOSE, "sidebar-close", "Close Sidebar"},
    {WENA_UI_TEXT_OR, "or", "or"},
    {WENA_UI_TEXT_MEMBER_SETTINGS, "memberMenuPopup-title", "Member Settings"},
    {WENA_UI_TEXT_RENAME, "rename", "Rename"},
    {WENA_UI_TEXT_ADD, "add", "Add"},
    {WENA_UI_TEXT_CLOSE, "close", "Close"},
    {WENA_UI_TEXT_MOVE_CARD, "moveCardPopup-title", "Move Card"},
    {WENA_UI_TEXT_MOVE_TO_TOP, "moveCardToTop-title", "Move to Top"},
    {WENA_UI_TEXT_SEARCH, "search", "Search"},
    {WENA_UI_TEXT_ARCHIVE_CARD, "archive-card", "Move Card to Archive"},
    {WENA_UI_TEXT_CLOSE_CARD, "close-card", "Close Card"},
    {WENA_UI_TEXT_MAXIMIZE_CARD, "maximize-card", "Maximize Card"},
    {WENA_UI_TEXT_MINIMIZE_CARD, "minimize-card", "Minimize Card"},
    {WENA_UI_TEXT_EDIT, "edit", "Edit"},
    {WENA_UI_TEXT_CARD_LABELS_TITLE, "card-labels-title", "Change the labels for the card."},
    {WENA_UI_TEXT_CANCEL, "cancel", "Cancel"},
    {WENA_UI_TEXT_KEYBOARD_SHORTCUTS, "keyboard-shortcuts", "Keyboard shortcuts"},
    {WENA_UI_TEXT_ALL_BOARDS, "all-boards", "All Boards"},
    {WENA_UI_TEXT_ADD_BOARD, "add-board", "Add Board"},
    {WENA_UI_TEXT_REMAINING, "allboards.remaining", "Remaining"},
    {WENA_UI_TEXT_STARRED, "allboards.starred", "Starred"},
    {WENA_UI_TEXT_TEMPLATES, "templates", "Templates"},
    {WENA_UI_TEXT_HOME, "home", "Home"},
    {WENA_UI_TEXT_STAR_BOARD_TITLE, "star-board-title", "Click to star this board. It will show up at top of your boards list."},
    {WENA_UI_TEXT_CLICK_TO_STAR, "click-to-star", "Click to star this board."},
    {WENA_UI_TEXT_CLICK_TO_UNSTAR, "click-to-unstar", "Click to unstar this board."},
    {WENA_UI_TEXT_PRIVATE, "private", "Private"},
    {WENA_UI_TEXT_PUBLIC, "public", "Public"},
    {WENA_UI_TEXT_WATCHING, "watching", "Watching"},
    {WENA_UI_TEXT_TRACKING, "tracking", "Tracking"},
    {WENA_UI_TEXT_MUTED, "muted", "Muted"},
    {WENA_UI_TEXT_CHANGE_VISIBILITY_TITLE, "boardChangeVisibilityPopup-title", "Change Visibility"},
    {WENA_UI_TEXT_CHANGE_WATCH_TITLE, "boardChangeWatchPopup-title", "Change Watch"},
    {WENA_UI_TEXT_SEARCH_EXAMPLE, "search-example", "Write text you search and press Enter"},
    {WENA_UI_TEXT_SORT_CARDS, "sort-cards", "Sort Cards"},
    {WENA_UI_TEXT_SORT_IS_ON, "sort-is-on", "Sort is on"},
    {WENA_UI_TEXT_REMOVE_SORT, "remove-sort", "Remove sort"},
    {WENA_UI_TEXT_CARDS_SORT_TITLE, "cardsSortPopup-title", "Sort Cards"},
    {WENA_UI_TEXT_DUE_DATE, "due-date", "Due Date"},
    {WENA_UI_TEXT_TITLE_ALPHABETICALLY, "title-alphabetically", "Title (Alphabetically)"},
    {WENA_UI_TEXT_CREATED_NEWEST, "created-at-newest-first", "Created At (Newest First)"},
    {WENA_UI_TEXT_CREATED_OLDEST, "created-at-oldest-first", "Created At (Oldest First)"},
    {WENA_UI_TEXT_SORT_BY_VOTES, "sort-by-votes", "Sort by votes"},
    {WENA_UI_TEXT_BOARD_VIEW_SWIMLANES, "board-view-swimlanes", "Swimlanes"},
    {WENA_UI_TEXT_BOARD_VIEW_LISTS, "board-view-lists", "Lists"},
    {WENA_UI_TEXT_BOARD_VIEW_CALENDAR, "board-view-cal", "Calendar"},
    {WENA_UI_TEXT_BOARD_VIEW_GANTT, "board-view-gantt", "Gantt"},
    {WENA_UI_TEXT_BOARD_VIEW_TABLE, "board-view-table", "Table"},
    {WENA_UI_TEXT_BOARD_VIEW_TITLE, "boardChangeViewPopup-title", "Board View"},
    {WENA_UI_TEXT_NOTIFICATIONS, "notifications", "Notifications"},
    {WENA_UI_TEXT_MARK_ALL_READ, "mark-all-as-read", "Mark all as read"},
    {WENA_UI_TEXT_SELECTED, "selected-label", "Selected:"},
    {WENA_UI_TEXT_SELECT_LIST_CARDS, "list-select-cards", "Select all cards in this list"},
    {WENA_UI_TEXT_MULTI_SELECTION, "multi-selection", "Multi-Selection"},
    {WENA_UI_TEXT_MULTI_SELECTION_OFF, "multi-selection-off", "Turn Multi-Selection off"},
    {WENA_UI_TEXT_SELECT_ALL, "select-all", "Select all"},
    {WENA_UI_TEXT_SELECT_NONE, "select-none", "Select none"},
    {WENA_UI_TEXT_ARCHIVE_LIST_CARDS, "list-archive-cards", "Archive all cards in this list"},
    {WENA_UI_TEXT_ARCHIVE_LIST_CARDS_CONFIRM, "list-archive-cards-pop", "This will remove all the cards in this list from the board. To view cards in Archive and bring them back to the board, click \342\200\234Menu\342\200\235 > \342\200\234Archive\342\200\235."},
    {WENA_UI_TEXT_MOVE_TO_ARCHIVE, "archive", "Move to Archive"},
    {WENA_UI_TEXT_PREVIOUS_PAGE, "previous-page", "Previous Page"},
    {WENA_UI_TEXT_BOARDS, "boards", "Boards"},
    {WENA_UI_TEXT_LOADING, "loading", "Loading, please wait."},
    {WENA_UI_TEXT_NEXT_PAGE, "next-page", "Next Page"},
    {WENA_UI_TEXT_ACTIVITIES, "activities", "Activities"},
    {WENA_UI_TEXT_MEMBERS, "members", "Members"},
    {WENA_UI_TEXT_LABELS, "labels", "Labels"},
    {WENA_UI_TEXT_ARCHIVES, "archives", "Archives"},
    {WENA_UI_TEXT_REFRESH, "refresh", "Refresh"},
    {WENA_UI_TEXT_ADD_MEMBER, "add-members", "Add member"},
    {WENA_UI_TEXT_ADD_LABEL, "add-label", "Add label"},
    {WENA_UI_TEXT_REMOVE_LABEL, "remove-label", "Remove Label"},
    {WENA_UI_TEXT_RESTORE, "restore", "Restore selected"},
    {WENA_UI_TEXT_LANGUAGE, "language", "Language"},
    {WENA_UI_TEXT_SWIMLANE, "swimlane", "Swimlane"},
    {WENA_UI_TEXT_LISTS, "lists", "Lists"},
    {WENA_UI_TEXT_SWIMLANES, "swimlanes", "Swimlanes"},
    {WENA_UI_TEXT_NO_ARCHIVED_SWIMLANES, "no-archived-swimlanes", "No swimlanes in Archive."},
    {WENA_UI_TEXT_NO_ARCHIVED_LISTS, "no-archived-lists", "No lists in Archive."},
    {WENA_UI_TEXT_EDIT_WIP_LIMIT, "edit-wip-limit", "Edit WIP Limit"},
    {WENA_UI_TEXT_ENABLE_WIP_LIMIT, "enable-wip-limit", "Enable WIP Limit"},
    {WENA_UI_TEXT_SOFT_WIP_LIMIT, "soft-wip-limit", "Soft WIP Limit"},
    {WENA_UI_TEXT_LIST, "list", "List"},
    {WENA_UI_TEXT_NO_ARCHIVED_CARDS, "no-archived-cards", "No archived cards"},
    {WENA_UI_TEXT_ERROR, "error", "Error"},
    {WENA_UI_TEXT_OPERATION_FAILED, "error-undefined", "Something went wrong"},
    {WENA_UI_TEXT_NO_ITEMS, "no-items-message", "No items."},
    {WENA_UI_TEXT_UNKNOWN, "no-name", "(Unknown)"},
    {WENA_UI_TEXT_CARD_DETAILS, "cardDetailsPopup-title", "Card Details"},
    {WENA_UI_TEXT_MANUAL_ORDER, "list-label-sort", "Your Manual Order"},
    {WENA_UI_TEXT_MOVE_TO_BOTTOM, "moveCardToBottom-title", "Move to Bottom"},
    {WENA_UI_TEXT_ARCHIVED, "archived", "Archived"},
    {WENA_UI_TEXT_DESCRIPTION, "description", "Description"},
    {WENA_UI_TEXT_CHECKLISTS, "checklists", "Checklists"},
    {WENA_UI_TEXT_CHECKLIST, "checklist", "Checklist"},
    {WENA_UI_TEXT_CHECKLIST_COUNT, "checklist-count", "Checklist item count (0/0)"},
    {WENA_UI_TEXT_COMPLETE, "complete", "Complete"},
    {WENA_UI_TEXT_FILTER, "filter", "Filter"},
    {WENA_UI_TEXT_FILTER_CARD_TITLE, "filter-card-title-label", "Filter by card title"},
    {WENA_UI_TEXT_FILTER_CLEAR, "filter-clear", "Clear filter"},
    {WENA_UI_TEXT_NO_CARDS_FOUND, "no-cards-found", "No Cards Found"},
    {WENA_UI_TEXT_HIDE_CHECKED_ITEMS, "hideCheckedChecklistItems", "Hide checked checklist items"},
    {WENA_UI_TEXT_HIDE_ALL_ITEMS, "hideAllChecklistItems", "Hide all checklist items"},
    {WENA_UI_TEXT_SHOW_ON_MINICARD, "show-on-minicard", "Show on Minicard"},
    {WENA_UI_TEXT_DEFAULT, "default", "Default"},
    {WENA_UI_TEXT_YES, "yes", "Yes"},
    {WENA_UI_TEXT_NO, "no", "No"},
    {WENA_UI_TEXT_SETTINGS, "settings", "Settings"},
    {WENA_UI_TEXT_DELETE_CHECKLIST, "checklistDeletePopup-title", "Delete Checklist?"},
    {WENA_UI_TEXT_DELETE_CHECKLIST_ITEM, "checklistItemDeletePopup-title", "Delete Checklist Item?"},
    {WENA_UI_TEXT_CONFIRM_DELETE_CHECKLIST, "confirm-checklist-delete-popup", "Are you sure you want to delete the checklist?"},
    {WENA_UI_TEXT_CONFIRM_DELETE_CHECKLIST_ITEM, "confirm-checklist-item-delete-popup", "Are you sure you want to delete the checklist item?"},
    {WENA_UI_TEXT_CHECKLIST_WITH_ITEMS, "r-with-items", "with items"},
    {WENA_UI_TEXT_NAME, "name", "Name"},
    {WENA_UI_TEXT_SELECT_COLOR, "select-color", "Select Color"},
    {WENA_UI_TEXT_CUSTOM_COLOR, "custom-color", "Custom color"},
    {WENA_UI_TEXT_CREATE_LABEL, "createLabelPopup-title", "Create Label"},
    {WENA_UI_TEXT_EDIT_LABEL, "editLabelPopup-title", "Change Label"},
    {WENA_UI_TEXT_DELETE_LABEL, "deleteLabelPopup-title", "Delete Label?"},
    {WENA_UI_TEXT_CARDS, "cards", "Cards"},
    {WENA_UI_TEXT_CHECKLIST_SPLIT_LINES, "newlineBecomesNewChecklistItem", "Each line of text becomes one of the checklist items"},
    {WENA_UI_TEXT_CHECKLIST_COUNT_ON_MINICARD, "checklist-count-on-minicard", "Checklist item count (0/0) on minicard"},
    {WENA_UI_TEXT_MOVE_SELECTION, "move-selection", "Move selection"},
    {WENA_UI_TEXT_MOVE_CHECKLIST, "moveChecklist", "Move Checklist"},
    {WENA_UI_TEXT_MOVE_DESTINATION, "move-destination", "Destination"}
};

void wena_ui_set_translator(WenaUiTranslator translator, void *context)
{
    translation_callback = translator;
    translation_context = translator == NULL ? NULL : context;
}

static const char *translated(const char *key, const char *fallback)
{
    const char *value;
    if (translation_callback != NULL) {
        value = translation_callback(translation_context, key);
        if (value != NULL && value[0] != '\0') return value;
    }
    return fallback;
}

/* WeKan's texts looked up by their key, for what has many of them - the
 * board views and their charts. WENA_UI_FORMAT marks a text with __name__
 * placeholders that its caller fills in (scripts/generate_ui_i18n.py). */
#define WENA_UI_KEY 0
#define WENA_UI_FORMAT 1
static const struct {
    int kind;
    const char *key;
    const char *fallback;
} key_texts[] = {
    {WENA_UI_KEY, "date", "Date"},
    {WENA_UI_KEY, "board-view-wip-run", "WIP Run"},
    {WENA_UI_KEY, "card", "Card"},
    {WENA_UI_KEY, "completed", "Completed"},
    {WENA_UI_KEY, "board-view-cycle-time", "Cycle Time"},
    {WENA_UI_KEY, "board-view-lead-time", "Lead Time"},
    {WENA_UI_KEY, "board-view-burndown", "Burndown"},
    {WENA_UI_KEY, "board-view-throughput-histogram", "Throughput Histogram"},
    {WENA_UI_KEY, "cards", "Cards"},
    {WENA_UI_KEY, "chart-forecast-none-remaining", "Nothing left open to project - every card is done."},
    {WENA_UI_KEY, "board-view-flow-efficiency", "Flow Efficiency"},
    {WENA_UI_KEY, "board-view-pulse", "Pulse"},
    {WENA_UI_KEY, "no-assignee", "No assignee"},
    {WENA_UI_KEY, "no-label", "No label"},
    {WENA_UI_KEY, "name", "Name"},
    {WENA_UI_KEY, "assignees", "Assignees"},
    {WENA_UI_KEY, "labels", "Labels"},
    {WENA_UI_KEY, "lists", "Lists"},
    {WENA_UI_KEY, "list", "List"},
    {WENA_UI_KEY, "flow-age-days", "Age in stage (days)"},
    {WENA_UI_KEY, "flow-p85", "Stage 85th percentile (days)"},
    {WENA_UI_KEY, "flow-samples", "Historical stays"},
    {WENA_UI_KEY, "flow-signal", "Signal"},
    {WENA_UI_KEY, "flow-unknown", "Unknown"},
    {WENA_UI_KEY, "flow-unusual", "Outside limit"},
    {WENA_UI_KEY, "flow-cycle-days", "Cycle time (days)"},
    {WENA_UI_KEY, "flow-mean", "Mean"},
    {WENA_UI_KEY, "flow-moving-range", "Moving range"},
    {WENA_UI_KEY, "flow-mr-mean", "Mean moving range"},
    {WENA_UI_KEY, "flow-size-source", "Size field"},
    {WENA_UI_KEY, "flow-size", "Estimate"},
    {WENA_UI_KEY, "poker-question", "Planning Poker"},
    {WENA_UI_KEY, "flow-confidence", "Confidence"},
    {WENA_UI_KEY, "flow-target-count", "Target cards"},
    {WENA_UI_KEY, "flow-finish-days", "Days to finish"},
    {WENA_UI_KEY, "flow-finish-date", "Finish date"},
    {WENA_UI_KEY, "flow-target-date", "Target date"},
    {WENA_UI_KEY, "flow-capacity", "At least this many cards"},
    {WENA_UI_KEY, "flow-history-days", "History days"},
    {WENA_UI_KEY, "flow-beyond-horizon", "Beyond simulation horizon"},
    {WENA_UI_KEY, "flow-blocker", "Blocking card"},
    {WENA_UI_KEY, "flow-episodes", "Episodes"},
    {WENA_UI_KEY, "flow-active", "Open episodes"},
    {WENA_UI_KEY, "flow-blocked-days", "Blocked card-days"},
    {WENA_UI_KEY, "flow-unknown-start", "Unknown starts"},
    {WENA_UI_KEY, "card-start", "Start"},
    {WENA_UI_KEY, "card-end", "End"},
    {WENA_UI_KEY, "days", "days"},
    {WENA_UI_KEY, "flow-note-agingWip", "Open cards: days since entering the current stage. Red bars exceed its historical 85th percentile (at least five stays). Missing entry history is unknown."},
    {WENA_UI_KEY, "flow-note-blockerAnalysis", "Dependency history gives blocker start/end times per stage. Older links may have unknown starts. Deleted cards require retained history snapshots. Overlapping causes count separately."},
    {WENA_UI_KEY, "flow-note-monteCarlo", "2,000 trials sample full UTC calendar days, including zero-throughput days. Dates are upper-tail forecasts; counts are lower-tail commitments. Assumes similar future throughput; not a guarantee. Maximum horizon: 3,650 days. No completions means no forecast."},
    {WENA_UI_KEY, "flow-note-processBehavior", "Cycle time uses Start, falling back to creation, and End, falling back to archive. XmR shows individuals and successive differences, with mean and natural process limits; at least two valid completions are required."},
    {WENA_UI_KEY, "flow-note-sizeCycleTime", "Current Planning Poker estimate or one numeric custom field versus completed cycle time in days. Missing estimates and invalid dates are omitted. Start falls back to creation; End falls back to archive."},
    {WENA_UI_KEY, "flow-details", "Underlying history"},
    {WENA_UI_KEY, "export", "Export"},
    {WENA_UI_KEY, "no-results", "No results"},
    {WENA_UI_KEY, "loading", "Loading, please wait."},
    {WENA_UI_KEY, "flow-error", "Could not load the report. Check the values and try again."},
    {WENA_UI_KEY, "apply", "Apply"},
    {WENA_UI_KEY, "swimlanes", "Swimlanes"},
    {WENA_UI_KEY, "board-view-lists", "Lists"},
    {WENA_UI_KEY, "board-view-table", "Table"},
    {WENA_UI_KEY, "board-view-cal", "Calendar"},
    {WENA_UI_KEY, "board-view-multiboard-cal", "Multi Board Calendar"},
    {WENA_UI_KEY, "board-view-time", "Time"},
    {WENA_UI_KEY, "board-view-timeline", "Timeline"},
    {WENA_UI_KEY, "board-view-stats", "Statistics"},
    {WENA_UI_KEY, "board-view-group-by-assignee", "Group by Assignee"},
    {WENA_UI_KEY, "board-view-gantt", "Gantt"},
    {WENA_UI_KEY, "board-view-gantt-frappe", "Frappe Gantt"},
    {WENA_UI_KEY, "board-view-gantt-dhtmlx", "DHTMLX Gantt"},
    {WENA_UI_KEY, "board-view-product-backlog", "Product Backlog"},
    {WENA_UI_KEY, "board-view-sprints", "Sprints"},
    {WENA_UI_KEY, "board-view-sprint-report", "Sprint Report"},
    {WENA_UI_KEY, "board-view-velocity", "Velocity"},
    {WENA_UI_KEY, "board-view-roadmap", "Roadmap"},
    {WENA_UI_KEY, "board-view-dashboard", "Dashboard"},
    {WENA_UI_KEY, "board-view-bigboard", "Bigboard"},
    {WENA_UI_KEY, "board-view-burnup", "Burnup"},
    {WENA_UI_KEY, "board-view-cumulative-flow", "Cumulative Flow"},
    {WENA_UI_KEY, "board-view-control-chart", "Control"},
    {WENA_UI_KEY, "board-view-aging-wip", "Aging WIP"},
    {WENA_UI_KEY, "board-view-blocker-analysis", "Blocker Analysis"},
    {WENA_UI_KEY, "board-view-monte-carlo", "Monte Carlo Forecasts"},
    {WENA_UI_KEY, "board-view-process-behavior", "Process Behavior (XmR)"},
    {WENA_UI_KEY, "board-view-size-cycle-time", "Work Item Size vs. Cycle Time"},
    {WENA_UI_KEY, "board-view-map", "Map"},
    {WENA_UI_KEY, "board-status", "Board status"},
    {WENA_UI_KEY, "board-status-cards-with-time", "Cards with time spent"},
    {WENA_UI_KEY, "board-status-loading-mode", "Card loading"},
    {WENA_UI_KEY, "board-status-overtime-cards", "Overtime cards"},
    {WENA_UI_KEY, "board-status-remaining-time-total", "Remaining time until due"},
    {WENA_UI_KEY, "board-status-time-spent-total", "Total time spent"},
    {WENA_UI_KEY, "board-status-time-summary", "Time spent summary"},
    {WENA_UI_KEY, "board-table-group-by-swimlane-off", "Cards are shown as a flat list. Click to group them by swimlane instead."},
    {WENA_UI_KEY, "board-table-group-by-swimlane-on", "Cards are grouped by swimlane. Click to show them as a flat list instead."},
    {WENA_UI_KEY, "board-view-timeline-hint", "Click a point in time to see the board as it was then."},
    {WENA_UI_KEY, "board-view-timeline-now", "Now"},
    {WENA_UI_KEY, "board-view-timeline-showing", "Showing state as of:"},
    {WENA_UI_KEY, "card-archived", "This card is moved to Archive."},
    {WENA_UI_KEY, "card-due", "Due"},
    {WENA_UI_KEY, "card-received", "Received"},
    {WENA_UI_KEY, "cards-loading-all", "All cards"},
    {WENA_UI_KEY, "custom-fields", "Custom Fields"},
    {WENA_UI_KEY, "day", "Day"},
    {WENA_UI_KEY, "duration", "Duration"},
    {WENA_UI_KEY, "friday", "Friday"},
    {WENA_UI_KEY, "group-by-assignee-empty", "No cards."},
    {WENA_UI_KEY, "hours", "hours"},
    {WENA_UI_KEY, "monday", "Monday"},
    {WENA_UI_KEY, "month", "Month"},
    {WENA_UI_KEY, "next", "Next"},
    {WENA_UI_KEY, "no-boards-selected", "You did not select any boards."},
    {WENA_UI_KEY, "overtime", "Overtime"},
    {WENA_UI_KEY, "predicate-week", "week"},
    {WENA_UI_KEY, "previous", "Previous"},
    {WENA_UI_KEY, "roadmap-empty-no-custom-fields", "This board has no custom fields yet. Add a \"Version\" or \"Release\"-style custom field in Board Settings to use the Roadmap view."},
    {WENA_UI_KEY, "roadmap-group-by", "Group by:"},
    {WENA_UI_KEY, "roadmap-no-cards", "No cards."},
    {WENA_UI_KEY, "roadmap-no-value", "No value"},
    {WENA_UI_KEY, "saturday", "Saturday"},
    {WENA_UI_KEY, "scrum-added", "Added to scope"},
    {WENA_UI_KEY, "scrum-backlog", "Backlog"},
    {WENA_UI_KEY, "scrum-backlog-help", "Order work by backlog rank. Assign cards to a planned or active sprint."},
    {WENA_UI_KEY, "scrum-backlog-rank", "Backlog rank"},
    {WENA_UI_KEY, "scrum-committed", "Committed"},
    {WENA_UI_KEY, "scrum-completed", "Completed"},
    {WENA_UI_KEY, "scrum-estimate", "Estimate"},
    {WENA_UI_KEY, "scrum-estimate-unit", "Estimate unit"},
    {WENA_UI_KEY, "scrum-event-daily", "Daily Scrum"},
    {WENA_UI_KEY, "scrum-event-planning", "Sprint Planning"},
    {WENA_UI_KEY, "scrum-event-retrospective", "Sprint Retrospective"},
    {WENA_UI_KEY, "scrum-event-review", "Sprint Review"},
    {WENA_UI_KEY, "scrum-events", "Sprint events"},
    {WENA_UI_KEY, "scrum-incomplete", "Incomplete"},
    {WENA_UI_KEY, "scrum-no-closed-sprints", "No closed sprint snapshot is available."},
    {WENA_UI_KEY, "scrum-product-backlog", "Product Backlog"},
    {WENA_UI_KEY, "scrum-release", "Release"},
    {WENA_UI_KEY, "scrum-released-at", "Released at"},
    {WENA_UI_KEY, "scrum-releases", "Releases and increments"},
    {WENA_UI_KEY, "scrum-removed", "Removed from scope"},
    {WENA_UI_KEY, "scrum-report-help", "Reports retain estimates from sprint snapshots. Unknown estimates are counted separately; they are not zero estimates. Compare velocity only across matching estimate units and policies."},
    {WENA_UI_KEY, "scrum-select-sprint", "Select a sprint to view its report."},
    {WENA_UI_KEY, "scrum-sprint", "Sprint"},
    {WENA_UI_KEY, "scrum-sprints", "Sprints"},
    {WENA_UI_KEY, "scrum-state-active", "Active"},
    {WENA_UI_KEY, "scrum-state-cancelled", "Cancelled"},
    {WENA_UI_KEY, "scrum-state-closed", "Closed"},
    {WENA_UI_KEY, "scrum-state-planned", "Planned"},
    {WENA_UI_KEY, "scrum-unknown-estimate", "Unknown estimate"},
    {WENA_UI_KEY, "scrum-working-days", "Working days"},
    {WENA_UI_KEY, "sunday", "Sunday"},
    {WENA_UI_KEY, "task", "Task"},
    {WENA_UI_KEY, "thursday", "Thursday"},
    {WENA_UI_KEY, "time-adjustment-note", "Changes to recorded hours, grouped by the person editing the total. These are not individual work sessions. Negative values are corrections. Earlier unrecorded time cannot be attributed."},
    {WENA_UI_KEY, "time-adjustments", "Time adjustments by author"},
    {WENA_UI_KEY, "today", "Today"},
    {WENA_UI_KEY, "tuesday", "Tuesday"},
    {WENA_UI_KEY, "username", "Username"},
    {WENA_UI_KEY, "wednesday", "Wednesday"},
    {WENA_UI_KEY, "week", "Week"},
    {WENA_UI_FORMAT, "chart-forecast-no-velocity", "__remaining__ card(s) still open; no recent completions to project a date from."},
    {WENA_UI_FORMAT, "chart-forecast-projected", "At the recent pace of __average__ card(s)/week, the __remaining__ card(s) still open should be done by __date__."},
};

const char *wena_ui_key_text(const char *key, const char *fallback)
{
    size_t i;
    if (key == NULL) return fallback != NULL ? fallback : "";
    if (fallback == NULL)
        for (i = 0; i < sizeof(key_texts) / sizeof(key_texts[0]); ++i)
            if (!strcmp(key_texts[i].key, key)) { fallback = key_texts[i].fallback; break; }
    return translated(key, fallback != NULL ? fallback : key);
}

const char *wena_ui_text(WenaUiTextId id)
{
    size_t i;
    for (i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i)
        if (texts[i].id == id)
            return translated(texts[i].i18n_key, texts[i].fallback_text);
    return "";
}

static const WenaUiControlContract controls[] = {
    {WENA_UI_BOARD_MENU, "board", "Board menu", "[>]", "GET", "open-board-menu", 1u},
    {WENA_UI_ADD_CARD, "add-card", "Add card", "[+]", "POST", "create-card", 2u},
    {WENA_UI_LIST_MENU, "list", "List menu", "[>]", "GET", "open-list-menu", 3u},
    {WENA_UI_OPEN_CARD, "minicardDetailsActionsPopup-title", "Open card", "[>]", "GET", "open-card", 4u},
    {WENA_UI_CARD_MENU, "cardDetailsActionsPopup-title", "Card menu", "[>]", "GET", "open-card-menu", 5u},
    {WENA_UI_EDIT_TITLE, "edit", "Edit title", "[E]", "POST", "edit-card-title", 6u},
    {WENA_UI_ARCHIVE_CARD, "archive-card", "Archive card", "[A]", "POST", "archive-card", 7u},
    {WENA_UI_CLOSE, "close", "Close details", "[X]", "GET", "close-card", 8u},
    {WENA_UI_MOVE_CARD, "move-card-up", "Move card", "[>]", "POST", "move-card", 9u},
    {WENA_UI_MOVE_LIST, "move-list-right", "Move list", "[>]", "POST", "move-list", 10u},
    {WENA_UI_MOVE_SWIMLANE, "move-swimlane", "Move swimlane", "[>]", "POST", "move-swimlane", 11u},
    {WENA_UI_SAVE, "save", "Save", "[S]", "GET", "save-card-editor", 12u},
    {WENA_UI_CANCEL, "cancel", "Cancel", "[X]", "GET", "cancel-card-editor", 13u},
    {WENA_UI_COLLAPSE_LIST, "collapse", "Collapse", "[-]", "GET", "collapse-list", 14u},
    {WENA_UI_EXPAND_LIST, "uncollapse", "Uncollapse", "[+]", "GET", "expand-list", 15u},
    {WENA_UI_COLLAPSE_SWIMLANE, "collapse", "Collapse", "[-]", "GET", "collapse-swimlane", 16u},
    {WENA_UI_EXPAND_SWIMLANE, "uncollapse", "Uncollapse", "[+]", "GET", "expand-swimlane", 17u},
    {WENA_UI_MOVE_CARD_TO, "moveCardPopup-title", "Move card", "[>]", "GET", "open-move-card", 18u},
    {WENA_UI_ADD_LIST, "add-list", "Add list", "[+]", "GET", "open-create-list", 19u},
    {WENA_UI_ADD_SWIMLANE, "add-swimlane", "Add swimlane", "[+]", "GET", "open-create-swimlane", 20u},
    {WENA_UI_RENAME_BOARD, "rename", "Rename board", "[E]", "GET", "open-rename-board", 21u},
    {WENA_UI_RENAME_SWIMLANE, "rename", "Rename swimlane", "[E]", "GET", "open-rename-swimlane", 22u},
    {WENA_UI_RESTORE_CARD, "restore", "Restore", "[R]", "GET", "restore-card-editor", 23u},
    {WENA_UI_MOVE_LIST_TO, "moveListPopup-title", "Move List", "[>]", "GET", "open-move-list", 24u},
    {WENA_UI_MOVE_SWIMLANE_TO, "moveSwimlanePopup-title", "Move Swimlane", "[>]", "GET", "open-move-swimlane", 25u},
    {WENA_UI_EDIT_DESCRIPTION, "description", "Description", "[E]", "GET", "open-card-description", 26u},
    {WENA_UI_OPEN_CHECKLISTS, "checklists", "Checklists", "[>]", "GET", "open-checklists", 27u},
    {WENA_UI_ADD_CHECKLIST, "add-checklist", "Add Checklist", "[+]", "GET", "open-create-checklist", 28u},
    {WENA_UI_ADD_CHECKLIST_ITEM, "add-checklist-item", "Add an item to checklist", "[+]", "GET", "open-create-checklist-item", 29u},
    {WENA_UI_RENAME_CHECKLIST, "rename", "Rename", "[E]", "GET", "open-rename-checklist", 30u},
    {WENA_UI_RENAME_CHECKLIST_ITEM, "edit", "Edit", "[E]", "GET", "open-rename-checklist-item", 31u},
    {WENA_UI_CHECKLIST_SETTINGS, "checklistActionsPopup-title", "Checklist Actions", "[>]", "GET", "open-checklist-settings", 32u},
    {WENA_UI_OPEN_DELETE_CHECKLIST, "delete", "Delete", "[X]", "GET", "open-delete-checklist", 33u},
    {WENA_UI_OPEN_DELETE_CHECKLIST_ITEM, "delete", "Delete", "[X]", "GET", "open-delete-checklist-item", 34u},
    {WENA_UI_CONFIRM_DELETE, "delete", "Delete", "[X]", "GET", "confirm-local-delete", 35u},
    {WENA_UI_OPEN_LABELS, "cardLabelsPopup-title", "Labels", "[>]", "GET", "open-card-labels", 36u},
    {WENA_UI_ADD_LABEL, "label-create", "Create Label", "[+]", "GET", "open-create-label", 37u},
    {WENA_UI_EDIT_LABEL, "editLabelPopup-title", "Change Label", "[E]", "GET", "open-edit-label", 38u},
    {WENA_UI_OPEN_DELETE_LABEL, "delete", "Delete", "[X]", "GET", "open-delete-label", 39u},
    {WENA_UI_CREATE_LABEL, "create", "Create", "[+]", "GET", "create-local-label", 40u},
    {WENA_UI_ARCHIVE_LIST, "archive-list", "Move List to Archive", "[A]", "GET", "archive-list-editor", 41u},
    {WENA_UI_ARCHIVE_SWIMLANE, "archive-swimlane", "Move Swimlane to Archive", "[A]", "GET", "archive-swimlane-editor", 42u}
};

static const WenaUiPageContract pages[] = {
    {"/", "loginPopup-title"}, {"/sign-in", "loginPopup-title"},
    {"/sign-up", "signupPopup-title"}, {"/allboards", "all-boards"},
    {"/public", "public"}, {"/my-cards", "my-cards"},
    {"/due-cards", "dueCards-title"}, {"/global-search", "globalSearch-title"},
    {"/bookmarks", "bookmarksPopup-title"}, {"/import", "import"},
    {"/support", "support"}, {"/accessibility", "accessibility"},
    {"/shortcuts", "keyboard-shortcuts"}, {"/admin", "admin-panel"},
    {"/b/:boardId/:slug", "board"}
};



const WenaUiControlContract *wena_ui_control(WenaUiControlId id)
{
    size_t index;
    for (index = 0; index < sizeof(controls) / sizeof(controls[0]); ++index) {
        if (controls[index].id == id) {
            return &controls[index];
        }
    }
    return NULL;
}

const char *wena_ui_control_text(WenaUiControlId id)
{
    const WenaUiControlContract *control;
    control = wena_ui_control(id);
    return control == NULL ? "" :
        translated(control->i18n_key, control->fallback_text);
}

const WenaUiPageContract *wena_ui_pages(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(pages) / sizeof(pages[0]);
    }
    return pages;
}

const WenaUiColorContract *wena_ui_colors(size_t *count)
{
    return wena_color_contracts(count);
}
