/* MIT-licensed local desktop entrypoint. The local OS user supplies a trusted
 * actor identity; this is not a remote authentication or board-sharing API. */
#include <SDL2/SDL.h>
#include "platform/svg.h"
#include "components/boards/board_header.h"
#include "platform/sdl_nuklear.h"
#include "platform/theme.h"
#include "platform/dependencies.h"
#include "platform/font.h"
#include "features/board.h"
#include "features/board_filter.h"
#include "features/card_mutation.h"
#include "features/card_create.h"
#include "features/card_move.h"
#include "features/card_archives.h"
#include "features/card_description.h"
#include "features/card_description_mutation.h"
#include "features/checklists.h"
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
#include "features/hierarchy_move_mutation.h"
#include "components/cards/card_body.h"
#include "components/lists/list_header.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include "../server/sqlite_workspace.h"
#include "../server/embedded_migration.h"
#include "../server/executable_path.h"
#include "../imports/i18n/catalog.h"
#include "../imports/i18n/ui_catalog.h"
#include "../imports/i18n/locale.h"
#include "../imports/preferences/collapse.h"
#include "../imports/ui/page_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

#define DESKTOP_ADD_LIST 1u
#define DESKTOP_ADD_SWIMLANE 2u
#define DESKTOP_RENAME_BOARD 4u
#define DESKTOP_BOARD_SETTINGS 8u
typedef struct WenaDesktopToolbar {
    WenaLanguagePicker *language;
    WenaBoardFilterState *filter;
    WenaBoardPresentation *labels;
    int filter_changed;
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
    toolbar->collapse_retry = 0;
    wena_language_picker_render(context, toolbar->language);
    nk_layout_row_dynamic(context, 28.0f, 4);
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_ADD_LIST)))
        toolbar->actions |= DESKTOP_ADD_LIST;
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_ADD_SWIMLANE)))
        toolbar->actions |= DESKTOP_ADD_SWIMLANE;
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_RENAME_BOARD)))
        toolbar->actions |= DESKTOP_RENAME_BOARD;
    if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_SETTINGS)))
        toolbar->actions |= DESKTOP_BOARD_SETTINGS;
    toolbar->filter_changed = wena_board_filter_render(context, toolbar->filter);
    if (toolbar->labels != NULL && toolbar->labels->error) {
        nk_layout_row_dynamic(context, 22.0f, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_LABELS), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28.0f, 2);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH)))
            toolbar->labels->refresh_pending = 1;
    }
    if (toolbar->labels != NULL && toolbar->labels->summary_error) {
        nk_layout_row_dynamic(context, 22.0f, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_CHECKLISTS), NK_TEXT_LEFT);
        nk_layout_row_dynamic(context, 28.0f, 2);
        nk_label_wrap(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED));
        if (nk_button_label(context, wena_ui_text(WENA_UI_TEXT_REFRESH)))
            toolbar->labels->summary_pending = 1;
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

static void desktop_usage(FILE *output)
{
    fputs("Usage: wena-desktop --database ABS_PATH --actor ID --board ID\n"
          "                    [--create [--title TITLE]] [--language LOCALE] [--smoke]\n"
          "       wena-desktop --dependency-info\n"
          "       wena-desktop --help\n"
          "\n"
          "--create initializes a new local workspace without replacing files.\n"
          "Omit --create and --title to reopen it. The parent directory must exist.\n"
          "--smoke renders three frames with editor writes disabled.\n"
          "--dependency-info reports linked libraries without opening a workspace.\n",
          output);
}

