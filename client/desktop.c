/* MIT-licensed local desktop entrypoint. The local OS user supplies a trusted
 * actor identity; this is not a remote authentication or board-sharing API. */
#include <SDL2/SDL.h>
#include "platform/sdl_nuklear.h"
#include "platform/theme.h"
#include "features/board.h"
#include "features/card_mutation.h"
#include "features/card_create.h"
#include "features/card_move.h"
#include "features/card_archives.h"
#include "features/language_picker.h"
#include "features/hierarchy_title.h"
#include "features/hierarchy_mutation.h"
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
#include "../imports/ui/page_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DESKTOP_ADD_LIST 1u
#define DESKTOP_ADD_SWIMLANE 2u
#define DESKTOP_RENAME_BOARD 4u
typedef struct WenaDesktopToolbar {
    WenaLanguagePicker *language;
    unsigned int actions;
} WenaDesktopToolbar;

static void desktop_toolbar(struct nk_context *context, void *opaque)
{
    WenaDesktopToolbar *toolbar;
    toolbar = (WenaDesktopToolbar *)opaque;
    toolbar->actions = 0u;
    wena_language_picker_render(context, toolbar->language);
    nk_layout_row_dynamic(context, 28.0f, 3);
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_ADD_LIST)))
        toolbar->actions |= DESKTOP_ADD_LIST;
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_ADD_SWIMLANE)))
        toolbar->actions |= DESKTOP_ADD_SWIMLANE;
    if (nk_button_label(context, wena_ui_control_text(WENA_UI_RENAME_BOARD)))
        toolbar->actions |= DESKTOP_RENAME_BOARD;
}

