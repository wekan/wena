#include "components/cards/checklist_contents.h"
/* MIT-licensed local desktop entrypoint. The local OS user supplies a trusted
 * actor identity; this is not a remote authentication or board-sharing API. */
#include <SDL.h>
#include "platform/svg.h"
#include "components/boards/board_header.h"
#include "components/common/wekan_look.h"
#include "platform/sdl_nuklear.h"
#include "platform/theme.h"
#include "platform/dependencies.h"
#include "platform/font.h"
#include "platform/debug_log.h"
#include "platform/files.h"
#include "components/boards/swimlane_resize.h"
#include "features/board.h"
#include "features/board_filter.h"
#include "features/card_mutation.h"
#include "features/card_drag.h"
#include "features/boards/reload.h"
#include "features/card_actions.h"
#include "features/card_create.h"
#include "features/card_move.h"
#include "features/card_archives.h"
#include "features/card_selection_panel.h"
#include "features/card_description.h"
#include "features/card_description_mutation.h"
#include "features/checklists.h"
#include "../server/sqlite_directory.h"
#include "features/checklist_mutation.h"
#include "features/labels/panel.h"
#include "features/labels/mutation.h"
#include "features/labels/badges.h"
#include "features/boards/presentation.h"
#include "features/boards/settings_panel.h"
#include "features/checklists/badges.h"
#include "features/language_picker.h"
#include "features/hierarchy_title.h"
#include "features/hierarchy_mutation.h"
#include "features/hierarchy_move.h"
#include "features/hierarchy_drag.h"
#include "features/hierarchy_move_mutation.h"
#include "components/cards/card_body.h"
#include "components/lists/list_header.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include "../server/sqlite_workspace.h"
#include "../server/embedded_migration.h"
#include "../server/executable_path.h"
#include "../server/wekan_sync.h"
#include "../server/mutations/common.h"
#include "platform/wekan_files.h"
#include "../imports/i18n/ui_catalog.h"
#include "../imports/i18n/locale.h"
#include "../imports/preferences/collapse.h"
#include "../imports/ui/page_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__AROS__)
#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#endif

static unsigned int desktop_card_badges(struct nk_context *context,
    void *opaque, const WenaCard *card)
{
    WenaBoardPresentation *view;
    unsigned int actions;
    view = (WenaBoardPresentation *)opaque;
    actions = view->valid ? wena_label_badges_render(context, view->badges, card) :
                           WENA_CARD_BODY_NO_ACTION;
    if (view->summary_valid)
        actions |= wena_checklist_badges_render(context, view->summary, card);
    return actions;
}

typedef struct WenaDesktopChecklistPreview {
    WenaBoardPresentation *view;
    WenaChecklistCompletionIntent intent;
    WenaCardSectionControl sections;
    WenaChecklistInlineEdit inline_edit;
    WenaChecklistDrag drag;
    WenaCardDrag card_drag;
    WenaHierarchyDrag hierarchy_drag;
    const WenaBoardLayout *layout;
    int readonly;
    int error;
} WenaDesktopChecklistPreview;

static unsigned int desktop_card_contents(struct nk_context *context,
    void *opaque, const WenaCard *card)
{
    WenaDesktopChecklistPreview *preview;
    preview = (WenaDesktopChecklistPreview *)opaque;
    return preview->view->summary_valid ? wena_checklist_contents_render_editable(context,
        preview->view->contents, card, preview->view->settings.show_checklists,
        preview->readonly || preview->error ? NULL : &preview->intent, &preview->sections,
        preview->readonly || preview->error ? NULL : &preview->inline_edit, preview->readonly || preview->error ? NULL : &preview->drag) : WENA_CARD_BODY_NO_ACTION;
}

/* Card details' sections, from what the board already has loaded: the label
 * chips, the checklists (items can be ticked, as on the minicard), and the
 * description, read once per card and again only after a write. */
typedef struct WenaDesktopCardDetails {
    WenaDesktopChecklistPreview *preview;
    WenaCardDescriptionMutation *descriptions;
    WenaId card_id;
    sqlite3_int64 changes;
    int loaded;
    char description[WENA_DESCRIPTION_CAPACITY];
} WenaDesktopCardDetails;

static unsigned int desktop_card_details_section(struct nk_context *context, void *opaque,
    const WenaCard *card, WenaCardDetailsSection section)
{
    WenaDesktopCardDetails *details = (WenaDesktopCardDetails *)opaque;
    WenaBoardPresentation *view = details->preview->view;
    unsigned long version;
    struct nk_rect row;
    int lines;
    if (section == WENA_CARD_DETAILS_SECTION_LABELS) {
        return view->valid && (wena_label_badges_render(context, view->badges, card) &
                               WENA_CARD_BODY_OPEN_LABELS) != 0u ?
            WENA_CARD_DETAILS_LABELS : WENA_CARD_DETAILS_NO_ACTION;
    }
    if (section == WENA_CARD_DETAILS_SECTION_CHECKLISTS) {
        return view->summary_valid && (wena_checklist_contents_render_actions(context,
                view->contents, card, 1, details->preview->readonly ? NULL : &details->preview->intent) &
                WENA_CARD_BODY_OPEN_CHECKLISTS) != 0u ?
            WENA_CARD_DETAILS_CHECKLISTS : WENA_CARD_DETAILS_NO_ACTION;
    }
    if (!details->loaded || strcmp(details->card_id, card->id) != 0 ||
        details->changes != view->observed_changes) {
        details->description[0] = '\0';
        version = 0ul;
        if (!wena_card_description_mutation_load(details->descriptions, card->board_id, card->id,
                details->description, sizeof(details->description), &version))
            details->description[0] = '\0';
        strcpy(details->card_id, card->id);
        details->changes = view->observed_changes;
        details->loaded = 1;
    }
    if (details->description[0] == '\0') return WENA_CARD_DETAILS_NO_ACTION;
    nk_layout_row_dynamic(context, 1.0f, 1);
    row = nk_widget_bounds(context);
    nk_spacer(context); /* one slot: nk_spacing wraps to a new row */
    lines = wena_wekan_wrapped_lines(context, details->description, WENA_WEKAN_FONT_BODY, row.w);
    nk_layout_row_dynamic(context, (float)lines * 18.0f + 4.0f, 1);
    /* Clicking the text edits it, as in WeKan. */
    return wena_wekan_text_button(context, details->description, WENA_WEKAN_FONT_BODY,
                                  WENA_WEKAN_TEXT) ? WENA_CARD_DETAILS_DESCRIPTION : WENA_CARD_DETAILS_NO_ACTION;
}

static void desktop_card_drag(struct nk_context *context,void *opaque,
    const WenaCard *card,size_t ordinal)
{
    WenaDesktopChecklistPreview *preview;
    const WenaChecklistCardSummary *summary;
    preview=(WenaDesktopChecklistPreview *)opaque;
    summary=preview->view->summary_valid ?
        wena_checklist_summary_find(&preview->view->contents->summary,card->id) : NULL;
    wena_card_drag_handle(context,&preview->card_drag,preview->layout->cards,
        preview->layout->card_count,card,ordinal,summary && !summary->archived ? summary->card_version : 0,
        !preview->readonly && !preview->error && !preview->inline_edit.action &&
        !preview->drag.gesture.active);
}

static void desktop_list_drag(struct nk_context *context,void *opaque,
    const WenaList *list,size_t ordinal)
{
    WenaDesktopChecklistPreview *preview;
    preview=(WenaDesktopChecklistPreview *)opaque;
    wena_hierarchy_drag_handle(context,&preview->hierarchy_drag,preview->layout,
        WENA_HIERARCHY_LIST,list->id,ordinal,!preview->readonly && !preview->error &&
        !preview->inline_edit.action && !preview->drag.gesture.active && !preview->card_drag.gesture.active);
}
static void desktop_swimlane_drag(struct nk_context *context,void *opaque,
    const WenaSwimlane *lane,size_t ordinal)
{
    WenaDesktopChecklistPreview *preview;
    preview=(WenaDesktopChecklistPreview *)opaque;
    wena_hierarchy_drag_handle(context,&preview->hierarchy_drag,preview->layout,
        WENA_HIERARCHY_SWIMLANE,lane->id,ordinal,!preview->readonly && !preview->error &&
        !preview->inline_edit.action && !preview->drag.gesture.active && !preview->card_drag.gesture.active);
}

static void desktop_card_drop(struct nk_context *context,void *opaque,
    const WenaList *list,const WenaSwimlane *lane)
{
    WenaDesktopChecklistPreview *preview;
    preview=(WenaDesktopChecklistPreview *)opaque;
    if (!preview->readonly && !preview->error)
        wena_card_drag_destination(context,&preview->card_drag,list,lane);
}

/* WeKan's dragging: the whole minicard, list header or swimlane bar. */
static int desktop_drag_enabled(const WenaDesktopChecklistPreview *preview)
{
    return !preview->readonly && !preview->error && !preview->inline_edit.action &&
        !preview->drag.gesture.active;
}
static void desktop_card_drag_area(struct nk_context *context,void *opaque,
    const WenaCard *card,size_t ordinal,const struct nk_rect *area,int *clicked)
{
    WenaDesktopChecklistPreview *preview;
    const WenaChecklistCardSummary *summary;
    preview=(WenaDesktopChecklistPreview *)opaque;
    summary=preview->view->summary_valid ?
        wena_checklist_summary_find(&preview->view->contents->summary,card->id) : NULL;
    wena_card_drag_area(context,&preview->card_drag,preview->layout->cards,
        preview->layout->card_count,card,ordinal,summary && !summary->archived ? summary->card_version : 0,
        desktop_drag_enabled(preview),area,clicked);
}
static void desktop_list_drag_area(struct nk_context *context,void *opaque,
    const WenaList *list,size_t ordinal,const struct nk_rect *area,int *clicked)
{
    WenaDesktopChecklistPreview *preview;
    preview=(WenaDesktopChecklistPreview *)opaque;
    wena_hierarchy_drag_area(context,&preview->hierarchy_drag,preview->layout,WENA_HIERARCHY_LIST,
        list->id,ordinal,desktop_drag_enabled(preview) && !preview->card_drag.gesture.active,area,clicked);
}
static void desktop_swimlane_drag_area(struct nk_context *context,void *opaque,
    const WenaSwimlane *lane,size_t ordinal,const struct nk_rect *area,int *clicked)
{
    WenaDesktopChecklistPreview *preview;
    preview=(WenaDesktopChecklistPreview *)opaque;
    wena_hierarchy_drag_area(context,&preview->hierarchy_drag,preview->layout,WENA_HIERARCHY_SWIMLANE,
        lane->id,ordinal,desktop_drag_enabled(preview) && !preview->card_drag.gesture.active,area,clicked);
}
static void desktop_card_drop_area(struct nk_context *context,void *opaque,
    const WenaList *list,const WenaSwimlane *lane,const struct nk_rect *area)
{
    WenaDesktopChecklistPreview *preview;
    preview=(WenaDesktopChecklistPreview *)opaque;
    if (!preview->readonly && !preview->error)
        wena_card_drag_destination_area(context,&preview->card_drag,list,lane,area);
}

static int desktop_card_collapsed(struct nk_context *context,
    void *opaque, const WenaCard *card)
{
    WenaDesktopChecklistPreview *preview;
    int collapsed;
    preview = (WenaDesktopChecklistPreview *)opaque;
    if (!preview->view->summary_valid || !preview->view->settings.allow_minicard_collapse) return 0;
    collapsed = wena_card_section_toggle_caret(context, &preview->sections,
        card->board_id, card->id, "minicard");
    if (collapsed && preview->inline_edit.action &&
        !strcmp(preview->inline_edit.card_id, card->id))
        wena_checklist_inline_cancel(&preview->inline_edit);
    return collapsed;
}

#define DESKTOP_ADD_LIST 1u
#define DESKTOP_ADD_SWIMLANE 2u
#define DESKTOP_RENAME_BOARD 4u
#define DESKTOP_BOARD_SETTINGS 8u
/* WeKan's popups: List Actions, Swimlane Actions, the user's menu. */
typedef enum WenaDesktopMenuKind {
    DESKTOP_MENU_NONE, DESKTOP_MENU_LIST, DESKTOP_MENU_SWIMLANE, DESKTOP_MENU_MEMBER,
    DESKTOP_MENU_CARD
} WenaDesktopMenuKind;
typedef struct WenaDesktopMenu {
    WenaDesktopMenuKind kind;
    WenaId list_id, swimlane_id, card_id;
} WenaDesktopMenu;
typedef struct WenaDesktopToolbar {
    WenaLanguagePicker *language;
    int filter_visible, language_visible;
    unsigned int header_actions;
    WenaDesktopMenu menu;
    WenaBoardFilterState *filter;
    WenaBoardPresentation *labels;
    WenaDesktopChecklistPreview *preview;
    int filter_changed;
    int board_refresh;
    int card_menu_error;      /* a Card Actions item that could not be done */
    int collapse_error;
    int collapse_retry;
    int collapse_writable;
    unsigned int actions;
} WenaDesktopToolbar;