int main(int argc, char **argv)
{
    const char *database_path, *actor_id, *board_id, *board_title, *requested_language;
    const char *const *languages;
    size_t language_count;
    int create_workspace, smoke, i, status, running, frames, width, height, sdl_started;
    char executable[WENA_EXECUTABLE_PATH_CAPACITY];
    char language_path[512], detected_locale[64];
    char collapse_path[WENA_EXECUTABLE_PATH_CAPACITY];
    struct stat info;
    sqlite3 *database;
    WenaEmbeddedMigration migration;
    WenaI18nCatalog catalog;
    WenaLanguageState language;
    WenaLanguagePicker language_picker;
    WenaDesktopToolbar toolbar;
    WenaSqliteWorkspaceSeed seed;
    WenaSqliteBoardSnapshot *snapshot;
    WenaBoardLayout layout;
    WenaBoardSidebar sidebar;
    WenaBoardCollapseState collapse;
    WenaBoardCollapseState observed_collapse;
    WenaBoardFilterState filter;
    WenaCardInteraction card_interaction;
    WenaCardMutation mutation;
    WenaCardDescriptionMutation description_mutation;
    WenaChecklistMutation checklist_mutation;
    WenaBoardPresentation label_view;
    const WenaCard *selected_card;
    WenaListInteraction list_interaction;
    WenaSwimlaneInteraction swimlane_interaction;
    WenaHierarchyMutation hierarchy_mutation;
    WenaHierarchyMoveMutation hierarchy_move_mutation;
    WenaDesktopEditors editors;
    WenaDesktopPanel opened_panel;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Event event;
    struct nk_context *context;
    struct nk_font_atlas *atlas;
    struct nk_font *font;
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        desktop_usage(stdout);
        return ferror(stdout) ? 1 : 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--dependency-info"))
        return wena_desktop_dependency_report(stdout) ? 0 : 1;
    database_path = NULL; actor_id = NULL; board_id = NULL;
    board_title = NULL; requested_language = NULL;
    smoke = 0; create_workspace = 0;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--smoke") && !smoke) smoke = 1;
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
            desktop_usage(stderr);
            return 2;
        }
    }
    if (database_path == NULL || database_path[0] != '/' ||
        strlen(database_path) >= WENA_EXECUTABLE_PATH_CAPACITY ||
        !wena_model_identifier_valid(actor_id) || !wena_model_identifier_valid(board_id) ||
        (board_title != NULL && !create_workspace) ||
        (!create_workspace && (stat(database_path, &info) != 0 || !S_ISREG(info.st_mode)))) {
        fputs("An absolute database path, actor and board are required; use --create for a new workspace\n", stderr);
        return 2;
    }
    memset(&migration, 0, sizeof(migration));
    memset(&catalog, 0, sizeof(catalog));
    snapshot = (WenaSqliteBoardSnapshot *)calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) return 1;
    memset(&layout, 0, sizeof(layout));
    memset(&editors, 0, sizeof(editors));
    memset(&card_interaction, 0, sizeof(card_interaction));
    memset(&list_interaction, 0, sizeof(list_interaction));
    memset(&swimlane_interaction, 0, sizeof(swimlane_interaction));
    memset(&toolbar, 0, sizeof(toolbar));
    memset(&label_view, 0, sizeof(label_view));
    database = NULL; window = NULL; renderer = NULL; context = NULL;
    sdl_started = 0; status = 1;
    if (!wena_executable_path_current(executable, sizeof(executable)) ||
        !wena_i18n_catalog_open(&catalog, executable) ||
        !wena_embedded_migration_load(executable, &migration)) goto cleanup;
    languages = wena_ui_catalog_languages(&language_count);
    detected_locale[0] = '\0';
    (void)wena_locale_detect(detected_locale, sizeof(detected_locale));
    language_path[0] = '\0';
    /* Leave room for both ".language" and the settings writer's ".tmp". */
    if (strlen(database_path) + 14 < sizeof(language_path)) {
        strcpy(language_path, database_path);
        strcat(language_path, ".language");
    }
    if (!wena_language_init(&language, requested_language != NULL ? NULL : language_path,
        requested_language != NULL ? requested_language : detected_locale,
        languages, language_count)) goto cleanup;
    wena_ui_set_translator(wena_ui_catalog_translate, &language);
    if (create_workspace) {
        seed.actor_id = actor_id; seed.actor_name = actor_id;
        seed.board_id = board_id;
        seed.board_title = board_title == NULL ? board_id : board_title;
        seed.swimlane_id = "default-lane";
        seed.swimlane_title = wena_ui_text(WENA_UI_TEXT_SWIMLANE);
        seed.list_id = "default-list";
        seed.list_title = wena_ui_text(WENA_UI_TEXT_LIST);
        if (!wena_sqlite_workspace_create(database_path, migration.bytes,
            migration.length, migration.sha256, &seed)) goto cleanup;
    }
    /* Read-only scope preflight prevents invalid actor/board launches from
     * creating a schema, WAL or any domain records. */
    if (sqlite3_open_v2(database_path, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto cleanup;
    if (!wena_sqlite_connection_harden(database) ||
        !actor_exists(database, actor_id) ||
        !wena_sqlite_board_load(database, board_id, snapshot)) goto cleanup;
    if (sqlite3_close(database) != SQLITE_OK) goto cleanup;
    database = NULL;
    if (!wena_sqlite_open(database_path, migration.bytes, migration.length,
                          migration.sha256, &database) ||
        !actor_exists(database, actor_id) ||
        !wena_sqlite_board_load(database, board_id, snapshot)) goto cleanup;
    if (requested_language != NULL && !smoke && language_path[0] != '\0' &&
        !wena_language_set(&language, language_path, requested_language,
            languages, language_count)) goto cleanup;
    if (!wena_language_picker_init(&language_picker, &language, language_path,
        smoke || language_path[0] == '\0')) goto cleanup;
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
    if (!wena_board_collapse_sync(&collapse, &layout)) goto cleanup;
    observed_collapse = collapse;
    wena_board_filter_init(&filter);
    if (!wena_board_filter_sync(&filter, snapshot->board.id)) goto cleanup;
    wena_card_details_init(&editors.details);
    layout.sidebar = &sidebar; layout.collapse = &collapse;
    layout.sidebar_as_window = 1;
    layout.card_interaction = &card_interaction;
    layout.list_interaction = &list_interaction;
    toolbar.language = &language_picker;
    toolbar.filter = &filter;
    toolbar.labels = &label_view;
    layout.card_badges = desktop_card_badges;
    layout.card_badges_context = &label_view;
    layout.card_visible = wena_board_filter_matches;
    layout.card_visible_context = &filter;
    layout.toolbar = desktop_toolbar;
    layout.toolbar_context = &toolbar;
    layout.swimlane_interaction = &swimlane_interaction;
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
                                 snapshot->cards, snapshot->card_count)) goto cleanup;
    if (!wena_card_description_mutation_init(&description_mutation, database,
                                             actor_id, board_id)) goto cleanup;
    wena_card_description_init(&editors.description,
        wena_card_description_mutation_load,
        smoke ? NULL : wena_card_description_mutation_save,
        &description_mutation);
    if (!wena_checklist_mutation_init(&checklist_mutation, database,
                                      actor_id, board_id)) goto cleanup;
    wena_checklists_init(&editors.checklists, wena_checklist_mutation_load,
        smoke ? NULL : wena_checklist_mutation_save, &checklist_mutation);
    if (!wena_board_presentation_init(&label_view, database, actor_id, board_id) ||
        !label_view.valid) goto cleanup;
    wena_labels_init(&editors.labels, wena_board_presentation_labels_load,
        smoke ? NULL : wena_board_presentation_labels_save, &label_view);
    wena_board_settings_init(&editors.board_settings, wena_board_presentation_settings_load,
        smoke ? NULL : wena_board_presentation_settings_save, &label_view);
    if (!smoke) {
        if (!wena_hierarchy_mutation_init(&hierarchy_mutation, database,
            actor_id, board_id, snapshot)) goto cleanup;
        if (!wena_hierarchy_move_mutation_init(&hierarchy_move_mutation,
            database, actor_id, board_id, snapshot)) goto cleanup;
        wena_hierarchy_move_init(&editors.hierarchy_move,
            wena_hierarchy_move_mutation_load, wena_hierarchy_move_mutation_move,
            &hierarchy_move_mutation);
        wena_hierarchy_title_set_adapter(&editors.hierarchy,
            wena_hierarchy_mutation_load, wena_hierarchy_mutation_save,
            &hierarchy_mutation);
        wena_hierarchy_title_set_create_adapter(&editors.hierarchy,
            wena_hierarchy_mutation_create);
        if (!wena_card_mutation_set_create_cache(&mutation, &snapshot->card_count,
            WENA_SQLITE_BOARD_MAX_CARDS)) goto cleanup;
        wena_card_create_init(&editors.create, wena_card_mutation_create, &mutation);
        wena_card_move_init(&editors.move, wena_card_mutation_load,
                            wena_card_mutation_move, &mutation);
        wena_card_move_set_reorder_adapter(&editors.move,
                                           wena_card_mutation_reorder);
        wena_card_archives_init(&editors.archives, wena_card_mutation_load_archived,
                                wena_card_mutation_restore, &mutation);
        wena_card_details_set_title_adapter(&editors.details, wena_card_mutation_load,
                                            wena_card_mutation_save, &mutation);
        wena_card_details_set_archive_adapter(&editors.details, wena_card_mutation_archive);
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) goto cleanup;
    sdl_started = 1;
    window = SDL_CreateWindow("WeKan Native", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 1024, 720, SDL_WINDOW_RESIZABLE |
        (smoke ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
    if (window == NULL) goto cleanup;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL) goto cleanup;
    context = nk_sdl_init(window, renderer);
    if (context == NULL) goto cleanup;
    wena_sdl_install_clipboard(context);
    nk_sdl_font_stash_begin(&atlas);
    font = wena_native_font_add(atlas, 14.0f);
    if (font == NULL) font = nk_font_atlas_add_default(atlas, 14.0f, NULL);
    if (font == NULL) goto cleanup;
    nk_sdl_font_stash_end();
    nk_style_set_font(context, &font->handle);
    if (!wena_native_theme_apply(context)) goto cleanup;
    wena_board_header_set_title_renderer(wena_svg_board_title);
    SDL_StartTextInput();
    running = 1; frames = 0;
    while (running) {
        nk_input_begin(context);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (!smoke) (void)wena_sdl_handle_event(context, &event);
        }
        nk_input_end(context);
        SDL_GetWindowSize(window, &width, &height);
        if (width > 0 && height > 0) {
            opened_panel = DESKTOP_PANEL_NONE;
            layout.card_count = snapshot->card_count;
            layout.list_count = snapshot->list_count;
            layout.swimlane_count = snapshot->swimlane_count;
            if (!wena_board_feature_render_with_state(context, &layout,
                (float)width, (float)height, &editors.details)) goto cleanup;
            if (toolbar.filter_changed) {
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
                (list_interaction.actions & WENA_LIST_HEADER_OPEN_MENU) != 0u ||
                swimlane_interaction.actions != 0u) {
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
                else if (swimlane_interaction.actions != 0u)
                    (void)wena_hierarchy_title_open(&editors.hierarchy, &layout,
                        WENA_HIERARCHY_SWIMLANE, swimlane_interaction.swimlane_id);
                else
                    (void)wena_hierarchy_title_open(&editors.hierarchy, &layout,
                        WENA_HIERARCHY_LIST, list_interaction.list_id);
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
                } else if (sidebar.section == WENA_SIDEBAR_LABELS) {
                    if (wena_labels_open(&editors.labels, snapshot->board.id, NULL))
                        opened_panel = DESKTOP_PANEL_LABELS;
                    sidebar.visible = 0;
                    sidebar.section = WENA_SIDEBAR_ACTIVITIES;
                }
            }
            if (opened_panel != DESKTOP_PANEL_CREATE_CARD)
                (void)wena_card_create_render(context, &editors.create, &layout,
                                          (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_MOVE_CARD)
                (void)wena_card_move_render(context, &editors.move, &layout,
                                        (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_HIERARCHY_TITLE)
                (void)wena_hierarchy_title_render(context, &editors.hierarchy, &layout,
                                              (float)width, (float)height);
            if (editors.hierarchy.requested_action == WENA_HIERARCHY_TITLE_MOVE) {
                if (wena_hierarchy_move_open(&editors.hierarchy_move, &layout,
                    editors.hierarchy.kind, editors.hierarchy.target_id))
                    opened_panel = DESKTOP_PANEL_HIERARCHY_MOVE;
                desktop_close_other_editors(&editors, DESKTOP_PANEL_HIERARCHY_MOVE);
            }
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
            if (opened_panel != DESKTOP_PANEL_LABELS)
                (void)wena_labels_render(context, &editors.labels,
                    snapshot->board.id, snapshot->cards, snapshot->card_count,
                    (float)width, (float)height);
            if (opened_panel != DESKTOP_PANEL_BOARD_SETTINGS)
                (void)wena_board_settings_render(context, &editors.board_settings,
                    snapshot->board.id, (float)width, (float)height);
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
                SDL_RenderClear(renderer) != 0) goto cleanup;
            nk_sdl_render(NK_ANTI_ALIASING_ON);
            SDL_RenderPresent(renderer);
        }
        if (smoke) {
            ++frames;
            if (frames >= 3) running = 0;
        }
        if (!smoke) SDL_Delay(16);
    }
    status = 0;
cleanup:
    wena_ui_set_translator(NULL, NULL);
    desktop_close_other_editors(&editors, DESKTOP_PANEL_NONE);
    if (context != NULL) nk_sdl_shutdown();
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    if (sdl_started) { SDL_StopTextInput(); SDL_Quit(); }
    wena_board_presentation_close(&label_view);
    free(snapshot);
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) status = 1;
    wena_embedded_migration_free(&migration);
    wena_i18n_catalog_close(&catalog);
    if (status != 0) fputs("Unable to open the local Wena desktop\n", stderr);
    else if (smoke) puts("Wena desktop smoke passed");
    return status;
}
