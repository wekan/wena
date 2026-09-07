/* MIT-licensed local desktop entrypoint. The local OS user supplies a trusted
 * actor identity; this is not a remote authentication or board-sharing API. */
#include <SDL2/SDL.h>
#include "platform/sdl_nuklear.h"
#include "features/board.h"
#include "features/card_mutation.h"
#include "../server/sqlite_board.h"
#include "../server/sqlite_storage.h"
#include "../server/embedded_migration.h"
#include "../server/executable_path.h"
#include "../imports/i18n/catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    const char *database_path, *actor_id, *board_id;
    int smoke, i, status, running, frames, width, height, sdl_started;
    char executable[WENA_EXECUTABLE_PATH_CAPACITY];
    struct stat info;
    sqlite3 *database;
    WenaEmbeddedMigration migration;
    WenaI18nCatalog catalog;
    WenaSqliteBoardSnapshot *snapshot;
    WenaBoardLayout layout;
    WenaBoardSidebar sidebar;
    WenaBoardCollapseState collapse;
    WenaCardDetailsState details;
    WenaCardInteraction card_interaction;
    WenaCardMutation mutation;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Event event;
    struct nk_context *context;
    struct nk_font_atlas *atlas;
    struct nk_font *font;
    database_path = NULL; actor_id = NULL; board_id = NULL;
    smoke = 0;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--smoke") && !smoke) smoke = 1;
        else if (!strcmp(argv[i], "--database") && database_path == NULL && i + 1 < argc)
            database_path = argv[++i];
        else if (!strcmp(argv[i], "--actor") && actor_id == NULL && i + 1 < argc)
            actor_id = argv[++i];
        else if (!strcmp(argv[i], "--board") && board_id == NULL && i + 1 < argc)
            board_id = argv[++i];
        else {
            fputs("Usage: wena-desktop --database ABS_PATH --actor ID --board ID [--smoke]\n", stderr);
            return 2;
        }
    }
    if (database_path == NULL || database_path[0] != '/' ||
        strlen(database_path) >= WENA_EXECUTABLE_PATH_CAPACITY ||
        !identifier(actor_id) || !identifier(board_id) ||
        stat(database_path, &info) != 0 || !S_ISREG(info.st_mode)) {
        fputs("An existing absolute database path, actor and board are required\n", stderr);
        return 2;
    }
    memset(&migration, 0, sizeof(migration));
    memset(&catalog, 0, sizeof(catalog));
    snapshot = (WenaSqliteBoardSnapshot *)calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) return 1;
    memset(&layout, 0, sizeof(layout));
    memset(&card_interaction, 0, sizeof(card_interaction));
    database = NULL; window = NULL; renderer = NULL; context = NULL;
    sdl_started = 0; status = 1;
    if (!wena_executable_path_current(executable, sizeof(executable)) ||
        !wena_i18n_catalog_open(&catalog, executable) ||
        !wena_embedded_migration_load(executable, &migration)) goto cleanup;
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
    layout.board = &snapshot->board;
    layout.swimlanes = snapshot->swimlanes; layout.swimlane_count = snapshot->swimlane_count;
    layout.lists = snapshot->lists; layout.list_count = snapshot->list_count;
    layout.cards = snapshot->cards; layout.card_count = snapshot->card_count;
    wena_board_sidebar_init(&sidebar);
    wena_board_collapse_init(&collapse);
    wena_card_details_init(&details);
    layout.sidebar = &sidebar; layout.collapse = &collapse;
    layout.card_interaction = &card_interaction;
    if (!wena_card_mutation_init(&mutation, database, actor_id, board_id,
                                 snapshot->cards, snapshot->card_count)) goto cleanup;
    if (!smoke) {
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
    SDL_StartTextInput();
    running = 1; frames = 0;
    while (running) {
        nk_input_begin(context);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (!smoke) (void)nk_sdl_handle_event(&event);
        }
        nk_input_end(context);
        SDL_GetWindowSize(window, &width, &height);
        if (width > 0 && height > 0) {
            if (!wena_board_feature_render_with_state(context, &layout,
                (float)width, (float)height, &details)) goto cleanup;
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
