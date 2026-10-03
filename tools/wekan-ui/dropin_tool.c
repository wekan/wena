/* tools/wekan-ui/dropin.sh's Wena: on the first board of WENA_USER in
 * ROOT/files/db/wekan.sqlite, a list "To Do" and a card "Made in Wena" with a
 * checklist, written as Wena writes them. */
#include "../../server/wekan_sync.h"
#include "../../server/sqlite_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    unsigned char *bundle;
    size_t length;
    char sha[65], path[1024], user[64];
    sqlite3 *db;
    int ok;
    if (argc != 3 || strlen(argv[1]) > 900) return 2;
    sprintf(path, "%s/files/db/wekan.sqlite", argv[1]);
    if (!wena_sqlite_compiled_bundle(&bundle, &length, sha) ||
        !wena_sqlite_open(":memory:", bundle, length, sha, &db)) return 1;
    ok = wena_wekan_sync_attach(db, path) && wena_wekan_sync_user(db, argv[2], user, sizeof(user)) &&
         wena_wekan_sync_import(db) &&
         sqlite3_exec(db,
            "INSERT INTO lists(id, board_id, title, position, version) SELECT 'dropinList', b.id, 'To Do', 0, 1 "
            "FROM boards b LIMIT 1;"
            "INSERT INTO cards(id, board_id, swimlane_id, list_id, title, position, version) SELECT 'dropinCard', "
            "l.board_id, s.id, l.id, 'Made in Wena', 0, 1 FROM lists l JOIN swimlanes s ON s.board_id = l.board_id "
            "WHERE l.id = 'dropinList' LIMIT 1;"
            "INSERT INTO card_descriptions(card_id, board_id, description) SELECT id, board_id, "
            "'Written by Wena into wekan.sqlite' FROM cards WHERE id = 'dropinCard';"
            "INSERT INTO checklists(id, board_id, card_id, title, position) SELECT 'dropinChecklist', board_id, id, "
            "'Steps', 0 FROM cards WHERE id = 'dropinCard';"
            "INSERT INTO checklist_items(id, board_id, card_id, checklist_id, title, position, is_finished) "
            "SELECT 'dropinItem', board_id, card_id, id, 'Open in WeKan', 0, 1 FROM checklists WHERE id = 'dropinChecklist'",
            NULL, NULL, NULL) == SQLITE_OK &&
         wena_wekan_sync_export(db, user) > 0;
    if (!ok) fprintf(stderr, "%s / %s\n", wena_wekan_sync_error(), sqlite3_errmsg(db));
    sqlite3_close(db);
    free(bundle);
    return ok ? 0 : 1;
}