static void desktop_toolbar(struct nk_context *context, void *opaque)
{
    WenaDesktopToolbar *toolbar;
    toolbar = (WenaDesktopToolbar *)opaque;
    toolbar->actions = 0u;
    toolbar->filter_changed = 0;
    toolbar->board_refresh = 0;
    toolbar->collapse_retry = 0;
    /* Under WeKan's header only what needs retrying is shown; the rest is in
     * the header, its menus and the Filter panel (desktop_panels). */
    if (toolbar->labels != NULL && toolbar->labels->error) {
        nk_layout_row_dynamic(context, 22.0f, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_LABELS), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28.0f, 2);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH)))
            toolbar->labels->refresh_pending = 1;
    }
    if (toolbar->labels != NULL && (toolbar->labels->summary_error || toolbar->labels->sections_error ||
        toolbar->preview->error || toolbar->preview->sections.error || toolbar->preview->drag.error || toolbar->preview->card_drag.error || toolbar->preview->hierarchy_drag.error)) {
        nk_layout_row_dynamic(context, 22.0f, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_CARDS), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28.0f, 2);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH))) {
            toolbar->labels->summary_pending = 1;
            toolbar->preview->error = 0;
            toolbar->preview->drag.error = 0;
            toolbar->board_refresh = toolbar->preview->card_drag.error || toolbar->preview->hierarchy_drag.error;
            toolbar->preview->sections.error = 0;
            toolbar->labels->sections_pending = 1;
        }
    }
    if (toolbar->card_menu_error) {
        nk_layout_row_dynamic(context, 22.0f, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_CARD_ACTIONS), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28.0f, 2);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_CLOSE)))
            toolbar->card_menu_error = 0;
    }
    if (toolbar->collapse_error) {
        nk_layout_row_dynamic(context, 22.0f, 2);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_SETTINGS), NK_TEXT_LEFT);
        nk_label(context, wena_ui_control_text(WENA_UI_COLLAPSE_LIST), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28.0f, toolbar->collapse_writable ? 2 : 1);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        if (toolbar->collapse_writable &&
            nk_button_label(context, wena_ui_control_text(WENA_UI_SAVE)))
            toolbar->collapse_retry = 1;
    }
}

typedef enum WenaDesktopPanel {
    DESKTOP_PANEL_NONE,
    DESKTOP_PANEL_DETAILS,
    DESKTOP_PANEL_CREATE_CARD,
    DESKTOP_PANEL_MOVE_CARD,
    DESKTOP_PANEL_ARCHIVES,
    DESKTOP_PANEL_SELECTION,
    DESKTOP_PANEL_CARD_TRANSFER,
    DESKTOP_PANEL_DESCRIPTION,
    DESKTOP_PANEL_CHECKLISTS,
    DESKTOP_PANEL_LABELS,
    DESKTOP_PANEL_BOARD_SETTINGS,
    DESKTOP_PANEL_HIERARCHY_TITLE,
    DESKTOP_PANEL_HIERARCHY_MOVE
} WenaDesktopPanel;

typedef struct WenaDesktopEditors {
    WenaCardDetailsState details;
    WenaCardCreateState create;
    WenaCardMoveState move;
    WenaCardArchivesState archives;
    WenaCardSelectionPanel selection,card_transfer;
    WenaCardDescriptionState description;
    WenaChecklistsState checklists;
    WenaLabelsState labels;
    WenaBoardSettingsState board_settings;
    WenaHierarchyTitleState hierarchy;
    WenaHierarchyMoveState hierarchy_move;
} WenaDesktopEditors;

/* One focus boundary for every local editor prevents stale dialogs from
 * retaining a previous object when another action opens a new panel. */
static void desktop_close_other_editors(WenaDesktopEditors *editors,
                                        WenaDesktopPanel keep)
{
    if (keep != DESKTOP_PANEL_DETAILS)
        wena_card_details_close(&editors->details);
    if (keep != DESKTOP_PANEL_CREATE_CARD)
        wena_card_create_close(&editors->create);
    if (keep != DESKTOP_PANEL_MOVE_CARD)
        wena_card_move_close(&editors->move);
    if (keep != DESKTOP_PANEL_SELECTION) wena_card_selection_panel_hide(&editors->selection);
    if (keep != DESKTOP_PANEL_CARD_TRANSFER) wena_card_selection_panel_hide(&editors->card_transfer);
    if (keep != DESKTOP_PANEL_ARCHIVES)
        wena_card_archives_close(&editors->archives);
    if (keep != DESKTOP_PANEL_DESCRIPTION)
        wena_card_description_close(&editors->description);
    if (keep != DESKTOP_PANEL_CHECKLISTS)
        wena_checklists_close(&editors->checklists);
    if (keep != DESKTOP_PANEL_LABELS)
        wena_labels_close(&editors->labels);
    if (keep != DESKTOP_PANEL_BOARD_SETTINGS)
        wena_board_settings_close(&editors->board_settings);
    if (keep != DESKTOP_PANEL_HIERARCHY_TITLE)
        wena_hierarchy_title_close(&editors->hierarchy);
    if (keep != DESKTOP_PANEL_HIERARCHY_MOVE)
        wena_hierarchy_move_close(&editors->hierarchy_move);
}

/* What WeKan's sidebar lists: the board's members (read when it opens: its
 * active members and the board's own user) and its labels (from the label
 * catalogue the board already has). */
#define DESKTOP_SIDEBAR_MEMBERS 32
typedef struct WenaDesktopSidebarData {
    int was_visible;
    size_t member_count;
    char member_names[DESKTOP_SIDEBAR_MEMBERS][WENA_TITLE_CAPACITY];
    const char *members[DESKTOP_SIDEBAR_MEMBERS];
    const char *labels[WENA_BOARD_LABEL_CAPACITY];
    const char *label_colors[WENA_BOARD_LABEL_CAPACITY];
} WenaDesktopSidebarData;

