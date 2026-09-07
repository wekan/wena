#include "../server/sqlite_storage.h"
#include "../server/sqlite_backup.h"
#include "../server/sqlite_restore.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep this identical to the native checklist snapshot's selected-card query. */
static const char item_query[] = "SELECT id,checklist_id,title,position,is_finished,version,created_at,updated_at,board_id FROM checklist_items WHERE card_id=?2 ORDER BY checklist_id,position,id";
typedef struct Bundle { unsigned char *bytes; size_t length; char hash[65]; } Bundle;
typedef struct Stats { int rows, steps, scans, sorts, indexed, searched, malformed; } Stats;
static void read_bundle(const char *path, Bundle *bundle)
{
    FILE *file; long size;
    file = fopen(path, "rb"); assert(file); assert(fseek(file, 0, SEEK_END) == 0); size = ftell(file); assert(size > 0); rewind(file);
    bundle->length = (size_t)size; bundle->bytes = (unsigned char *)malloc(bundle->length); assert(bundle->bytes);
    assert(fread(bundle->bytes, 1, bundle->length, file) == bundle->length); assert(fclose(file) == 0);
    wena_sha256_hex(bundle->bytes, bundle->length, bundle->hash);
}
static void open_database(const char *path, const Bundle *bundle, sqlite3 **db)
{ assert(wena_sqlite_open(path, bundle->bytes, bundle->length, bundle->hash, db)); }
static void execute(sqlite3 *db, const char *sql)
{ assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK); }
static int number(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *s; int value;
    assert(sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK); assert(sqlite3_step(s) == SQLITE_ROW);
    value = sqlite3_column_int(s, 0); assert(sqlite3_finalize(s) == SQLITE_OK); return value;
}
static void bind_scope(sqlite3_stmt *s)
{
    int count;
    count = sqlite3_bind_parameter_count(s);
    if (count >= 1) assert(sqlite3_bind_text(s, 1, "board", -1, SQLITE_STATIC) == SQLITE_OK);
    if (count >= 2) assert(sqlite3_bind_text(s, 2, "selected", -1, SQLITE_STATIC) == SQLITE_OK);
    if (count >= 3) assert(sqlite3_bind_text(s, 3, "actor", -1, SQLITE_STATIC) == SQLITE_OK);
}
static Stats measure(sqlite3 *db, const char *query)
{
    Stats result; sqlite3_stmt *s; int step; const unsigned char *detail; char explain[512];
    memset(&result, 0, sizeof(result)); sprintf(explain, "EXPLAIN QUERY PLAN %s", query);
    assert(sqlite3_prepare_v2(db, explain, -1, &s, NULL) == SQLITE_OK); bind_scope(s);
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        detail = sqlite3_column_text(s, 3); assert(detail);
        if (strstr((const char *)detail, "checklist_items_card_order_idx")) result.indexed = 1;
        if (strstr((const char *)detail, "SEARCH")) result.searched = 1;
    }
    assert(step == SQLITE_DONE); assert(sqlite3_finalize(s) == SQLITE_OK);
    assert(sqlite3_prepare_v2(db, query, -1, &s, NULL) == SQLITE_OK); bind_scope(s);
    while ((step = sqlite3_step(s)) == SQLITE_ROW) {
        ++result.rows;
        if (sqlite3_column_count(s) == 9 && sqlite3_column_text(s, 0) &&
            strcmp((const char *)sqlite3_column_text(s, 0), "malformed") == 0) ++result.malformed;
    }
    assert(step == SQLITE_DONE);
    result.steps = sqlite3_stmt_status(s, SQLITE_STMTSTATUS_VM_STEP, 0);
    result.scans = sqlite3_stmt_status(s, SQLITE_STMTSTATUS_FULLSCAN_STEP, 0);
    result.sorts = sqlite3_stmt_status(s, SQLITE_STMTSTATUS_SORT, 0);
    assert(sqlite3_finalize(s) == SQLITE_OK); return result;
}
static void seed(sqlite3 *db, int version)
{
    execute(db, "INSERT INTO actors VALUES('actor','Actor',1);INSERT INTO boards VALUES('board','Board',1);"
        "INSERT INTO swimlanes VALUES('lane','board','Lane',0,1);INSERT INTO lists VALUES('list','board','List',0,1);"
        "INSERT INTO cards VALUES('selected','board','lane','list','Selected',0,0,7)");
    if (version >= 3) {
        execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('selected-check','board','selected','Selected',0)");
        execute(db, "WITH RECURSIVE n(x) AS(VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<8) "
            "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
            "SELECT printf('selected-item-%d',x),'board','selected','selected-check','Item',x FROM n");
    }
}
static void unrelated(sqlite3 *db, int first, int last)
{
    char query[1024];
    execute(db, "BEGIN");
    sprintf(query, "WITH RECURSIVE n(x) AS(VALUES(%d) UNION ALL SELECT x+1 FROM n WHERE x<%d) "
        "INSERT OR IGNORE INTO boards SELECT printf('board-%%d',(x-1)/120),'Unrelated',1 FROM n", first, last); execute(db, query);
    execute(db, "INSERT OR IGNORE INTO swimlanes SELECT 'lane-'||id,id,'Lane',0,1 FROM boards WHERE id!='board'");
    execute(db, "INSERT OR IGNORE INTO lists SELECT 'list-'||id,id,'List',0,1 FROM boards WHERE id!='board'");
    sprintf(query, "WITH RECURSIVE n(x) AS(VALUES(%d) UNION ALL SELECT x+1 FROM n WHERE x<%d) "
        "INSERT INTO cards SELECT printf('card-%%d',x),printf('board-%%d',(x-1)/120),printf('lane-board-%%d',(x-1)/120),"
        "printf('list-board-%%d',(x-1)/120),'Unrelated',x,0,1 FROM n", first, last); execute(db, query);
    sprintf(query, "WITH RECURSIVE n(x) AS(VALUES(%d) UNION ALL SELECT x+1 FROM n WHERE x<%d) "
        "INSERT INTO checklists(id,board_id,card_id,title,position) SELECT printf('check-%%d',x),printf('board-%%d',(x-1)/120),"
        "printf('card-%%d',x),'Unrelated',0 FROM n", first, last); execute(db, query);
    sprintf(query, "WITH RECURSIVE n(x) AS(VALUES(%d) UNION ALL SELECT x+1 FROM n WHERE x<%d) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) SELECT printf('item-%%d',x),printf('board-%%d',(x-1)/120),"
        "printf('card-%%d',x),printf('check-%%d',x),'Unrelated',0 FROM n", first, last); execute(db, query);
    execute(db, "COMMIT");
}
static int failure_mode, failure_seen;
static int authorize(void *context, int action, const char *first, const char *second,
    const char *database, const char *trigger)
{
    (void)context; (void)second; (void)database; (void)trigger;
    if (first && ((failure_mode == 1 && action == SQLITE_CREATE_INDEX && strcmp(first, "checklist_items_card_order_idx") == 0) ||
        (failure_mode == 2 && action == SQLITE_INSERT && strcmp(first, "schema_migrations") == 0) ||
        (failure_mode == 3 && action == SQLITE_TRANSACTION && strcmp(first, "COMMIT") == 0))) {
        failure_seen = 1; return SQLITE_DENY;
    }
    return SQLITE_OK;
}
static int extension(sqlite3 *db, char **error, const sqlite3_api_routines *api)
{ (void)error; (void)api; return sqlite3_set_authorizer(db, authorize, NULL); }
static int space(void *context, const char *path, unsigned long *bytes)
{ (void)context; (void)path; *bytes = (unsigned long)-1; return 1; }
typedef struct Life { sqlite3 *db; int stops, starts, fail_start; } Life;
static int stop(void *context)
{ Life *life; life = (Life *)context; ++life->stops; if (life->db) { assert(sqlite3_close(life->db) == SQLITE_OK); life->db = NULL; } return 1; }
static int start(void *context, const char *path)
{
    Life *life; life = (Life *)context; ++life->starts;
    if (life->fail_start) { life->fail_start = 0; return 0; }
    return sqlite3_open_v2(path, &life->db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK;
}
int main(int argc, char **argv)
{
    Bundle bundles[4], source; Stats base, before, three, after, grown, other; sqlite3 *db;
    char path[512], backup[512], live[512], hash[65]; int old, mode, stops; size_t index;
    Life life; WenaRestoreLifecycle lifecycle;
    static const char *scoped[] = {
        "SELECT id,title,position,version,hide_checked_items,hide_all_items,show_on_minicard,created_at,updated_at,board_id FROM checklists WHERE card_id=?2 ORDER BY position,id",
        "SELECT id,board_id,swimlane_id,list_id,title,position,archived,version FROM cards WHERE board_id=?1 ORDER BY position,id",
        "SELECT id,board_id,title,position,version FROM lists WHERE board_id=?1 ORDER BY position,id",
        "SELECT id,board_id,title,position,version FROM swimlanes WHERE board_id=?1 ORDER BY position,id",
        "SELECT version FROM cards WHERE board_id=?1 AND id=?2 AND archived=0 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)"
    };
    assert(argc == 6);
    printf("Query-work regression uses C SQLite %s (%s)\n", sqlite3_libversion(), sqlite3_sourceid());
    for (index = 0; index < 4; ++index) { read_bundle(argv[index + 1], &bundles[index]); assert(wena_sqlite_migration_target(bundles[index].hash) == (int)index + 1); }
    sprintf(path, "%s/performance.sqlite", argv[5]); open_database(path, &bundles[2], &db); seed(db, 3);
    base = measure(db, item_query); unrelated(db, 1, 12000); before = measure(db, item_query);
    assert(base.rows == 8 && before.rows == 8 && before.scans > 11000 && before.steps > base.steps * 50);
    execute(db, "CREATE INDEX candidate_three ON checklist_items(card_id,checklist_id,position)"); three = measure(db, item_query);
    assert(three.rows == 8 && three.scans == 0 && three.sorts > 0); execute(db, "DROP INDEX candidate_three");
    assert(sqlite3_close(db) == SQLITE_OK); open_database(path, &bundles[3], &db); after = measure(db, item_query);
    assert(after.rows == 8 && after.indexed && after.searched && after.scans == 0 && after.sorts == 0);
    assert(after.steps < 1024 && after.steps * 50 < before.steps);
    for (index = 0; index < sizeof(scoped) / sizeof(scoped[0]); ++index) {
        other = measure(db, scoped[index]); assert(other.rows == 1 && other.searched && other.scans == 0 && other.steps < 256);
    }
    unrelated(db, 12001, 24000); grown = measure(db, item_query);
    assert(grown.rows == 8 && grown.indexed && grown.searched && grown.scans == 0 && grown.sorts == 0);
    assert(grown.steps <= after.steps + 20 && grown.steps < 1024);
    printf("8 selected items: v3 %d VM steps/%d fullscan; three-column %d steps/%d sorts; v4 %d steps, after doubling unrelated rows %d steps\n",
        before.steps, before.scans, three.steps, three.sorts, after.steps, grown.steps);
    execute(db, "PRAGMA foreign_keys=OFF;INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "VALUES('malformed','wrong-board','selected','selected-check','Must remain visible to validator',9)");
    other = measure(db, item_query); assert(other.rows == 9 && other.malformed == 1);
    execute(db, "DELETE FROM checklist_items WHERE id='malformed';PRAGMA foreign_keys=ON");
    assert(number(db, "SELECT version FROM cards WHERE id='selected'") == 7);
    assert(sqlite3_close(db) == SQLITE_OK); assert(!wena_sqlite_open(path, bundles[2].bytes, bundles[2].length, bundles[2].hash, &db));
    open_database(path, &bundles[3], &db); grown = measure(db, item_query); assert(grown.indexed && grown.rows == 8);
    sprintf(backup, "%s/current-backup.sqlite", argv[5]); assert(wena_sqlite_backup_create(db, backup, space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
    memset(&life, 0, sizeof(life)); lifecycle.stop = stop; lifecycle.start = start; lifecycle.context = &life;
    sprintf(live, "%s/current-live.sqlite", argv[5]); open_database(live, &bundles[3], &life.db);
    assert(wena_sqlite_restore(backup, live, bundles[3].hash, space, NULL, &lifecycle)); grown = measure(life.db, item_query);
    assert(grown.indexed && grown.rows == 8 && grown.steps < 1024); assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL;
    for (old = 1; old <= 3; ++old) {
        sprintf(path, "%s/legacy-%d.sqlite", argv[5], old); open_database(path, &bundles[old - 1], &db); seed(db, old);
        sprintf(backup, "%s/legacy-backup-%d.sqlite", argv[5], old); assert(wena_sqlite_backup_create(db, backup, space, NULL)); assert(sqlite3_close(db) == SQLITE_OK);
        read_bundle(backup, &source); strcpy(hash, source.hash); free(source.bytes);
        sprintf(live, "%s/legacy-live-%d.sqlite", argv[5], old); open_database(live, &bundles[3], &life.db);
        execute(life.db, "INSERT INTO boards VALUES('sentinel','Keep',1)"); stops = life.stops;
        failure_mode = 1; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_restore(backup, live, bundles[3].hash, space, NULL, &lifecycle)); sqlite3_reset_auto_extension(); failure_mode = 0;
        assert(failure_seen && life.stops == stops && number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        life.fail_start = 1; assert(!wena_sqlite_restore(backup, live, bundles[3].hash, space, NULL, &lifecycle));
        assert(number(life.db, "SELECT count(*) FROM boards WHERE id='sentinel'") == 1);
        assert(wena_sqlite_restore(backup, live, bundles[3].hash, space, NULL, &lifecycle));
        assert(wena_sqlite_schema_version(life.db) == 4 && number(life.db, "SELECT version FROM cards WHERE id='selected'") == 7);
        if (old == 3) { other = measure(life.db, item_query); assert(other.indexed && other.rows == 8); }
        assert(sqlite3_close(life.db) == SQLITE_OK); life.db = NULL; read_bundle(backup, &source); assert(strcmp(hash, source.hash) == 0); free(source.bytes);
        open_database(path, &bundles[3], &db); assert(wena_sqlite_schema_version(db) == 4); assert(sqlite3_close(db) == SQLITE_OK);
    }
    for (mode = 1; mode <= 3; ++mode) {
        sprintf(path, "%s/index-rollback-%d.sqlite", argv[5], mode); open_database(path, &bundles[2], &db); seed(db, 3); assert(sqlite3_close(db) == SQLITE_OK);
        failure_mode = mode; failure_seen = 0; assert(sqlite3_auto_extension((void (*)(void))extension) == SQLITE_OK);
        assert(!wena_sqlite_open(path, bundles[3].bytes, bundles[3].length, bundles[3].hash, &db)); sqlite3_reset_auto_extension(); failure_mode = 0; assert(failure_seen);
        open_database(path, &bundles[2], &db); assert(number(db, "SELECT count(*) FROM sqlite_master WHERE name='checklist_items_card_order_idx'") == 0);
        assert(number(db, "SELECT count(*) FROM checklist_items") == 8); assert(sqlite3_close(db) == SQLITE_OK);
        open_database(path, &bundles[3], &db); other = measure(db, item_query); assert(other.indexed && other.rows == 8); assert(sqlite3_close(db) == SQLITE_OK);
    }
    sprintf(path, "%s/altered-index.sqlite", argv[5]); open_database(path, &bundles[3], &db);
    execute(db, "DROP INDEX checklist_items_card_order_idx;CREATE INDEX checklist_items_card_order_idx ON checklist_items(checklist_id,position)");
    assert(!wena_sqlite_schema_validate(db, bundles[3].hash)); assert(sqlite3_close(db) == SQLITE_OK);
    assert(!wena_sqlite_open(path, bundles[3].bytes, bundles[3].length, bundles[3].hash, &db));
    for (index = 0; index < 4; ++index) free(bundles[index].bytes);
    puts("schema-v4 bounded indexed query, upgrade and restore tests passed"); return 0;
}