static int identifier(const char *text)
{
    size_t i;
    unsigned char c;
    if (text == NULL || text[0] == '\0') return 0;
    for (i = 0; text[i] != '\0'; ++i) {
        if (i >= WENA_ID_CAPACITY - 1u) return 0;
        c = (unsigned char)text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 1;
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

int main(int argc, char **argv)
{
    const char *database_path, *actor_id, *board_id, *board_title, *requested_language;
    const char *const *languages;
    size_t language_count;
    int create_workspace, smoke, i, status, running, frames, width, height, sdl_started;
    char executable[WENA_EXECUTABLE_PATH_CAPACITY];
    char language_path[512], detected_locale[64];
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
    WenaCardDetailsState details;
    WenaCardInteraction card_interaction;
    WenaCardMutation mutation;
    WenaCardCreateState create_state;
    WenaCardMoveState move_state;
    WenaCardArchivesState archives_state;
    WenaListInteraction list_interaction;
    WenaSwimlaneInteraction swimlane_interaction;
    WenaHierarchyTitleState hierarchy_state;
    WenaHierarchyMutation hierarchy_mutation;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Event event;
    struct nk_context *context;
    struct nk_font_atlas *atlas;
    struct nk_font *font;
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
            fputs("Usage: wena-desktop --database ABS_PATH --actor ID --board ID [--create [--title TITLE]] [--language LOCALE] [--smoke]\n", stderr);
            return 2;
        }
    }
    if (database_path == NULL || database_path[0] != '/' ||
        strlen(database_path) >= WENA_EXECUTABLE_PATH_CAPACITY ||
        !identifier(actor_id) || !identifier(board_id) ||
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
    memset(&card_interaction, 0, sizeof(card_interaction));
    memset(&list_interaction, 0, sizeof(list_interaction));
    memset(&swimlane_interaction, 0, sizeof(swimlane_interaction));
    memset(&toolbar, 0, sizeof(toolbar));
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
    if (!actor_exists(database, actor_id) ||
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
    wena_card_details_init(&details);
    layout.sidebar = &sidebar; layout.collapse = &collapse;
    layout.sidebar_as_window = 1;
    layout.card_interaction = &card_interaction;
    layout.list_interaction = &list_interaction;
    toolbar.language = &language_picker;
    layout.toolbar = desktop_toolbar;
    layout.toolbar_context = &toolbar;
    layout.swimlane_interaction = &swimlane_interaction;
    wena_card_create_init(&create_state, NULL, NULL);
    wena_card_move_init(&move_state, NULL, NULL, NULL);
    wena_card_archives_init(&archives_state, NULL, NULL, NULL);
    wena_hierarchy_title_init(&hierarchy_state);
    if (!wena_card_mutation_init(&mutation, database, actor_id, board_id,
                                 snapshot->cards, snapshot->card_count)) goto cleanup;
    if (!smoke) {
        if (!wena_hierarchy_mutation_init(&hierarchy_mutation, database,
            actor_id, board_id, snapshot)) goto cleanup;
        wena_hierarchy_title_set_adapter(&hierarchy_state,
            wena_hierarchy_mutation_load, wena_hierarchy_mutation_save,
            &hierarchy_mutation);
        wena_hierarchy_title_set_create_adapter(&hierarchy_state,
            wena_hierarchy_mutation_create);
        if (!wena_card_mutation_set_create_cache(&mutation, &snapshot->card_count,
            WENA_SQLITE_BOARD_MAX_CARDS)) goto cleanup;
        wena_card_create_init(&create_state, wena_card_mutation_create, &mutation);
        wena_card_move_init(&move_state, wena_card_mutation_load,
                            wena_card_mutation_move, &mutation);
        wena_card_archives_init(&archives_state, wena_card_mutation_load_archived,
                                wena_card_mutation_restore, &mutation);
        wena_card_details_set_title_adapter(&details, wena_card_mutation_load,
                                            wena_card_mutation_save, &mutation);
        wena_card_details_set_archive_adapter(&details, wena_card_mutation_archive);
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
    nk_sdl_font_stash_begin(&atlas);
    font = nk_font_atlas_add_default(atlas, 14.0f, NULL);
    if (font == NULL) goto cleanup;
    nk_sdl_font_stash_end();
    nk_style_set_font(context, &font->handle);
    if (!wena_native_theme_apply(context)) goto cleanup;
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
            layout.card_count = snapshot->card_count;
            layout.list_count = snapshot->list_count;
            layout.swimlane_count = snapshot->swimlane_count;
            if (!wena_board_feature_render_with_state(context, &layout,
                (float)width, (float)height, &details)) goto cleanup;
            /* An explicit board action takes focus from the sidebar. Keeping
             * the old sidebar visible would close the newly opened editor
             * again below during this same frame. */
            if (list_interaction.actions != 0u ||
                card_interaction.actions != 0u || toolbar.actions != 0u ||
                swimlane_interaction.actions != 0u ||
                (details.interaction.actions & WENA_CARD_DETAILS_MOVE) != 0u)
                sidebar.visible = 0;
            if ((list_interaction.actions & WENA_LIST_HEADER_ADD_CARD) != 0u) {
                wena_card_details_close(&details);
                wena_card_move_close(&move_state);
                wena_hierarchy_title_close(&hierarchy_state);
                wena_card_archives_close(&archives_state);
                (void)wena_card_create_open(&create_state, &layout, &list_interaction);
            }
            if ((card_interaction.actions & (WENA_CARD_BODY_OPEN_DETAILS |
                                             WENA_CARD_BODY_OPEN_MENU)) != 0u) {
                wena_card_create_close(&create_state);
                wena_card_move_close(&move_state);
                wena_hierarchy_title_close(&hierarchy_state);
                wena_card_archives_close(&archives_state);
            }
            if ((details.interaction.actions & WENA_CARD_DETAILS_MOVE) != 0u) {
                (void)wena_card_move_open(&move_state, &layout,
                    details.interaction.card_id);
                wena_card_details_close(&details);
                wena_card_create_close(&create_state);
                wena_hierarchy_title_close(&hierarchy_state);
                wena_card_archives_close(&archives_state);
            }
            if (toolbar.actions != 0u ||
                (list_interaction.actions & WENA_LIST_HEADER_OPEN_MENU) != 0u ||
                swimlane_interaction.actions != 0u) {
                wena_card_details_close(&details);
                wena_card_create_close(&create_state);
                wena_card_move_close(&move_state);
                wena_card_archives_close(&archives_state);
                if ((toolbar.actions & DESKTOP_ADD_LIST) != 0u)
                    (void)wena_hierarchy_title_open_create(&hierarchy_state,
                        &layout, WENA_HIERARCHY_LIST);
                else if ((toolbar.actions & DESKTOP_ADD_SWIMLANE) != 0u)
                    (void)wena_hierarchy_title_open_create(&hierarchy_state,
                        &layout, WENA_HIERARCHY_SWIMLANE);
                else if ((toolbar.actions & DESKTOP_RENAME_BOARD) != 0u)
                    (void)wena_hierarchy_title_open(&hierarchy_state, &layout,
                        WENA_HIERARCHY_BOARD, snapshot->board.id);
                else if (swimlane_interaction.actions != 0u)
                    (void)wena_hierarchy_title_open(&hierarchy_state, &layout,
                        WENA_HIERARCHY_SWIMLANE, swimlane_interaction.swimlane_id);
                else
                    (void)wena_hierarchy_title_open(&hierarchy_state, &layout,
                        WENA_HIERARCHY_LIST, list_interaction.list_id);
            }
            if (sidebar.visible) {
                wena_card_details_close(&details);
                wena_card_create_close(&create_state);
                wena_card_move_close(&move_state);
                wena_hierarchy_title_close(&hierarchy_state);
                wena_card_archives_close(&archives_state);
                if (sidebar.section == WENA_SIDEBAR_ARCHIVES) {
                    (void)wena_card_archives_open(&archives_state, &layout);
                    sidebar.visible = 0;
                    sidebar.section = WENA_SIDEBAR_ACTIVITIES;
                }
            }
            (void)wena_card_create_render(context, &create_state, &layout,
                                          (float)width, (float)height);
            (void)wena_card_move_render(context, &move_state, &layout,
                                        (float)width, (float)height);
            (void)wena_hierarchy_title_render(context, &hierarchy_state, &layout,
                                              (float)width, (float)height);
            (void)wena_card_archives_render(context, &archives_state, &layout,
                                            (float)width, (float)height);
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
    if (context != NULL) nk_sdl_shutdown();
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    if (sdl_started) { SDL_StopTextInput(); SDL_Quit(); }
    free(snapshot);
    if (database != NULL && sqlite3_close(database) != SQLITE_OK) status = 1;
    wena_embedded_migration_free(&migration);
    wena_i18n_catalog_close(&catalog);
    if (status != 0) fputs("Unable to open the local Wena desktop\n", stderr);
    else if (smoke) puts("Wena desktop smoke passed");
    return status;
}
