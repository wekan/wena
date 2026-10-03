/* tests/test_ferretdb_roundtrip.py's Wena: imports WeKan's documents from
 * DIR/wekan.sqlite, renames card "c1", adds card "wena-new" and ticks the
 * checklist item, and exports - Wena's whole read and write path over a file
 * FerretDB wrote. */
#include "../server/wekan_sync.h"
#include "../server/sqlite_storage.h"
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
    if (argc != 2 || strlen(argv[1]) > 900) return 2;
    sprintf(path, "%s/wekan.sqlite", argv[1]);
    if (!wena_sqlite_compiled_bundle(&bundle, &length, sha) ||
        !wena_sqlite_open(":memory:", bundle, length, sha, &db)) return 1;
    ok = wena_wekan_sync_attach(db, path) && wena_wekan_sync_user(db, "ada", user, sizeof(user)) &&
         wena_wekan_sync_import(db) &&
         sqlite3_exec(db, "UPDATE cards SET title = 'Renamed in Wena', version = version + 1 WHERE id = 'c1';"
                          "INSERT INTO cards(id, board_id, swimlane_id, list_id, title, position, version) "
                          "SELECT 'wena-new', board_id, swimlane_id, list_id, 'Made in Wena', 1, 1 FROM cards WHERE id = 'c1';"
                          "UPDATE checklist_items SET is_finished = 1 WHERE id = 'i1'", NULL, NULL, NULL) == SQLITE_OK &&
         wena_wekan_sync_export(db, user) == 3;
    if (!ok) fprintf(stderr, "%s / %s\n", wena_wekan_sync_error(), sqlite3_errmsg(db));
    sqlite3_close(db);
    free(bundle);
    return ok ? 0 : 1;
}