static void desktop_sidebar_members(WenaDesktopSidebarData *data, sqlite3 *database,
                                    const char *actor, const char *board)
{
    sqlite3_stmt *query;
    const unsigned char *name;
    data->member_count = 0;
    query = NULL;
    if (sqlite3_prepare_v2(database,
        "SELECT display_name FROM actors WHERE id=?1 UNION "
        "SELECT a.display_name FROM board_members m JOIN actors a ON a.id=m.actor_id "
        "WHERE m.board_id=?2 AND m.active=1 ORDER BY 1 LIMIT 32", -1, &query, NULL) != SQLITE_OK) return;
    if (sqlite3_bind_text(query, 1, actor, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(query, 2, board, -1, SQLITE_TRANSIENT) == SQLITE_OK) {
        while (data->member_count < DESKTOP_SIDEBAR_MEMBERS && sqlite3_step(query) == SQLITE_ROW) {
            name = sqlite3_column_text(query, 0);
            if (name == NULL || strlen((const char *)name) >= WENA_TITLE_CAPACITY) continue;
            strcpy(data->member_names[data->member_count], (const char *)name);
            data->members[data->member_count] = data->member_names[data->member_count];
            ++data->member_count;
        }
    }
    sqlite3_finalize(query);
}

static void desktop_sidebar_fill(WenaDesktopSidebarData *data, WenaBoardSidebar *sidebar,
    const WenaBoardPresentation *view, sqlite3 *database, const char *actor, const char *board)
{
    size_t index, count;
    if (sidebar->visible && !data->was_visible) desktop_sidebar_members(data, database, actor, board);
    data->was_visible = sidebar->visible;
    count = view->valid ? view->badges->catalogue.label_count : 0;
    if (count > WENA_BOARD_LABEL_CAPACITY) count = WENA_BOARD_LABEL_CAPACITY;
    for (index = 0; index < count; ++index) {
        data->labels[index] = view->badges->catalogue.labels[index].name;
        data->label_colors[index] = view->badges->catalogue.labels[index].color;
    }
    sidebar->items.members = data->members;
    sidebar->items.member_count = data->member_count;
    sidebar->items.labels = data->labels;
    sidebar->items.label_colors = data->label_colors;
    sidebar->items.label_count = count;
}

/* WeKan's inline Add Card composer, in the list it adds to. */
typedef struct WenaDesktopComposer {
    WenaDesktopEditors *editors;
    WenaCardMutation *mutation;
    const WenaBoardLayout *layout;
    const size_t *card_count;   /* the snapshot's, already counting a new card */
} WenaDesktopComposer;

static int desktop_card_composer(struct nk_context *context, void *opaque, const WenaList *list,
                                 const WenaSwimlane *lane, int bottom)
{
    WenaDesktopComposer *composer = (WenaDesktopComposer *)opaque;
    return wena_card_create_render_inline(context, &composer->editors->create, composer->layout,
                                          list, lane, bottom);
}

/* Add Card to Top of List: the card just created, first in its list. */
static int desktop_card_to_top(void *opaque, const WenaBoardLayout *layout)
{
    WenaDesktopComposer *composer = (WenaDesktopComposer *)opaque;
    WenaBoardLayout current = *layout;
    current.card_count = *composer->card_count;
    return wena_card_move_to_end(&composer->editors->move, &current,
                                 composer->mutation->persistence.created_card_id, 0);
}

/* WeKan's popups and panels over the board: the List and Swimlane Actions
 * menus (their items carry out the same actions as Wena's panels), the
 * user's menu with the language, and the Filter panel at the right. Returns
 * an opened panel. */
static WenaDesktopPanel desktop_menus(struct nk_context *context, WenaDesktopToolbar *toolbar,
    WenaDesktopEditors *editors, const WenaBoardLayout *layout, float width, float height)
{
    WenaDesktopMenu *menu;
    WenaWekanMenuItem items[8];
    WenaListInteraction target;
    WenaHierarchyKind kind;
    float menu_width;
    int chosen, readonly;
    size_t count;
    menu = &toolbar->menu;
    readonly = !editors->hierarchy.load_title;
    menu_width = width - 64.0f < 960.0f ? width - 64.0f : 960.0f;
    if (menu_width < 240.0f) menu_width = 240.0f;
    count = 0;
    memset(items, 0, sizeof(items));
#define DESKTOP_ITEM(icon_, text_, enabled_, separator_) do { items[count].icon = icon_; \
    items[count].text = wena_ui_text(text_); items[count].enabled = enabled_; \
    items[count].separator_before = separator_; ++count; } while (0)
    if (menu->kind == DESKTOP_MENU_LIST) {
        DESKTOP_ITEM(WENA_ICON_PLUS, WENA_UI_TEXT_ADD_CARD_TOP, !readonly, 0);
        DESKTOP_ITEM(WENA_ICON_PLUS, WENA_UI_TEXT_ADD_CARD_BOTTOM, !readonly, 0);
        DESKTOP_ITEM(WENA_ICON_PLUS, WENA_UI_TEXT_ADD_LIST, !readonly, 1);
        DESKTOP_ITEM(WENA_ICON_BRUSH, WENA_UI_TEXT_SET_COLOR, !readonly, 0);
        DESKTOP_ITEM(WENA_ICON_NONE, WENA_UI_TEXT_SELECT_LIST_CARDS, editors->hierarchy.selection_enabled, 0);
        DESKTOP_ITEM(WENA_ICON_ARCHIVE, WENA_UI_TEXT_ARCHIVE_LIST_CARDS, editors->hierarchy.archive_list_cards != NULL, 0);
        DESKTOP_ITEM(WENA_ICON_BAN, WENA_UI_TEXT_SET_WIP_LIMIT, editors->hierarchy.save_wip != NULL, 0);
        DESKTOP_ITEM(WENA_ICON_ARROW_RIGHT, WENA_UI_TEXT_MOVE_LIST, !readonly, 0);
        chosen = wena_wekan_menu(context, wena_ui_text(WENA_UI_TEXT_LIST_ACTIONS), 32.0f, 12.0f,
                                 menu_width, 3, items, count);
    } else if (menu->kind == DESKTOP_MENU_SWIMLANE) {
        DESKTOP_ITEM(WENA_ICON_PLUS, WENA_UI_TEXT_ADD_SWIMLANE, !readonly, 0);
        DESKTOP_ITEM(WENA_ICON_PLUS, WENA_UI_TEXT_ADD_LIST, !readonly, 1);
        DESKTOP_ITEM(WENA_ICON_BRUSH, WENA_UI_TEXT_SELECT_COLOR, !readonly, 1);
        DESKTOP_ITEM(WENA_ICON_ARROW_UP, WENA_UI_TEXT_MOVE_SWIMLANE, !readonly, 0);
        DESKTOP_ITEM(WENA_ICON_ARCHIVE, WENA_UI_TEXT_ARCHIVE_SWIMLANE, !readonly, 1);
        chosen = wena_wekan_menu(context, wena_ui_text(WENA_UI_TEXT_SWIMLANE_ACTIONS), 32.0f, 12.0f,
                                 menu_width, 2, items, count);
    } else if (menu->kind == DESKTOP_MENU_CARD) {
        count = wena_card_actions_items(items, &editors->move, &editors->details);
        chosen = wena_wekan_menu(context, wena_ui_text(WENA_UI_TEXT_CARD_ACTIONS), 32.0f, 12.0f,
                                 menu_width, 3, items, count);
    } else if (menu->kind == DESKTOP_MENU_MEMBER) {
        DESKTOP_ITEM(WENA_ICON_GLOBE, WENA_UI_TEXT_CHANGE_LANGUAGE, 1, 0);
        chosen = wena_wekan_menu(context, wena_ui_text(WENA_UI_TEXT_MEMBER_SETTINGS),
                                 width - 340.0f > 0.0f ? width - 340.0f : 0.0f, 80.0f, 320.0f, 1, items, count);
    } else chosen = WENA_WEKAN_MENU_NONE;
#undef DESKTOP_ITEM
    if (chosen == WENA_WEKAN_MENU_CLOSED) menu->kind = DESKTOP_MENU_NONE;
    if (chosen >= 0) {
        kind = menu->kind == DESKTOP_MENU_LIST ? WENA_HIERARCHY_LIST : WENA_HIERARCHY_SWIMLANE;
        memset(&target, 0, sizeof(target));
        strcpy(target.board_id, layout->board->id);
        strcpy(target.list_id, menu->list_id);
        strcpy(target.swimlane_id, menu->swimlane_id);
        if (menu->kind == DESKTOP_MENU_MEMBER) {
            toolbar->language_visible = 1;
        } else if (menu->kind == DESKTOP_MENU_CARD) {
            menu->kind = DESKTOP_MENU_NONE;
            desktop_close_other_editors(editors, chosen == WENA_CARD_ACTION_MOVE ?
                DESKTOP_PANEL_MOVE_CARD : DESKTOP_PANEL_NONE);
            switch (wena_card_actions_apply((WenaCardAction)chosen, &editors->move, &editors->details,
                                            layout, menu->card_id)) {
            case WENA_CARD_ACTIONS_OPEN_MOVE: return DESKTOP_PANEL_MOVE_CARD;
            case WENA_CARD_ACTIONS_FAILED: toolbar->card_menu_error = 1; break;
            default: break;
            }
            return DESKTOP_PANEL_NONE;
        } else if (menu->kind == DESKTOP_MENU_LIST && chosen <= 1) {
            desktop_close_other_editors(editors, DESKTOP_PANEL_CREATE_CARD);
            target.actions = WENA_LIST_HEADER_ADD_CARD | (chosen == 1 ? WENA_LIST_HEADER_ADD_CARD_BOTTOM : 0u);
            menu->kind = DESKTOP_MENU_NONE;
            return wena_card_create_open(&editors->create, layout, &target) ?
                DESKTOP_PANEL_CREATE_CARD : DESKTOP_PANEL_NONE;
        } else if ((menu->kind == DESKTOP_MENU_LIST && chosen == 2) ||
                   (menu->kind == DESKTOP_MENU_SWIMLANE && chosen <= 1)) {
            toolbar->actions |= menu->kind == DESKTOP_MENU_SWIMLANE && chosen == 0 ?
                DESKTOP_ADD_SWIMLANE : DESKTOP_ADD_LIST;
        } else {
            static const WenaHierarchyRequest list_requests[] = {
                WENA_HIERARCHY_REQUEST_NONE, WENA_HIERARCHY_REQUEST_NONE, WENA_HIERARCHY_REQUEST_NONE,
                WENA_HIERARCHY_REQUEST_COLOR, WENA_HIERARCHY_REQUEST_SELECT_CARDS,
                WENA_HIERARCHY_REQUEST_ARCHIVE_CARDS, WENA_HIERARCHY_REQUEST_WIP, WENA_HIERARCHY_REQUEST_MOVE};
            static const WenaHierarchyRequest lane_requests[] = {
                WENA_HIERARCHY_REQUEST_NONE, WENA_HIERARCHY_REQUEST_NONE, WENA_HIERARCHY_REQUEST_COLOR,
                WENA_HIERARCHY_REQUEST_MOVE, WENA_HIERARCHY_REQUEST_ARCHIVE};
            desktop_close_other_editors(editors, DESKTOP_PANEL_HIERARCHY_TITLE);
            if (kind == WENA_HIERARCHY_LIST ?
                    wena_hierarchy_title_open_list(&editors->hierarchy, layout, menu->list_id, menu->swimlane_id) :
                    wena_hierarchy_title_open(&editors->hierarchy, layout, kind, menu->swimlane_id))
                (void)wena_hierarchy_title_request(&editors->hierarchy, kind == WENA_HIERARCHY_LIST ?
                    list_requests[chosen] : lane_requests[chosen]);
        }
        menu->kind = DESKTOP_MENU_NONE;
    }
    /* The Filter panel: WeKan's filter sidebar, at the right under the header. */
    if (toolbar->filter_visible) {
        nk_style_push_style_item(context, &context->style.window.fixed_background,
                                 nk_style_item_color(nk_rgb(0xf7, 0xf7, 0xf7)));
        if (nk_begin(context, "Wena filter", nk_rect(width - 420.0f > 0.0f ? width - 420.0f : 0.0f, 88.0f,
                     width < 420.0f ? width : 420.0f, height > 88.0f ? height - 88.0f : height),
                     NK_WINDOW_NO_SCROLLBAR)) {
            wena_ui_region("sidebar");
            nk_layout_row_begin(context, NK_DYNAMIC, 32.0f, 2);
            nk_layout_row_push(context, 0.88f);
            wena_wekan_text(context, wena_ui_text(WENA_UI_TEXT_FILTER), WENA_WEKAN_FONT_SECTION,
                            WENA_WEKAN_TEXT, NK_TEXT_LEFT);
            nk_layout_row_push(context, 0.10f);
            if (wena_wekan_icon_button(context, WENA_ICON_TIMES, wena_ui_text(WENA_UI_TEXT_CLOSE), 14.0f,
                                       WENA_WEKAN_TEXT))
                toolbar->filter_visible = 0;
            nk_layout_row_end(context);
            toolbar->filter_changed = wena_board_filter_render(context, toolbar->filter);
        }
        nk_end(context);
        nk_style_pop_style_item(context);
    }
    /* Change Language, from the user's menu. */
    if (toolbar->language_visible) {
        if (nk_begin_titled(context, "Wena language", wena_ui_text(WENA_UI_TEXT_CHANGE_LANGUAGE),
                nk_rect(width - 420.0f > 0.0f ? width - 420.0f : 0.0f, 88.0f, width < 400.0f ? width : 400.0f, 150.0f),
                NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {
            wena_language_picker_render(context, toolbar->language);
            nk_layout_row_dynamic(context, 30.0f, 1);
            if (wena_wekan_button(context, wena_ui_text(WENA_UI_TEXT_CLOSE), WENA_WEKAN_BUTTON))
                toolbar->language_visible = 0;
        }
        nk_end(context);
    }
    return DESKTOP_PANEL_NONE;
}

static const WenaCard *desktop_selected_card(const WenaSqliteBoardSnapshot *snapshot,
                                              const char *card_id)
{
    const WenaCard *selected;
    size_t index;
    selected = NULL;
    for (index = 0; index < snapshot->card_count; ++index) {
        if (!snapshot->cards[index].archived &&
            !strcmp(snapshot->cards[index].id, card_id)) {
            if (selected != NULL) return NULL;
            selected = &snapshot->cards[index];
        }
    }
    return selected;
}

static int desktop_collapse_changed(const WenaBoardCollapseState *current,
                                     const WenaBoardCollapseState *previous)
{
    size_t index;
    if (strcmp(current->board_id, previous->board_id) ||
        current->list_count != previous->list_count ||
        current->swimlane_count != previous->swimlane_count) return 1;
    for (index = 0; index < current->list_count; ++index)
        if (strcmp(current->list_ids[index], previous->list_ids[index])) return 1;
    for (index = 0; index < current->swimlane_count; ++index)
        if (strcmp(current->swimlane_ids[index], previous->swimlane_ids[index])) return 1;
    if (current->height_count != previous->height_count) return 1;
    for (index = 0; index < current->height_count; ++index)
        if (strcmp(current->height_ids[index], previous->height_ids[index]) ||
            current->heights[index] != previous->heights[index]) return 1;
    return 0;
}

static int actor_exists(sqlite3 *database, const char *actor)
{
    sqlite3_stmt *query;
    int valid;
    query = NULL;
    if (sqlite3_prepare_v2(database, "SELECT 1 FROM actors WHERE id=?1",
        -1, &query, NULL) != SQLITE_OK) return 0;
    valid = sqlite3_bind_text(query, 1, actor, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_step(query) == SQLITE_ROW;
    sqlite3_finalize(query);
    return valid;
}

/* Every failure path names its line in the debug log before cleaning up. */
#define DESKTOP_FAIL() do { \
        wena_debug_log("failed at %s:%d", __FILE__, __LINE__); goto cleanup; \
    } while (0)

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
/* Android and iOS run the desktop as an SDL app: SDLActivity or SDL's UIKit
 * delegate calls main (SDL.h renames it SDL_main). There is no command line
 * from a launcher, no HOME or LANG to go by, and a touch screen. */
#if defined(__APPLE__) && TARGET_OS_IPHONE
#define DESKTOP_IOS 1
/* client/platform/ios/scene.m: shows SDL's window in the app's UIScene, and
 * names the part of it clear of the status bar, notch and home indicator. */
void wena_ios_scene_attach(SDL_Window *window);
int wena_ios_safe_area(int *left, int *top, int *right, int *bottom);
#else
#define DESKTOP_IOS 0
#endif
#if defined(__ANDROID__) || DESKTOP_IOS
#define DESKTOP_MOBILE 1
#else
#define DESKTOP_MOBILE 0
#endif
#if DESKTOP_MOBILE
#define DESKTOP_SYSTEM WENA_SYSTEM_MOBILE
#if defined(__ANDROID__)
/* Full screen: with a current target SDK, Android draws an app under its
 * status and navigation bars, which would hide the board's edges. */
#define DESKTOP_WINDOW_FLAGS SDL_WINDOW_FULLSCREEN
#else
/* Retina-sized drawing; the window itself stays in points. */
#define DESKTOP_WINDOW_FLAGS SDL_WINDOW_ALLOW_HIGHDPI
#endif
#elif defined(_WIN32)
#define DESKTOP_SYSTEM WENA_SYSTEM_WINDOWS
#define DESKTOP_HOME "APPDATA"
#elif defined(__APPLE__)
#define DESKTOP_SYSTEM WENA_SYSTEM_MACOS
#define DESKTOP_HOME "HOME"
#elif defined(__amigaos__) || defined(__AROS__)
#define DESKTOP_SYSTEM WENA_SYSTEM_AMIGA
#define DESKTOP_HOME "HOME"
#else
#define DESKTOP_SYSTEM WENA_SYSTEM_OTHER
#define DESKTOP_HOME "HOME"
#endif
/* AmigaOS and AROS start a program on the stack its icon or the Shell gives,
 * 4 KB to 40 KB by default; the desktop's --smoke run needs about 150 KB on
 * x86-64 Linux. A megabyte leaves room for deeper editors and the OS. */
#define DESKTOP_STACK 1048576UL
#if defined(__amigaos4__)
/* AmigaOS 4 reads the stack a program needs from this cookie. */
static const char desktop_stack_cookie[] __attribute__((used)) = "$STACK:1048576";
#elif defined(__amigaos__)
/* libnix moves main() to a stack of this size (its swapstack module, which
 * scripts/build_desktop_amiga_container.sh links). */
unsigned long __stack = DESKTOP_STACK;
#endif
#ifndef DESKTOP_WINDOW_FLAGS
#define DESKTOP_WINDOW_FLAGS 0
#endif
#define DESKTOP_DEFAULT_ACTOR "local-user"
#define DESKTOP_DEFAULT_BOARD "my-board"
#define DESKTOP_DEFAULT_TITLE "My board"

/* WeKan's files: the board the user sees first - theirs, by title - or, in
 * a file without one, a new board with WeKan's "Default" swimlane, as WeKan
 * makes a new board. */
static int desktop_wekan_board(sqlite3 *db, const char *actor, char *board, size_t capacity)
{
    sqlite3_stmt *query = NULL;
    const unsigned char *found;
    int ok = 0;
    if (sqlite3_prepare_v2(db, "SELECT b.id FROM boards b JOIN board_members m ON m.board_id = b.id "
        "AND m.actor_id = ?1 AND m.active ORDER BY b.title COLLATE NOCASE, b.id LIMIT 1", -1, &query, NULL) != SQLITE_OK)
        return 0;
    if (sqlite3_bind_text(query, 1, actor, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(query) == SQLITE_ROW &&
        (found = sqlite3_column_text(query, 0)) != NULL && strlen((const char *)found) < capacity) {
        strcpy(board, (const char *)found);
        ok = 1;
    }
    sqlite3_finalize(query);
    return ok;
}

static void desktop_meteor_id(char out[18])
{
    static const char alphabet[] = "23456789ABCDEFGHJKLMNPQRSTWXYZabcdefghijkmnopqrstuvwxyz";
    unsigned char random[17];
    int index;
    sqlite3_randomness(17, random);
    for (index = 0; index < 17; ++index) out[index] = alphabet[random[index] % (sizeof(alphabet) - 1)];
    out[17] = '\0';
}

static int desktop_wekan_starter(sqlite3 *db, const char *actor, const char *title, char *board, size_t capacity)
{
    char lane[18];
    sqlite3_stmt *query = NULL;
    int ok;
    if (capacity < 18) return 0;
    desktop_meteor_id(board);
    desktop_meteor_id(lane);
    ok = sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK &&
         sqlite3_prepare_v2(db, "INSERT INTO boards(id, title, version) VALUES (?1, ?2, 1)", -1, &query, NULL) == SQLITE_OK &&
         sqlite3_bind_text(query, 1, board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_bind_text(query, 2, title, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(query) == SQLITE_DONE;
    sqlite3_finalize(query); query = NULL;
    ok = ok && sqlite3_prepare_v2(db, "INSERT INTO swimlanes(id, board_id, title, position, version) "
        "VALUES (?1, ?2, 'Default', 0, 1)", -1, &query, NULL) == SQLITE_OK &&
         sqlite3_bind_text(query, 1, lane, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_bind_text(query, 2, board, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(query) == SQLITE_DONE;
    sqlite3_finalize(query);
    if (!ok) { sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); return 0; }
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) return 0;
    /* Written with its creator as its admin (a board without member rows is
     * exported so), then the membership Wena keeps: the export merges it with
     * the written member, which stays as it is. */
    if (wena_wekan_sync_export(db, actor) < 0) return 0;
    ok = sqlite3_prepare_v2(db, "INSERT INTO board_members(board_id, actor_id, active, version, created_at, "
        "updated_at) VALUES (?1, ?2, 1, 1, 0, 0)", -1, &query, NULL) == SQLITE_OK &&
         sqlite3_bind_text(query, 1, board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
         sqlite3_bind_text(query, 2, actor, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(query) == SQLITE_DONE;
    sqlite3_finalize(query);
    return ok;
}

static void desktop_usage(FILE *output)
{
    fputs("Usage: wena-desktop [--database ABS_PATH --actor ID --board ID]\n"
          "                    [--create [--title TITLE]] [--language LOCALE] [--smoke]\n"
          "       wena-desktop --dependency-info\n"
          "       wena-desktop --licenses\n"
          "       wena-desktop --help\n"
          "\n"
          "Without a workspace it opens board my-board as local-user in WENA_DATABASE or\n"
          "the user's data folder, creating it on the first run.\n",
          output);
    fputs("--create initializes a new local workspace without replacing files.\n"
          "Omit --create and --title to reopen it. The parent directory must exist.\n"
          "--smoke renders three frames with editor writes disabled.\n"
          "--screenshot FILE does the same and saves the last frame as a BMP image.\n"
          "--dependency-info reports linked libraries without opening a workspace.\n"
          "--licenses prints the licenses of everything compiled into this program.\n",
          output);
    fputs("--show STATE opens card:ID, card-menu:ID, list-menu:ID, add-card:LIST or sidebar\n"
          "(with --smoke or --screenshot), as WeKan's UI capture does.\n", output);
}

/* The frame drawn so far, read back from the renderer before it is shown. */
static int desktop_screenshot(SDL_Renderer *renderer, const char *path)
{
    SDL_Surface *surface;
    int width, height, saved;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) return 0;
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) return 0;
    saved = SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                 surface->pixels, surface->pitch) == 0 &&
            SDL_SaveBMP(surface, path) == 0;
    SDL_FreeSurface(surface);
    if (saved) wena_debug_log("screenshot %s (%dx%d)", path, width, height);
    return saved;
}

#include "platform/notices_data.h"

static int desktop_licenses(FILE *output)
{
    /* The terminating zero is the array's last byte, not part of the text. */
    return fwrite(wena_notices, 1, sizeof(wena_notices) - 1, output) == sizeof(wena_notices) - 1 &&
           fflush(output) == 0;
}

/* Where the default board lives: an environment variable on a desktop; on
 * Android and iOS the app's own data folder - Android's internal files
 * directory, iOS's Library/Application Support/wekan/wena - which is private,
 * writable and kept when the app is updated. */
static int desktop_home(char *out, size_t capacity)
{
#if DESKTOP_MOBILE
    char *path;
    int found;
    path = SDL_GetPrefPath("wekan", "wena");
    found = path != NULL && strlen(path) < capacity;
    if (found) strcpy(out, path);
    else if (capacity > 0) out[0] = '\0';
    SDL_free(path);
    return found;
#else
    return wena_environment(DESKTOP_HOME, out, capacity);
#endif
}

/* The user's language. A phone has no LANG and setlocale() answers "C", so
 * Android and iOS ask the system for its first preferred locale. */
static void desktop_locale(char *out, size_t capacity)
{
#if DESKTOP_MOBILE
    SDL_Locale *locales;
    char tag[64];
    int found;
    found = 0;
    locales = SDL_GetPreferredLocales();
    if (locales != NULL && locales[0].language != NULL &&
        strlen(locales[0].language) + 1 +
        (locales[0].country != NULL ? strlen(locales[0].country) : 0) < sizeof(tag)) {
        strcpy(tag, locales[0].language);
        if (locales[0].country != NULL && locales[0].country[0] != '\0') {
            strcat(tag, "-");
            strcat(tag, locales[0].country);
        }
        found = wena_locale_normalize(tag, out, capacity);
    }
    SDL_free(locales);
    if (found) return;
#endif
    (void)wena_locale_detect(out, capacity);
}

#if DESKTOP_MOBILE
/* Drawing on a phone: the board is laid out in density-independent units
 * (window size / input), drawn at the screen's own pixels (SDL_RenderSetScale
 * by the returned factor) with the font baked at that size, so text stays
 * sharp. input is how many window units one layout unit is: Android's density
 * (densityDpi / 160), 1 on iOS, whose window is in points already. */
static float desktop_mobile_scale(SDL_Window *window, SDL_Renderer *renderer, float *input)
{
    int window_width, window_height, output_width, output_height;
    float density;
    *input = 1.0f;
#if defined(__ANDROID__)
    {
        float dpi;
        int display;
        display = SDL_GetWindowDisplayIndex(window);
        if (display >= 0 && SDL_GetDisplayDPI(display, &dpi, NULL, NULL) == 0 && dpi > 160.0f)
            *input = dpi > 640.0f ? 4.0f : dpi / 160.0f;
    }
#endif
    density = 1.0f;
    SDL_GetWindowSize(window, &window_width, &window_height);
    if (window_width > 0 && SDL_GetRendererOutputSize(renderer, &output_width, &output_height) == 0 &&
        output_width > window_width)
        density = (float)output_width / (float)window_width;
    return density * *input;
}

/* Touches arrive as mouse events in window units; the board is in layout
 * units, from the corner of the safe area (left, top). */
static void desktop_mobile_event(SDL_Event *event, float input, int left, int top)
{
    if (event->type == SDL_MOUSEMOTION) {
        event->motion.x = (Sint32)((float)event->motion.x / input) - left;
        event->motion.y = (Sint32)((float)event->motion.y / input) - top;
        event->motion.xrel = (Sint32)((float)event->motion.xrel / input);
        event->motion.yrel = (Sint32)((float)event->motion.yrel / input);
    } else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        event->button.x = (Sint32)((float)event->button.x / input) - left;
        event->button.y = (Sint32)((float)event->button.y / input) - top;
    }
}

/* The on-screen keyboard covers half a phone: show it only while a text field
 * is being edited, instead of from the start as a desktop does. */
static int desktop_mobile_editing(const struct nk_context *context)
{
    const struct nk_window *window;
    for (window = context->begin; window != NULL; window = window->next)
        if (window->edit.active ||
            (window->popup.win != NULL && window->popup.win->edit.active)) return 1;
    return 0;
}
#endif

/* main below is the desktop's. On Android and iOS SDL_main, after it, reports
 * how it ended; on AROS main, after it, first moves it to a larger stack. */
#if DESKTOP_MOBILE || defined(__AROS__)
static int desktop_main(int argc, char **argv);
#define DESKTOP_MAIN desktop_main
#else
#define DESKTOP_MAIN main
#endif

int DESKTOP_MAIN(int argc, char **argv)
{
    const char *database_path, *actor_id, *board_id, *board_title, *requested_language;
    const char *const *languages;
    size_t language_count;
    int create_workspace, smoke, i, status, running, frames, width, height, sdl_started;
    const char *screenshot, *show;
    /* WeKan's files (wekan-files/db/wekan.sqlite), the default; else Wena's own file. */
    int wekan_mode;
    char wekan_actor[WENA_ID_CAPACITY], wekan_board[WENA_ID_CAPACITY];
    int synced_changes;
    char executable[WENA_EXECUTABLE_PATH_CAPACITY];
    char language_path[512], detected_locale[64];
    char collapse_path[WENA_EXECUTABLE_PATH_CAPACITY];
    char default_database[WENA_EXECUTABLE_PATH_CAPACITY];
    sqlite3 *database;
    WenaEmbeddedMigration migration;
    WenaLanguageState language;
    WenaLanguagePicker language_picker;
    WenaDesktopToolbar toolbar;
    WenaSqliteWorkspaceSeed seed;
    WenaSqliteBoardSnapshot *snapshot,*transfer_snapshot;
    WenaCardSelection *selection,*single_selection;
    WenaCardSelectionTraversal *selection_traversal;
    WenaBoardLayout layout;
    WenaBoardSidebar sidebar;
    WenaBoardCollapseState collapse;
    WenaBoardCollapseState observed_collapse;
    WenaBoardFilterState filter;
    WenaCardInteraction card_interaction;
    WenaCardMutation mutation;
    WenaCardDescriptionMutation description_mutation;
    WenaChecklistMutation checklist_mutation;
    WenaCardDestination checklist_destination;
    WenaSqliteDirectoryReader directory_reader;
    WenaBoardPresentation label_view;
    WenaDesktopChecklistPreview preview;
    WenaDesktopCardDetails card_details_view;
    WenaDesktopComposer composer;
    WenaDesktopSidebarData sidebar_data;
    int completion_result;
    const WenaCard *selected_card;
    WenaListInteraction list_interaction;
    WenaSwimlaneInteraction swimlane_interaction;
    WenaHierarchyMutation hierarchy_mutation;
    WenaHierarchyTransfer transfer;
    WenaHierarchyMoveMutation hierarchy_move_mutation;
    WenaDesktopEditors editors;
    WenaDesktopPanel opened_panel;
    SDL_Window *window;
    SDL_Cursor *resize_cursor, *arrow_cursor;
    int resize_cursor_shown;
    WenaSwimlaneResize swimlane_resize;
    SDL_Renderer *renderer;
    SDL_Event event;
    struct nk_context *context;
    struct nk_font_atlas *atlas;
    struct nk_font *font;
    float ui_scale, input_scale;
    int paused;
#if DESKTOP_MOBILE
    int inset_left, inset_top;
#endif
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        desktop_usage(stdout);
        return ferror(stdout) ? 1 : 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--licenses"))
        return desktop_licenses(stdout) ? 0 : 1;
    if (argc == 2 && !strcmp(argv[1], "--dependency-info"))
        return wena_desktop_dependency_report(stdout) ? 0 : 1;
#if DESKTOP_MOBILE
    /* Here executable holds the app's data folder: the log goes in there. */
    (void)(desktop_home(executable, sizeof(executable)) && wena_debug_log_open_data(executable));
#else
    (void)wena_debug_log_open(wena_executable_path_current(executable, sizeof(executable)) ? executable : NULL);
#endif
    wena_debug_log("wena-desktop starting, %d argument(s)", argc - 1);
    for (i = 1; i < argc; ++i) wena_debug_log("argument %d: %s", i, argv[i]);
    if (wena_debug_log_directory()[0] != '\0')
        fprintf(stderr, "Wena debug log: %s/desktop.log\n", wena_debug_log_directory());
    database_path = NULL; actor_id = NULL; board_id = NULL;
    board_title = NULL; requested_language = NULL;
    smoke = 0; create_workspace = 0; screenshot = NULL; show = NULL;
    wekan_mode = 0; wekan_actor[0] = wekan_board[0] = '\0'; synced_changes = 0;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--smoke") && !smoke) smoke = 1;
        /* A smoke run whose last frame is kept: UI comparisons with WeKan. */
        else if (!strcmp(argv[i], "--screenshot") && screenshot == NULL && i + 1 < argc) {
            screenshot = argv[++i]; smoke = 1;
        }
        /* The state to open first, as WeKan's captured ones: card:ID,
         * card-menu:ID, list-menu:ID, add-card:LIST or sidebar. Only with a
         * smoke run. */
        else if (!strcmp(argv[i], "--show") && show == NULL && i + 1 < argc) show = argv[++i];
        else if (!strcmp(argv[i], "--create") && !create_workspace) create_workspace = 1;
        else if (!strcmp(argv[i], "--title") && board_title == NULL && i + 1 < argc)
            board_title = argv[++i];
        else if (!strcmp(argv[i], "--language") && requested_language == NULL && i + 1 < argc)
            requested_language = argv[++i];
        else if (!strcmp(argv[i], "--database") && database_path == NULL && i + 1 < argc)
            database_path = argv[++i];
        else if (!strcmp(argv[i], "--actor") && actor_id == NULL && i + 1 < argc)
            actor_id = argv[++i];
        else if (!strcmp(argv[i], "--board") && board_id == NULL && i + 1 < argc)
            board_id = argv[++i];
        else {
            wena_debug_log("unknown or repeated argument: %s", argv[i]);
            desktop_usage(stderr);
            wena_debug_log_close();
            return 2;
        }
    }
    if (show != NULL && !smoke) {
        fputs("--show needs --smoke or --screenshot\n", stderr);
        wena_debug_log("refused: --show without --smoke or --screenshot");
        wena_debug_log_close();
        return 2;
    }
    /* No workspace named - double-clicked, opened from a file manager, or only
     * --smoke/--language given: the local board, created on the first run. */
    if (database_path == NULL && actor_id == NULL && !create_workspace && board_title == NULL) {
        char database_env[WENA_EXECUTABLE_PATH_CAPACITY], home[WENA_EXECUTABLE_PATH_CAPACITY];
        char xdg[WENA_EXECUTABLE_PATH_CAPACITY], writable[WENA_EXECUTABLE_PATH_CAPACITY];
        char executable[WENA_EXECUTABLE_PATH_CAPACITY], root[WENA_EXECUTABLE_PATH_CAPACITY];
        if (!wena_environment("WENA_DATABASE", database_env, sizeof(database_env))) {
            /* WeKan's files, as a WeKan FerretDB bundle keeps them:
             * WRITABLE_PATH, else wekan-files beside this program. */
            if (!wena_wekan_files_root(wena_environment("WRITABLE_PATH", writable, sizeof(writable)) ? writable : NULL,
                    wena_executable_path_current(executable, sizeof(executable)) ? executable : NULL,
                    desktop_home(home, sizeof(home)) ? home : NULL, DESKTOP_SYSTEM, root, sizeof(root)) ||
                !wena_wekan_files_prepare(root, DESKTOP_SYSTEM) ||
                !wena_wekan_files_database(root, DESKTOP_SYSTEM, default_database, sizeof(default_database))) {
                wena_debug_log("no wekan-files folder: set WRITABLE_PATH to an absolute path");
                fputs("Cannot make the wekan-files folder; set WRITABLE_PATH to an absolute path\n", stderr);
                wena_debug_log_close();
                return 2;
            }
            database_path = default_database;
            wekan_mode = 1;
            wena_debug_log("WeKan's files in %s, database %s", root, database_path);
        } else if (board_id == NULL) {
            if (!wena_desktop_default_database(database_env,
                    desktop_home(home, sizeof(home)) ? home : NULL,
                    wena_environment("XDG_DATA_HOME", xdg, sizeof(xdg)) ? xdg : NULL,
                    DESKTOP_SYSTEM, default_database, sizeof(default_database)) ||
                !wena_make_parent_directories(default_database)) {
                wena_debug_log("no default board file: set WENA_DATABASE to an absolute path");
                fputs("Cannot name a folder for the local board; set WENA_DATABASE to an absolute path\n", stderr);
                wena_debug_log_close();
                return 2;
            }
            database_path = default_database;
            actor_id = DESKTOP_DEFAULT_ACTOR; board_id = DESKTOP_DEFAULT_BOARD;
            if (wena_file_kind(default_database) == WENA_FILE_MISSING) {
                create_workspace = 1; board_title = DESKTOP_DEFAULT_TITLE;
            }
            wena_debug_log("default board %s in %s%s", board_id, database_path,
                           create_workspace ? " (creating it)" : "");
        }
    }
    if (!wekan_mode && (database_path == NULL || !wena_path_absolute(database_path) ||
        strlen(database_path) >= WENA_EXECUTABLE_PATH_CAPACITY ||
        !wena_model_identifier_valid(actor_id) || !wena_model_identifier_valid(board_id) ||
        (board_title != NULL && !create_workspace) ||
        (!create_workspace && wena_file_kind(database_path) != WENA_FILE_REGULAR))) {
        fputs("An absolute database path, actor and board are required; use --create for a new workspace\n", stderr);
        wena_debug_log("refused: an absolute database path, actor and board are required");
        wena_debug_log_close();
        return 2;
    }
    memset(&migration, 0, sizeof(migration));
    snapshot = (WenaSqliteBoardSnapshot *)calloc(1, sizeof(*snapshot));
    transfer_snapshot=(WenaSqliteBoardSnapshot*)calloc(1,sizeof(*transfer_snapshot));
    if(!snapshot||!transfer_snapshot){free(snapshot);free(transfer_snapshot);return 1;}
    memset(&layout, 0, sizeof(layout));
    memset(&preview, 0, sizeof(preview));
    memset(&editors, 0, sizeof(editors));
    memset(&card_interaction, 0, sizeof(card_interaction));
    memset(&list_interaction, 0, sizeof(list_interaction));
    memset(&swimlane_interaction, 0, sizeof(swimlane_interaction));
    memset(&toolbar, 0, sizeof(toolbar));
    memset(&label_view, 0, sizeof(label_view));
    resize_cursor = NULL; arrow_cursor = NULL; resize_cursor_shown = 0;
    memset(&swimlane_resize, 0, sizeof(swimlane_resize));
    database = NULL; window = NULL; renderer = NULL; context = NULL;single_selection=NULL;selection=NULL;selection_traversal=NULL;
    sdl_started = 0; status = 1;
    /* Compiled in: the desktop reads nothing from its own file, which an app
     * bundle, an APK or an Amiga PROGDIR: does not let it find reliably. */
    if (!wena_sqlite_compiled_bundle(&migration.bytes, &migration.length, migration.sha256))
        DESKTOP_FAIL();
    languages = wena_ui_catalog_languages(&language_count);
    detected_locale[0] = '\0';
    desktop_locale(detected_locale, sizeof(detected_locale));
    language_path[0] = '\0';
    /* Leave room for both ".language" and the settings writer's ".tmp". */
    if (strlen(database_path) + 14 < sizeof(language_path)) {
        strcpy(language_path, database_path);
        strcat(language_path, ".language");
    }
    if (!wena_language_init(&language, requested_language != NULL ? NULL : language_path,
        requested_language != NULL ? requested_language : detected_locale,
        languages, language_count)) DESKTOP_FAIL();
    wena_ui_set_translator(wena_ui_catalog_translate, &language);
    if (wekan_mode) {
        char wanted[WENA_ID_CAPACITY + 64], salt[33];
        unsigned char random[16];
        int index;
        /* Wena's tables in memory over WeKan's file, read in; what Wena
         * changes is written back after each frame that changed something. */
        if (!wena_sqlite_open(":memory:", migration.bytes, migration.length, migration.sha256, &database) ||
            !wena_wekan_sync_attach(database, database_path) ||
            !wena_wekan_sync_user(database, wena_environment("WENA_USER", wanted, sizeof(wanted)) ? wanted : NULL,
                                  wekan_actor, sizeof(wekan_actor)) ||
            !wena_wekan_sync_import(database)) {
            wena_debug_log("WeKan's database %s: %s", database_path, wena_wekan_sync_error());
            DESKTOP_FAIL();
        }
        actor_id = wekan_actor;
        if (board_id == NULL) {
            if (!desktop_wekan_board(database, actor_id, wekan_board, sizeof(wekan_board))) {
                if (!desktop_wekan_starter(database, actor_id, DESKTOP_DEFAULT_TITLE, wekan_board, sizeof(wekan_board)) ||
                    wena_wekan_sync_export(database, actor_id) < 0) {
                    wena_debug_log("new board in %s: %s", database_path, wena_wekan_sync_error());
                    DESKTOP_FAIL();
                }
                wena_debug_log("made board %s for %s", wekan_board, actor_id);
            }
            board_id = wekan_board;
        }
        sqlite3_randomness(16, random);
        for (index = 0; index < 16; ++index) sprintf(salt + index * 2, "%02x", random[index]);
        wena_mutation_identity_salt(salt);
        synced_changes = sqlite3_total_changes(database);
        if (!actor_exists(database, actor_id) || !wena_sqlite_board_load(database, board_id, snapshot)) {
            wena_debug_log("board %s for %s is not in %s", board_id, actor_id, database_path);
            DESKTOP_FAIL();
        }
        wena_debug_log("user %s, board %s", actor_id, board_id);
    } else {
        if (create_workspace) {
            seed.actor_id = actor_id; seed.actor_name = actor_id;
            seed.board_id = board_id;
            seed.board_title = board_title == NULL ? board_id : board_title;
            seed.swimlane_id = "default-lane";
            seed.swimlane_title = wena_ui_text(WENA_UI_TEXT_SWIMLANE);
            seed.list_id = "default-list";
            seed.list_title = wena_ui_text(WENA_UI_TEXT_LIST);
            if (!wena_sqlite_workspace_create(database_path, migration.bytes,
                migration.length, migration.sha256, &seed)) DESKTOP_FAIL();
        }
        /* Query-only scope preflight prevents invalid actor/board launches from
         * creating a file, a schema or any domain records. Not SQLITE_OPEN_READONLY:
         * a read-only handle cannot read a WAL database whose -wal file a clean
         * exit removed ("unable to open database file"), so every launch after a
         * normal quit failed. Without SQLITE_OPEN_CREATE a missing file still
         * fails, and query_only refuses every write. */
        if (sqlite3_open_v2(database_path, &database, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
            DESKTOP_FAIL();
        if (!wena_sqlite_connection_harden(database) ||
            sqlite3_exec(database, "PRAGMA query_only=ON", NULL, NULL, NULL) != SQLITE_OK) DESKTOP_FAIL();
        if (!actor_exists(database, actor_id)) {
            wena_debug_log("actor %s is not in %s: %s", actor_id, database_path, sqlite3_errmsg(database));
            DESKTOP_FAIL();
        }
        if (!wena_sqlite_board_load(database, board_id, snapshot)) {
            wena_debug_log("board %s is not in %s", board_id, database_path);
            DESKTOP_FAIL();
        }
        if (sqlite3_close(database) != SQLITE_OK) DESKTOP_FAIL();
        database = NULL;
        if (!wena_sqlite_open(database_path, migration.bytes, migration.length,
                              migration.sha256, &database) ||
            !actor_exists(database, actor_id) ||
            !wena_sqlite_board_load(database, board_id, snapshot)) DESKTOP_FAIL();
    }
    if (requested_language != NULL && !smoke && language_path[0] != '\0' &&
        !wena_language_set(&language, language_path, requested_language,
            languages, language_count)) DESKTOP_FAIL();
    if (!wena_language_picker_init(&language_picker, &language, language_path,
        smoke || language_path[0] == '\0')) DESKTOP_FAIL();
    single_selection=(WenaCardSelection*)calloc(1,sizeof(*single_selection));
    if(!single_selection)DESKTOP_FAIL();
    wena_card_selection_panel_init(&editors.card_transfer,single_selection);
    selection=(WenaCardSelection*)malloc(sizeof(*selection));
    selection_traversal=(WenaCardSelectionTraversal*)calloc(1,sizeof(*selection_traversal));
    if(!selection_traversal||!selection||!wena_card_selection_init(selection,board_id))DESKTOP_FAIL();
    wena_card_selection_panel_init(&editors.selection,selection);
    layout.board = &snapshot->board;
    layout.swimlanes = snapshot->swimlanes; layout.swimlane_count = snapshot->swimlane_count;
    layout.lists = snapshot->lists; layout.list_count = snapshot->list_count;
    layout.cards = snapshot->cards; layout.card_count = snapshot->card_count;
    wena_board_sidebar_init(&sidebar);
    wena_board_collapse_init(&collapse);
    collapse_path[0] = '\0';
    if (wena_collapse_preferences_path(database_path, actor_id, board_id,
                                      collapse_path, sizeof(collapse_path))) {
        toolbar.collapse_error = wena_collapse_preferences_load(collapse_path,
            database_path, actor_id, board_id, &collapse) == WENA_COLLAPSE_PREFS_ERROR;
        toolbar.collapse_writable = !smoke;
    } else toolbar.collapse_error = 1;
    /* Prune only against the complete validated board snapshot. Loading and
     * pruning never rewrite preferences; only explicit interaction does. */
    if (!wena_board_collapse_sync(&collapse, &layout)) DESKTOP_FAIL();
    observed_collapse = collapse;
    wena_board_filter_init(&filter);
    if (!wena_board_filter_sync(&filter, snapshot->board.id)) DESKTOP_FAIL();
    wena_card_details_init(&editors.details);
    layout.sidebar = &sidebar; layout.collapse = &collapse;
    layout.swimlane_resize = &swimlane_resize;
    layout.swimlane_resize_bar = wena_swimlane_resize_bar;
    layout.sidebar_as_window = 1;
    layout.card_interaction = &card_interaction;
    layout.list_interaction = &list_interaction;
    toolbar.language = &language_picker;
    toolbar.filter = &filter;
    toolbar.labels = &label_view;
    toolbar.preview = &preview;
    preview.view = &label_view;
    preview.layout = &layout;
    composer.editors = &editors;
    composer.mutation = &mutation;
    composer.layout = &layout;
    composer.card_count = &snapshot->card_count;
    layout.card_composer = desktop_card_composer;
    layout.card_composer_context = &composer;
    memset(&sidebar_data, 0, sizeof(sidebar_data));
    memset(&card_details_view, 0, sizeof(card_details_view));
    card_details_view.preview = &preview;
    card_details_view.descriptions = &description_mutation;
    editors.details.view.body = desktop_card_details_section;
    editors.details.view.body_context = &card_details_view;
    preview.readonly = smoke;
    layout.card_contents = desktop_card_contents;
    layout.card_contents_context = &preview;
    layout.card_collapsed = desktop_card_collapsed;
    layout.card_collapsed_context = &preview;
    layout.card_drag_handle = desktop_card_drag;
    layout.card_drag_context = &preview;
    layout.list_drag_handle = desktop_list_drag;
    layout.swimlane_drag_handle = desktop_swimlane_drag;
    layout.hierarchy_drag_context = &preview;
    layout.card_drop_target = desktop_card_drop;
    layout.card_drop_context = &preview;
    layout.card_badges = desktop_card_badges;
    layout.card_badges_context = &label_view;
    layout.card_visible = wena_board_filter_matches;
    layout.card_visible_context = &filter;
    layout.toolbar = desktop_toolbar;
    layout.toolbar_context = &toolbar;
    layout.swimlane_interaction = &swimlane_interaction;
    layout.header_actor = actor_id;
    layout.header_actions = &toolbar.header_actions;
    layout.card_drag_area = desktop_card_drag_area;
    layout.list_drag_area = desktop_list_drag_area;
    layout.swimlane_drag_area = desktop_swimlane_drag_area;
    layout.card_drop_area = desktop_card_drop_area;
    wena_card_create_init(&editors.create, NULL, NULL);
    wena_card_move_init(&editors.move, NULL, NULL, NULL);
    wena_card_archives_init(&editors.archives, NULL, NULL, NULL);
    wena_card_description_init(&editors.description, NULL, NULL, NULL);
    wena_checklists_init(&editors.checklists, NULL, NULL, NULL);
    wena_labels_init(&editors.labels, NULL, NULL, NULL);
    wena_board_settings_init(&editors.board_settings, NULL, NULL, NULL);
    wena_hierarchy_title_init(&editors.hierarchy);
    wena_hierarchy_move_init(&editors.hierarchy_move, NULL, NULL, NULL);
    if (!wena_card_mutation_init(&mutation, database, actor_id, board_id,
                                 snapshot->cards, snapshot->card_count)) DESKTOP_FAIL();
    if (!wena_card_description_mutation_init(&description_mutation, database,
                                             actor_id, board_id)) DESKTOP_FAIL();
    wena_card_description_init(&editors.description,
        wena_card_description_mutation_load,
        smoke ? NULL : wena_card_description_mutation_save,
        &description_mutation);
    if (!wena_checklist_mutation_init(&checklist_mutation, database,
                                      actor_id, board_id)) DESKTOP_FAIL();
    wena_checklists_init(&editors.checklists, wena_checklist_mutation_load,
        smoke ? NULL : wena_checklist_mutation_save, &checklist_mutation);
    if (!wena_sqlite_directory_reader_init(&directory_reader,database,actor_id) ||
        !wena_card_destination_init(&checklist_destination,4,wena_sqlite_directory_read,&directory_reader))
        DESKTOP_FAIL();
    editors.checklists.destination = &checklist_destination;
    editors.checklists.load_destination = wena_checklist_mutation_load_destination;
    editors.checklists.destination_context = &checklist_mutation;
    editors.checklists.sections = &preview.sections;
    if (!wena_board_presentation_init(&label_view, database, actor_id, board_id) ||
        !label_view.valid) DESKTOP_FAIL();
    wena_labels_init(&editors.labels, wena_board_presentation_labels_load,
        smoke ? NULL : wena_board_presentation_labels_save, &label_view);
    wena_board_settings_init(&editors.board_settings, wena_board_presentation_settings_load,
        smoke ? NULL : wena_board_presentation_settings_save_display, &label_view);
    editors.board_settings.save_all = smoke ? NULL : wena_board_presentation_settings_save_all;
    if (!smoke) {
        if (!wena_hierarchy_mutation_init(&hierarchy_mutation, database,
            actor_id, board_id, snapshot)) DESKTOP_FAIL();
        if (!wena_hierarchy_move_mutation_init(&hierarchy_move_mutation,
            database, actor_id, board_id, snapshot)) DESKTOP_FAIL();
        wena_hierarchy_drag_init(&preview.hierarchy_drag,
            wena_hierarchy_move_mutation_load,wena_hierarchy_move_mutation_move,&hierarchy_move_mutation);
        wena_hierarchy_move_init(&editors.hierarchy_move,
            wena_hierarchy_move_mutation_load, wena_hierarchy_move_mutation_move,
            &hierarchy_move_mutation);
        wena_hierarchy_title_set_adapter(&editors.hierarchy,
            wena_hierarchy_mutation_load, wena_hierarchy_mutation_save,
            &hierarchy_mutation);
        wena_hierarchy_title_set_create_adapter(&editors.hierarchy,
            wena_hierarchy_mutation_create);
        wena_hierarchy_title_set_archive_adapter(&editors.hierarchy,wena_hierarchy_mutation_archive);
        wena_hierarchy_title_set_archive_provider(&editors.hierarchy,WENA_HIERARCHY_SWIMLANE,
            wena_hierarchy_mutation_swimlane_archive);
        wena_hierarchy_title_set_color_adapters(&editors.hierarchy,wena_hierarchy_mutation_color_load,wena_hierarchy_mutation_color_save);
        wena_card_selection_panel_set_archive(&editors.selection,
            wena_hierarchy_mutation_selected_cards_load,wena_hierarchy_mutation_selected_cards_archive,&hierarchy_mutation);
        wena_card_selection_panel_set_labels(&editors.selection,wena_board_presentation_selected_labels_load,
            wena_board_presentation_selected_labels_save,&label_view);
        wena_card_selection_panel_set_move(&editors.selection,wena_hierarchy_mutation_selected_move_load,
            wena_hierarchy_mutation_selected_move,&hierarchy_mutation);
        if(!wena_hierarchy_transfer_init(&transfer,&hierarchy_mutation,transfer_snapshot)||
            !wena_card_selection_panel_set_transfer(&editors.selection,wena_hierarchy_transfer_load,wena_hierarchy_transfer_save,
                wena_hierarchy_transfer_view,&transfer,wena_sqlite_directory_read,&directory_reader))DESKTOP_FAIL();
        wena_card_selection_panel_set_move(&editors.card_transfer,wena_hierarchy_mutation_selected_move_load,
            wena_hierarchy_mutation_selected_move,&hierarchy_mutation);
        if(!wena_card_selection_panel_set_transfer(&editors.card_transfer,wena_hierarchy_transfer_load,
            wena_hierarchy_transfer_save,wena_hierarchy_transfer_view,&transfer,
            wena_sqlite_directory_read,&directory_reader))DESKTOP_FAIL();
        editors.hierarchy.selection_enabled=1;
        layout.card_selected=wena_card_selection_selected;layout.card_selected_context=selection;
        layout.card_selection=wena_card_selection_traversal_control;layout.card_selection_context=selection_traversal;
        wena_hierarchy_title_set_list_cards_adapters(&editors.hierarchy,
            wena_hierarchy_mutation_list_cards_load,wena_hierarchy_mutation_list_cards_archive);
        wena_hierarchy_title_set_wip_adapters(&editors.hierarchy,wena_hierarchy_mutation_wip_load,wena_hierarchy_mutation_wip_save);
        if (!wena_card_mutation_set_create_cache(&mutation, &snapshot->card_count,
            WENA_SQLITE_BOARD_MAX_CARDS)) DESKTOP_FAIL();
        wena_card_create_init(&editors.create, wena_card_mutation_create, &mutation);
        editors.create.to_top = desktop_card_to_top;
        editors.create.to_top_context = &composer;
        wena_card_move_init(&editors.move, wena_card_mutation_load,
                            wena_card_mutation_move, &mutation);
        wena_card_move_set_reorder_adapter(&editors.move,
                                           wena_card_mutation_reorder);
        wena_card_move_set_insert_adapter(&editors.move,wena_card_mutation_insert);
        editors.move.boards_enabled=1;
        wena_card_archives_init(&editors.archives, wena_card_mutation_load_archived,
                                wena_card_mutation_restore, &mutation);
        wena_card_archives_set_lists(&editors.archives,wena_hierarchy_mutation_archive_load,
            wena_hierarchy_mutation_restore,&hierarchy_mutation);
        hierarchy_mutation.published_card_count=&mutation.card_count;
        wena_card_archives_set_provider(&editors.archives,WENA_ARCHIVE_SWIMLANES,
            wena_hierarchy_mutation_swimlane_archive_load,
            wena_hierarchy_mutation_swimlane_restore,&hierarchy_mutation);
        wena_card_details_set_title_adapter(&editors.details, wena_card_mutation_load,
                                            wena_card_mutation_save, &mutation);
        wena_card_details_set_archive_adapter(&editors.details, wena_card_mutation_archive);
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) DESKTOP_FAIL();
    sdl_started = 1;
    window = SDL_CreateWindow("WeKan Native", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 1024, 720, SDL_WINDOW_RESIZABLE | DESKTOP_WINDOW_FLAGS |
        (smoke && !DESKTOP_MOBILE ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
    if (window == NULL) DESKTOP_FAIL();
#if DESKTOP_IOS
    wena_ios_scene_attach(window);
#endif
    ui_scale = 1.0f; input_scale = 1.0f; paused = 0;
#if DESKTOP_MOBILE
    inset_left = 0; inset_top = 0;
    /* The GPU where there is one (OpenGL ES, Metal): a phone's software
     * framebuffer is itself a texture on it. */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL) DESKTOP_FAIL();
    ui_scale = desktop_mobile_scale(window, renderer, &input_scale);
    if (ui_scale != 1.0f && SDL_RenderSetScale(renderer, ui_scale, ui_scale) != 0) DESKTOP_FAIL();
    wena_debug_log("display scale %.2f, touch scale %.2f", ui_scale, input_scale);
#else
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL) DESKTOP_FAIL();
#endif
    context = nk_sdl_init(window, renderer);
    if (context == NULL) DESKTOP_FAIL();
    wena_sdl_install_clipboard(context);
    nk_sdl_font_stash_begin(&atlas);
    /* Baked at the screen's pixels, measured in layout units (ui_scale is 1
     * on a desktop). */
    font = wena_native_font_add(atlas, 14.0f * ui_scale);
    if (font == NULL) font = nk_font_atlas_add_default(atlas, 14.0f * ui_scale, NULL);
    if (font == NULL) DESKTOP_FAIL();
    /* WeKan's text: Roboto 14px, bold titles and menu items, 12-13px header
     * links and minicards, 16 and 19px bold in card details. */
    {
        static const struct { WenaWekanFont which; float size; int bold; } faces[] = {
            {WENA_WEKAN_FONT_BOLD, 14.0f, 1}, {WENA_WEKAN_FONT_SMALL, 12.0f, 0},
            {WENA_WEKAN_FONT_LINK, 13.0f, 0}, {WENA_WEKAN_FONT_SECTION, 16.0f, 1},
            {WENA_WEKAN_FONT_TITLE, 19.0f, 1}};
        struct nk_font *baked[sizeof(faces) / sizeof(faces[0])];
        size_t face;
        for (face = 0; face < sizeof(faces) / sizeof(faces[0]); ++face)
            baked[face] = faces[face].bold ? wena_native_font_add_bold(atlas, faces[face].size * ui_scale) :
                                             wena_native_font_add(atlas, faces[face].size * ui_scale);
        nk_sdl_font_stash_end();
        for (face = 0; face < sizeof(faces) / sizeof(faces[0]); ++face) {
            if (baked[face] == NULL) continue;
            if (ui_scale != 1.0f) baked[face]->handle.height = faces[face].size;
            wena_wekan_font_set(faces[face].which, &baked[face]->handle);
        }
    }
    if (ui_scale != 1.0f) font->handle.height = 14.0f;
    wena_wekan_font_set(WENA_WEKAN_FONT_BODY, &font->handle);
    nk_style_set_font(context, &font->handle);
    if (!wena_native_theme_apply(context)) DESKTOP_FAIL();
#if !DESKTOP_MOBILE
    SDL_StartTextInput();
#endif
    /* Up-down arrows over the bar between swimlanes; optional decoration. */
    resize_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    arrow_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    wena_debug_log("window open, board %s loaded%s", board_id, smoke ? " (smoke)" : "");
    running = 1; frames = 0;
    while (running) {
        nk_input_begin(context);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = 0; wena_debug_log("window closed"); }
#if DESKTOP_MOBILE
            /* iOS ends an app that draws in the background. */
            if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERBACKGROUND)
                paused = 1;
            else if (event.type == SDL_APP_DIDENTERFOREGROUND) paused = 0;
            desktop_mobile_event(&event, input_scale, inset_left, inset_top);
#endif
            if (!smoke) (void)wena_sdl_handle_event(context, &event);
        }
        nk_input_end(context);
        if (show != NULL && frames == 0) {
            const WenaCard *shown;
            const char *id = strchr(show, ':') != NULL ? strchr(show, ':') + 1 : "";
            if (!strncmp(show, "card:", 5) && (shown = desktop_selected_card(snapshot, id)) != NULL)
                (void)wena_card_details_open(&editors.details, shown);
            else if (!strncmp(show, "card-menu:", 10) && desktop_selected_card(snapshot, id) != NULL &&
                     strlen(id) < sizeof(toolbar.menu.card_id)) {
                toolbar.menu.kind = DESKTOP_MENU_CARD;
                strcpy(toolbar.menu.card_id, id);
            } else if (!strncmp(show, "list-menu:", 10) && snapshot->swimlane_count > 0 &&
                       strlen(id) < sizeof(toolbar.menu.list_id)) {
                toolbar.menu.kind = DESKTOP_MENU_LIST;
                strcpy(toolbar.menu.list_id, id);
                strcpy(toolbar.menu.swimlane_id, snapshot->swimlanes[0].id);
            } else if (!strncmp(show, "add-card:", 9) && snapshot->swimlane_count > 0 &&
                       strlen(id) < sizeof(list_interaction.list_id)) {
                WenaListInteraction add;
                memset(&add, 0, sizeof(add));
                add.actions = WENA_LIST_HEADER_ADD_CARD;
                strcpy(add.board_id, snapshot->board.id);
                strcpy(add.list_id, id);
                strcpy(add.swimlane_id, snapshot->swimlanes[0].id);
                (void)wena_card_create_open(&editors.create, &layout, &add);
            } else if (!strcmp(show, "sidebar")) sidebar.visible = 1;
            else wena_debug_log("--show %s: nothing to show", show);
        }
        SDL_GetWindowSize(window, &width, &height);
        if (input_scale != 1.0f) {
            width = (int)((float)width / input_scale);
            height = (int)((float)height / input_scale);
        }
#if DESKTOP_IOS
        {
            /* The board inside the safe area; the background fills the rest. */
            int inset_right, inset_bottom;
            SDL_Rect area;
            if (wena_ios_safe_area(&inset_left, &inset_top, &inset_right, &inset_bottom) &&
                width > inset_left + inset_right && height > inset_top + inset_bottom) {
                area.x = inset_left; area.y = inset_top;
                area.w = width - inset_left - inset_right;
                area.h = height - inset_top - inset_bottom;
                if (SDL_RenderSetViewport(renderer, &area) != 0) DESKTOP_FAIL();
                width = area.w; height = area.h;
            } else {
                inset_left = 0; inset_top = 0;
                if (SDL_RenderSetViewport(renderer, NULL) != 0) DESKTOP_FAIL();
            }
        }
#endif
        if (width > 0 && height > 0 && !paused) {
            opened_panel = DESKTOP_PANEL_NONE;
            if(selection->count)(void)wena_card_selection_sync(selection,snapshot->cards,snapshot->card_count);
            if (label_view.summary_valid)
                wena_checklist_inline_sync(&preview.inline_edit, label_view.contents, label_view.settings.show_checklists);
            {
                const WenaChecklistCardSummary *source;
                source = label_view.summary_valid ? wena_checklist_summary_find(
                    &label_view.contents->summary, preview.card_drag.gesture.source_id) : NULL;
                wena_card_drag_begin(context, &preview.card_drag, snapshot->cards,
                    snapshot->card_count, source && !source->archived ? source->card_version : 0);
            }
            wena_hierarchy_drag_begin(context,&preview.hierarchy_drag,&layout);
            wena_reorder_drag_begin(context, &preview.drag.gesture);
            preview.intent.pending = 0;
            preview.sections.pending = 0;
            preview.sections.snapshot = label_view.sections_valid ? label_view.sections : NULL;
            preview.sections.readonly = smoke || !label_view.sections_valid;
            layout.card_count = snapshot->card_count;
            layout.list_count = snapshot->list_count;
            layout.swimlane_count = snapshot->swimlane_count;
            wena_card_selection_traversal_begin(selection_traversal,selection);
            layout.header_filter_active = filter.query[0] != '\0';
            wena_ui_controls_begin();
            desktop_sidebar_fill(&sidebar_data, &sidebar, &label_view, database, actor_id,
                                 snapshot->board.id);
            if (!wena_board_feature_render_with_state(context, &layout,
                (float)width, (float)height, &editors.details)) DESKTOP_FAIL();
            {
                WenaDesktopPanel menu_panel;
                const char *dragged;
                menu_panel = desktop_menus(context, &toolbar, &editors, &layout, (float)width, (float)height);
                if (menu_panel != DESKTOP_PANEL_NONE) opened_panel = menu_panel;
                /* WeKan's header: the title renames, Filter opens its panel,
                 * the user's name its menu. */
                if ((toolbar.header_actions & WENA_BOARD_HEADER_RENAME) != 0u)
                    toolbar.actions |= DESKTOP_RENAME_BOARD;
                if ((toolbar.header_actions & WENA_BOARD_HEADER_FILTER) != 0u)
                    toolbar.filter_visible = !toolbar.filter_visible;
                if ((toolbar.header_actions & WENA_BOARD_HEADER_MEMBER_MENU) != 0u)
                    toolbar.menu.kind = DESKTOP_MENU_MEMBER;
                if ((list_interaction.actions & WENA_LIST_HEADER_OPEN_MENU) != 0u) {
                    toolbar.menu.kind = DESKTOP_MENU_LIST;
                    strcpy(toolbar.menu.list_id, list_interaction.list_id);
                    strcpy(toolbar.menu.swimlane_id, list_interaction.swimlane_id);
                }
                if ((list_interaction.actions & WENA_LIST_HEADER_ADD_LIST) != 0u)
                    toolbar.actions |= DESKTOP_ADD_LIST;
                if ((card_interaction.actions & WENA_CARD_BODY_OPEN_MENU) != 0u) {
                    toolbar.menu.kind = DESKTOP_MENU_CARD;
                    strcpy(toolbar.menu.card_id, card_interaction.card_id);
                }
                /* Card Actions in the card details' header: the same popup. */
                if ((editors.details.interaction.actions & WENA_CARD_DETAILS_OPEN_MENU) != 0u) {
                    toolbar.menu.kind = DESKTOP_MENU_CARD;
                    strcpy(toolbar.menu.card_id, editors.details.interaction.card_id);
                }
                if ((swimlane_interaction.actions & WENA_SWIMLANE_OPEN_MENU) != 0u) {
                    toolbar.menu.kind = DESKTOP_MENU_SWIMLANE;
                    toolbar.menu.list_id[0] = '\0';
                    strcpy(toolbar.menu.swimlane_id, swimlane_interaction.swimlane_id);
                }
                if ((swimlane_interaction.actions & WENA_SWIMLANE_ADD) != 0u)
                    toolbar.actions |= DESKTOP_ADD_SWIMLANE;
                /* The card being dragged follows the pointer, beside it so the
                 * place it is dropped stays under the pointer. */
                dragged = wena_card_drag_source(&preview.card_drag);
                selected_card = dragged ? desktop_selected_card(snapshot, dragged) : NULL;
                if (selected_card != NULL) {
                    nk_style_push_style_item(context, &context->style.window.fixed_background,
                                             nk_style_item_color(nk_rgb(0xff, 0xff, 0xff)));
                    if (nk_begin(context, "Wena dragged card",
                            nk_rect(context->input.mouse.pos.x + 14.0f, context->input.mouse.pos.y + 14.0f, 238.0f, 40.0f),
                            NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT | NK_WINDOW_BORDER)) {
                        nk_layout_row_dynamic(context, 24.0f, 1);
                        wena_wekan_text(context, selected_card->title, WENA_WEKAN_FONT_SMALL,
                                        WENA_WEKAN_MINICARD_TEXT, NK_TEXT_LEFT);
                    }
                    nk_end(context);
                    nk_style_pop_style_item(context);
                }
            }
            if (resize_cursor != NULL && arrow_cursor != NULL &&
                (swimlane_resize.hovered || swimlane_resize.active) != resize_cursor_shown) {
                resize_cursor_shown = swimlane_resize.hovered || swimlane_resize.active;
                SDL_SetCursor(resize_cursor_shown ? resize_cursor : arrow_cursor);
            }
            wena_reorder_drag_end(context, &preview.drag.gesture);
            wena_card_drag_end(context, &preview.card_drag);
            wena_hierarchy_drag_end(context,&preview.hierarchy_drag);
            if (preview.intent.pending || preview.inline_edit.action || preview.drag.gesture.active || preview.drag.gesture.pending ||
                preview.card_drag.gesture.active || preview.card_drag.gesture.pending ||
                preview.hierarchy_drag.gesture.active || preview.hierarchy_drag.gesture.pending) {
                desktop_close_other_editors(&editors, DESKTOP_PANEL_NONE);
                sidebar.visible = 0;
            }
            if (toolbar.filter_changed) {
                wena_checklist_inline_cancel(&preview.inline_edit);
                wena_reorder_drag_cancel(&preview.drag.gesture);
                wena_card_drag_cancel(&preview.card_drag);
                wena_hierarchy_drag_cancel(&preview.hierarchy_drag);
                desktop_close_other_editors(&editors, DESKTOP_PANEL_NONE);
                sidebar.visible = 0;
            }
            /* An explicit board action takes focus from the sidebar. Keeping
             * the old sidebar visible would close the newly opened editor
             * again below during this same frame. */
            if (list_interaction.actions != 0u ||
                card_interaction.actions != 0u || toolbar.actions != 0u ||
                swimlane_interaction.actions != 0u ||
                (editors.details.interaction.actions & (WENA_CARD_DETAILS_MOVE |
                    WENA_CARD_DETAILS_DESCRIPTION | WENA_CARD_DETAILS_CHECKLISTS |
                    WENA_CARD_DETAILS_LABELS)) != 0u)
                sidebar.visible = 0;
            if(card_interaction.actions&(WENA_CARD_BODY_TOGGLE_SELECTION|WENA_CARD_BODY_RANGE_SELECTION))
                (void)wena_card_selection_traversal_apply(selection_traversal,snapshot->cards,snapshot->card_count,
                    card_interaction.card_id,card_interaction.actions&(WENA_CARD_BODY_TOGGLE_SELECTION|WENA_CARD_BODY_RANGE_SELECTION));
            if ((list_interaction.actions & WENA_LIST_HEADER_ADD_CARD) != 0u) {
                desktop_close_other_editors(&editors, DESKTOP_PANEL_CREATE_CARD);
                if (wena_card_create_open(&editors.create, &layout, &list_interaction))
                    opened_panel = DESKTOP_PANEL_CREATE_CARD;
            }
            if ((card_interaction.actions & (WENA_CARD_BODY_OPEN_DETAILS |
                                             WENA_CARD_BODY_OPEN_MENU)) != 0u) {
                desktop_close_other_editors(&editors, DESKTOP_PANEL_DETAILS);
            }
            if ((card_interaction.actions & WENA_CARD_BODY_OPEN_LABELS) != 0u) {
                selected_card = desktop_selected_card(snapshot, card_interaction.card_id);
                if (selected_card != NULL && wena_labels_open(&editors.labels,
                    snapshot->board.id, selected_card))
                    opened_panel = DESKTOP_PANEL_LABELS;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_LABELS);
            }
            if ((card_interaction.actions & WENA_CARD_BODY_OPEN_CHECKLISTS) != 0u) {
                selected_card = desktop_selected_card(snapshot, card_interaction.card_id);
                if (selected_card != NULL && wena_checklists_open(&editors.checklists,
                    selected_card)) opened_panel = DESKTOP_PANEL_CHECKLISTS;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_CHECKLISTS);
            }
            if ((toolbar.actions & DESKTOP_BOARD_SETTINGS) != 0u) {
                if (wena_board_settings_open(&editors.board_settings, snapshot->board.id))
                    opened_panel = DESKTOP_PANEL_BOARD_SETTINGS;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_BOARD_SETTINGS);
            }
            if ((editors.details.interaction.actions & WENA_CARD_DETAILS_MOVE) != 0u) {
                if (wena_card_move_open(&editors.move, &layout,
                    editors.details.interaction.card_id))
                    opened_panel = DESKTOP_PANEL_MOVE_CARD;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_MOVE_CARD);
            }
            if ((editors.details.interaction.actions & WENA_CARD_DETAILS_DESCRIPTION) != 0u) {
                selected_card = desktop_selected_card(snapshot,
                    editors.details.interaction.card_id);
                if (wena_card_description_open(&editors.description, selected_card))
                    opened_panel = DESKTOP_PANEL_DESCRIPTION;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_DESCRIPTION);
            }
            if ((editors.details.interaction.actions & WENA_CARD_DETAILS_CHECKLISTS) != 0u) {
                selected_card = desktop_selected_card(snapshot,
                    editors.details.interaction.card_id);
                if (wena_checklists_open(&editors.checklists, selected_card))
                    opened_panel = DESKTOP_PANEL_CHECKLISTS;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_CHECKLISTS);
            }
            if ((editors.details.interaction.actions & WENA_CARD_DETAILS_LABELS) != 0u) {
                selected_card = desktop_selected_card(snapshot,
                    editors.details.interaction.card_id);
                if (selected_card != NULL && wena_labels_open(&editors.labels,
                    snapshot->board.id, selected_card))
                    opened_panel = DESKTOP_PANEL_LABELS;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_LABELS);
            }
            if ((toolbar.actions & (DESKTOP_ADD_LIST | DESKTOP_ADD_SWIMLANE |
                                     DESKTOP_RENAME_BOARD)) != 0u ||
                (list_interaction.actions & WENA_LIST_HEADER_EDIT_TITLE) != 0u ||
                (swimlane_interaction.actions & WENA_SWIMLANE_EDIT_TITLE) != 0u) {
                desktop_close_other_editors(&editors, DESKTOP_PANEL_HIERARCHY_TITLE);
                if ((toolbar.actions & DESKTOP_ADD_LIST) != 0u)
                    (void)wena_hierarchy_title_open_create(&editors.hierarchy,
                        &layout, WENA_HIERARCHY_LIST);
                else if ((toolbar.actions & DESKTOP_ADD_SWIMLANE) != 0u)
                    (void)wena_hierarchy_title_open_create(&editors.hierarchy,
                        &layout, WENA_HIERARCHY_SWIMLANE);
                else if ((toolbar.actions & DESKTOP_RENAME_BOARD) != 0u)
                    (void)wena_hierarchy_title_open(&editors.hierarchy, &layout,
                        WENA_HIERARCHY_BOARD, snapshot->board.id);
                else if ((swimlane_interaction.actions & WENA_SWIMLANE_EDIT_TITLE) != 0u)
                    (void)wena_hierarchy_title_open(&editors.hierarchy, &layout,
                        WENA_HIERARCHY_SWIMLANE, swimlane_interaction.swimlane_id);
                else
                    (void)wena_hierarchy_title_open_list(&editors.hierarchy, &layout,
                        list_interaction.list_id,list_interaction.swimlane_id);
                if (editors.hierarchy.visible)
                    opened_panel = DESKTOP_PANEL_HIERARCHY_TITLE;
            }
            if (sidebar.visible) {
                desktop_close_other_editors(&editors, DESKTOP_PANEL_NONE);
                if (sidebar.section == WENA_SIDEBAR_ARCHIVES) {
                    if (wena_card_archives_open(&editors.archives, &layout))
                        opened_panel = DESKTOP_PANEL_ARCHIVES;
                    sidebar.visible = 0;
                    sidebar.section = WENA_SIDEBAR_ACTIVITIES;
                } else if (sidebar.section == WENA_SIDEBAR_SETTINGS) {
                    if (wena_board_settings_open(&editors.board_settings, snapshot->board.id))
                        opened_panel = DESKTOP_PANEL_BOARD_SETTINGS;
                    sidebar.visible = 0;
                    sidebar.section = WENA_SIDEBAR_ACTIVITIES;
                } else if (sidebar.section == WENA_SIDEBAR_LABELS) {
                    if (wena_labels_open(&editors.labels, snapshot->board.id, NULL))
                        opened_panel = DESKTOP_PANEL_LABELS;
                    sidebar.visible = 0;
                    sidebar.section = WENA_SIDEBAR_ACTIVITIES;
                }
            }
            /* Add Card is WeKan's inline composer, drawn with the board
             * (layout.card_composer), not a panel of its own. */
            if (opened_panel != DESKTOP_PANEL_MOVE_CARD)
                (void)wena_card_move_render(context, &editors.move, &layout,
                                        (float)width, (float)height);
            if(editors.move.boards_requested){
                editors.move.boards_requested=0;
                if(wena_card_selection_panel_open_card(&editors.card_transfer,editors.move.board_id,editors.move.card_id)){
                    opened_panel=DESKTOP_PANEL_CARD_TRANSFER;
                    desktop_close_other_editors(&editors,DESKTOP_PANEL_CARD_TRANSFER);
                }else editors.move.error=1;
            }
            if(opened_panel!=DESKTOP_PANEL_CARD_TRANSFER)
                (void)wena_card_selection_panel_render_board(context,&editors.card_transfer,
                    &layout,(float)width,(float)height);
            if (opened_panel != DESKTOP_PANEL_HIERARCHY_TITLE)
                (void)wena_hierarchy_title_render(context, &editors.hierarchy, &layout,
                                              (float)width, (float)height);
            if (editors.hierarchy.requested_action == WENA_HIERARCHY_TITLE_MOVE) {
                if (wena_hierarchy_move_open(&editors.hierarchy_move, &layout,
                    editors.hierarchy.kind, editors.hierarchy.target_id))
                    opened_panel = DESKTOP_PANEL_HIERARCHY_MOVE;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_HIERARCHY_MOVE);
            }
            if(editors.hierarchy.requested_action==WENA_HIERARCHY_TITLE_SELECT_CARDS){
                if(wena_card_selection_panel_open(&editors.selection,snapshot->cards,snapshot->card_count,
                    snapshot->board.id,editors.hierarchy.target_id,editors.hierarchy.scope_lane)){
                    opened_panel=DESKTOP_PANEL_SELECTION;
                    desktop_close_other_editors(&editors,DESKTOP_PANEL_SELECTION);
                }else editors.hierarchy.error=1;
            }
            if(opened_panel!=DESKTOP_PANEL_SELECTION)
                (void)wena_card_selection_panel_render_board(context,&editors.selection,
                    &layout,(float)width,(float)height);
            /* Never replay an opener's input into the newly opened panel. */
            if (opened_panel != DESKTOP_PANEL_HIERARCHY_MOVE)
                (void)wena_hierarchy_move_render(context, &editors.hierarchy_move,
                &layout, (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_ARCHIVES)
                (void)wena_card_archives_render(context, &editors.archives, &layout,
                                            (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_DESCRIPTION)
                (void)wena_card_description_render(context, &editors.description,
                    snapshot->cards, snapshot->card_count,
                    (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_CHECKLISTS)
                (void)wena_checklists_render(context, &editors.checklists,
                    snapshot->cards, snapshot->card_count,
                    (float)width, (float)height);
            (void)wena_checklists_poll_destination(&editors.checklists);
            if (opened_panel != DESKTOP_PANEL_LABELS)
                (void)wena_labels_render(context, &editors.labels,
                    snapshot->board.id, snapshot->cards, snapshot->card_count,
                    (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_BOARD_SETTINGS)
                (void)wena_board_settings_render(context, &editors.board_settings,
                    snapshot->board.id, (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_NONE || sidebar.visible)
                wena_checklist_inline_cancel(&preview.inline_edit);
            if (opened_panel != DESKTOP_PANEL_NONE || sidebar.visible)
                wena_reorder_drag_cancel(&preview.drag.gesture);
            if (opened_panel != DESKTOP_PANEL_NONE || sidebar.visible) {
                wena_card_drag_cancel(&preview.card_drag);
                wena_hierarchy_drag_cancel(&preview.hierarchy_drag);
            }
            if (toolbar.board_refresh) {
                desktop_close_other_editors(&editors, DESKTOP_PANEL_NONE);
                wena_card_drag_cancel(&preview.card_drag);
                wena_hierarchy_drag_cancel(&preview.hierarchy_drag);
                wena_checklist_inline_cancel(&preview.inline_edit);
                wena_reorder_drag_cancel(&preview.drag.gesture);
                preview.intent.pending = 0;
                preview.card_drag.error = !wena_board_reload(&mutation, snapshot);
                preview.hierarchy_drag.error = preview.card_drag.error;
                if (!preview.card_drag.error) {
                    label_view.valid = 0;label_view.refresh_pending = 1;
                    label_view.summary_valid = 0;label_view.summary_pending = 1;
                    label_view.sections_valid = 0;label_view.sections_pending = 1;
                }
            }
            (void)wena_hierarchy_drag_process(&preview.hierarchy_drag,&layout);
            completion_result = wena_card_drag_apply(&preview.card_drag, &mutation);
            if (completion_result) {
                label_view.summary_valid = 0;label_view.summary_pending = 1;
            }
            completion_result = wena_checklist_mutation_drag(&checklist_mutation, &preview.drag);
            if (completion_result) {
                label_view.summary_valid = 0;label_view.summary_pending = 1;
            }
            completion_result = wena_checklist_mutation_inline(&checklist_mutation, &preview.inline_edit);
            if (completion_result) {
                label_view.summary_valid = 0;
                label_view.summary_pending = 1;
            }
            completion_result = wena_checklist_mutation_complete(&checklist_mutation, &preview.intent);
            if (completion_result) {
                preview.error = completion_result < 0;
                label_view.summary_valid = 0;
                label_view.summary_pending = 1;
            }
            if (preview.sections.pending) {
                preview.sections.pending = 0;
                preview.sections.error = !wena_card_section_save(database, actor_id,
                    snapshot->board.id, preview.sections.card_id, preview.sections.key,
                    preview.sections.version, preview.sections.collapsed);
                label_view.sections_valid = 0;
                label_view.sections_pending = 1;
            }
            if (opened_panel == DESKTOP_PANEL_CHECKLISTS) label_view.sections_pending = 1;
            (void)wena_card_selection_panel_poll(&editors.selection);
            (void)wena_card_selection_panel_poll(&editors.card_transfer);
            (void)wena_board_presentation_poll(&label_view);
            if (!smoke && collapse_path[0] != '\0' &&
                (toolbar.collapse_retry ||
                 desktop_collapse_changed(&collapse, &observed_collapse))) {
                /* Remember the attempted state even on failure. Retry occurs
                 * only after another change or the explicit Save button. */
                observed_collapse = collapse;
                toolbar.collapse_error = !wena_collapse_preferences_save(
                    collapse_path, database_path, actor_id, &collapse);
            }
            if (SDL_SetRenderDrawColor(renderer, 41, 128, 185, 255) != 0 ||
                SDL_RenderClear(renderer) != 0) DESKTOP_FAIL();
#if DESKTOP_MOBILE
            if (desktop_mobile_editing(context) != (SDL_IsTextInputActive() == SDL_TRUE)) {
                if (SDL_IsTextInputActive()) SDL_StopTextInput();
                else SDL_StartTextInput();
            }
#endif
            nk_sdl_render(NK_ANTI_ALIASING_ON);
            if (screenshot != NULL && frames == 2 && !desktop_screenshot(renderer, screenshot))
                DESKTOP_FAIL();
            SDL_RenderPresent(renderer);
        }
        /* WeKan's file gets what this frame changed. */
        if (wekan_mode && sqlite3_total_changes(database) != synced_changes) {
            if (wena_wekan_sync_export(database, actor_id) < 0)
                wena_debug_log("writing %s: %s", database_path, wena_wekan_sync_error());
            synced_changes = sqlite3_total_changes(database);
        }
        if (smoke) {
            ++frames;
            if (frames >= 3) running = 0;
        }
        if (!smoke) SDL_Delay(16);
    }
    status = 0;
cleanup:
    if (status != 0 && sdl_started && SDL_GetError()[0] != '\0') wena_debug_log("SDL: %s", SDL_GetError());
    wena_card_drag_cancel(&preview.card_drag);
    wena_ui_set_translator(NULL, NULL);
    desktop_close_other_editors(&editors, DESKTOP_PANEL_NONE);
    if (context != NULL) nk_sdl_shutdown();
    if (resize_cursor != NULL) SDL_FreeCursor(resize_cursor);
    if (arrow_cursor != NULL) SDL_FreeCursor(arrow_cursor);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    if (sdl_started) { SDL_StopTextInput(); SDL_Quit(); }
    wena_board_presentation_close(&label_view);
    free(selection_traversal);
    free(selection);
    free(single_selection);
    free(snapshot);free(transfer_snapshot);
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) status = 1;
    wena_embedded_migration_free(&migration);
    if (status != 0) {
        fputs("Unable to open the local Wena desktop\n", stderr);
        if (wena_debug_log_directory()[0] != '\0')
            fprintf(stderr, "See %s/desktop.log\n", wena_debug_log_directory());
    }
    else if (smoke) puts("Wena desktop smoke passed");
#if DESKTOP_MOBILE
    /* Android drops stdout; logcat (and the iOS console) gets the same line. */
    if (status != 0) SDL_Log("Unable to open the local Wena desktop");
    else if (smoke) SDL_Log("Wena desktop smoke passed");
#endif
    wena_debug_log("exit status %d", status);
    wena_debug_log_close();
    return status;
}

#if defined(__AROS__)
/* AROS has no stack request a program can make, so main() moves to a larger
 * stack itself when the one it was given is too small. */
int main(int argc, char **argv)
{
    struct Task *self = FindTask(NULL);
    struct StackSwapStruct stack;
    struct StackSwapArgs arguments;
    int (*entry)(int, char **) = desktop_main;
    APTR function;
    IPTR status;
    if ((IPTR)self->tc_SPUpper - (IPTR)self->tc_SPLower >= DESKTOP_STACK)
        return desktop_main(argc, argv);
    stack.stk_Lower = AllocVec(DESKTOP_STACK, MEMF_ANY);
    if (stack.stk_Lower == NULL) {
        fputs("Not enough memory for the Wena desktop's stack\n", stderr);
        return 1;
    }
    stack.stk_Upper = (APTR)((IPTR)stack.stk_Lower + DESKTOP_STACK);
    stack.stk_Pointer = stack.stk_Upper;
    memset(&arguments, 0, sizeof(arguments));
    arguments.Args[0] = (IPTR)argc;
    arguments.Args[1] = (IPTR)argv;
    /* exec takes the entry point as an APTR; ISO C has no cast for that. */
    memcpy(&function, &entry, sizeof(function));
    status = NewStackSwap(&stack, function, &arguments);
    FreeVec(stack.stk_Lower);
    return (int)status;
}
#endif

#if DESKTOP_MOBILE
/* SDL_main, called by SDLActivity on Android and SDL's UIKit delegate on iOS. */
int main(int argc, char **argv)
{
    int status;
    char message[WENA_EXECUTABLE_PATH_CAPACITY + 64];
    status = desktop_main(argc, argv);
    /* Launched from the home screen (no arguments), a failure is shown rather
     * than the app just closing; a test run with --smoke only logs it. */
    if (status != 0 && argc <= 1) {
        strcpy(message, "Wena could not open its board.");
        if (wena_debug_log_directory()[0] != '\0' &&
            strlen(wena_debug_log_directory()) + 64 < sizeof(message)) {
            strcat(message, "\nSee ");
            strcat(message, wena_debug_log_directory());
            strcat(message, "/desktop.log");
        }
        (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Wena", message, NULL);
    }
#if DESKTOP_IOS
    /* SDL's UIKit delegate keeps the app running after SDL_main returns. */
    exit(status);
#else
    return status;
#endif
}
#endif
