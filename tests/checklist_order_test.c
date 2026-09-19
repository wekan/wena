#include "../client/features/checklist_mutation.h"
#include "../models/checklist_item_titles.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3 *db, const char *query)
{
    char *error;
    int result;
    error = NULL;
    result = sqlite3_exec(db, query, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "%s: %s\n", query, error);
    sqlite3_free(error);
    assert(result == SQLITE_OK);
}
static int number(sqlite3 *db, const char *query)
{
    sqlite3_stmt *statement;
    int result;
    assert(sqlite3_prepare_v2(db, query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    result = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return result;
}
static void schema(sqlite3 *db, const char *path)
{
    FILE *file;
    long length;
    char *text;
    file = fopen(path, "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END)); length = ftell(file); assert(length > 0);
    rewind(file); text = (char *)malloc((size_t)length + 1u); assert(text);
    assert(fread(text, 1, (size_t)length, file) == (size_t)length);
    text[length] = 0; assert(!fclose(file)); sql(db, text); free(text);
}

static void edit(WenaChecklistEdit *change, const WenaChecklistSnapshot *snapshot,
    const char *checklist, const char *item, unsigned long target)
{
    size_t index;
    memset(change, 0, sizeof(*change));
    change->action = item ? WENA_CHECKLIST_REORDER_ITEM : WENA_CHECKLIST_REORDER;
    change->checklist_id = checklist; change->item_id = item;
    change->expected_card_version = snapshot->card_version;
    for (index = 0; index < snapshot->checklist_count; ++index)
        if (!strcmp(snapshot->checklists[index].id, checklist))
            change->expected_checklist_version = snapshot->checklist_versions[index];
    if (item) for (index = 0; index < snapshot->item_count; ++index)
        if (!strcmp(snapshot->items[index].id, item))
            change->expected_item_version = snapshot->item_versions[index];
    change->target_position = target;
}

static void unchanged(WenaChecklistMutation *adapter, WenaChecklistEdit *change,
    unsigned long request, int success)
{
    WenaChecklistSnapshot *before, *after;
    int keys;
    before = wena_checklist_snapshot_create(); after = wena_checklist_snapshot_create();
    assert(before && after);
    assert(wena_checklist_mutation_load(adapter, "b", "c", before));
    keys = number(adapter->persistence.database, "SELECT count(*) FROM idempotency_keys");
    assert(wena_checklist_mutation_save_request(adapter, "b", "c", change, request) == success);
    assert(wena_checklist_mutation_load(adapter, "b", "c", after));
    assert(!memcmp(before, after, sizeof(*before)));
    assert(keys == number(adapter->persistence.database, "SELECT count(*) FROM idempotency_keys"));
    assert(sqlite3_get_autocommit(adapter->persistence.database));
    wena_checklist_snapshot_free(before); wena_checklist_snapshot_free(after);
}

static void signature(sqlite3 *db, char *output)
{
    sqlite3_stmt *statement;
    const char *text;
    assert(sqlite3_prepare_v2(db,
        "SELECT group_concat(value,'|') FROM (SELECT 'c'||id||':'||version AS value FROM cards "
        "UNION ALL SELECT 'k'||id||':'||position||':'||version||':'||updated_at FROM checklists "
        "UNION ALL SELECT 'i'||id||':'||position||':'||version||':'||updated_at FROM checklist_items "
        "UNION ALL SELECT 'r'||actor_id||':'||operation||':'||request_version FROM idempotency_keys "
        "ORDER BY value)", -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    text = (const char *)sqlite3_column_text(statement, 0);
    assert(text && strlen(text) < 4096); strcpy(output, text);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
}

static void corrupted(WenaChecklistMutation *adapter, WenaChecklistEdit *change,
    const char *break_query, const char *repair_query)
{
    char before[4096], after[4096];
    sqlite3 *db;
    db = adapter->persistence.database;
    sql(db, "PRAGMA ignore_check_constraints=ON"); sql(db, break_query);
    signature(db, before);
    assert(!wena_checklist_mutation_save_request(adapter, "b", "c", change, 99));
    signature(db, after); assert(!strcmp(before, after));
    sql(db, repair_query); sql(db, "PRAGMA ignore_check_constraints=OFF");
}

int main(int argc, char **argv)
{
    sqlite3 *db, *second;
    WenaChecklistMutation adapter, other;
    WenaChecklistSnapshot *snapshot, *saved;
    WenaChecklistEdit change, stale;
    WenaDomainCommand command;
    WenaRegionResponse response;
    unsigned long version;
    char query[512];
    size_t index;
    assert(argc == 5); assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    sql(db, "PRAGMA foreign_keys=ON");
    schema(db, argv[1]); schema(db, argv[2]); schema(db, argv[3]);
    sql(db, "INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);"
        "INSERT INTO boards VALUES('foreign','Foreign',1);"
        "INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);"
        "INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);"
        "INSERT INTO cards VALUES('c2','b','s','l','Other',1,0,1)");
    sql(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES"
        "('cl0','b','c','First',0),('cl1','b','c','Second',3),"
        "('cl2','b','c','Third',2147483647),('other','b','c2','Other',0)");
    sql(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES"
        "('it0','b','c','cl0','First',0),('it1','b','c','cl0','Second',2),"
        "('it2','b','c','cl0','Third',5),('it3','b','c','cl0','Fourth',2147483647),"
        "('untouched','b','c','cl1','Other checklist',17),('foreign-item','b','c2','other','Other card',0)");
    snapshot = wena_checklist_snapshot_create(); saved = wena_checklist_snapshot_create();
    assert(snapshot && saved); assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    /* The raw operation rejects malformed stored sibling fields even for a
     * requested no-op, leaving every position/version/timestamp/key unchanged. */
    edit(&change, snapshot, "cl0", "it1", 1);
    corrupted(&adapter, &change, "UPDATE checklists SET title=char(1) WHERE id='cl1'",
        "UPDATE checklists SET title='Second' WHERE id='cl1'");
    corrupted(&adapter, &change, "UPDATE checklists SET hide_checked_items=2 WHERE id='cl1'",
        "UPDATE checklists SET hide_checked_items=0 WHERE id='cl1'");
    corrupted(&adapter, &change, "UPDATE checklists SET hide_all_items=1.5 WHERE id='cl1'",
        "UPDATE checklists SET hide_all_items=0 WHERE id='cl1'");
    corrupted(&adapter, &change, "UPDATE checklists SET show_on_minicard=2 WHERE id='cl1'",
        "UPDATE checklists SET show_on_minicard=NULL WHERE id='cl1'");
    corrupted(&adapter, &change, "UPDATE checklists SET created_at=10,updated_at=9 WHERE id='cl1'",
        "UPDATE checklists SET created_at=0,updated_at=0 WHERE id='cl1'");
    corrupted(&adapter, &change, "UPDATE checklists SET updated_at='bad' WHERE id='cl1'",
        "UPDATE checklists SET updated_at=0 WHERE id='cl1'");
    corrupted(&adapter, &change, "UPDATE checklist_items SET title=CAST(X'C0AF' AS TEXT) WHERE id='untouched'",
        "UPDATE checklist_items SET title='Other checklist' WHERE id='untouched'");
    corrupted(&adapter, &change, "UPDATE checklist_items SET title=char(128) WHERE id='untouched'",
        "UPDATE checklist_items SET title='Other checklist' WHERE id='untouched'");
    corrupted(&adapter, &change, "UPDATE checklist_items SET is_finished=2 WHERE id='untouched'",
        "UPDATE checklist_items SET is_finished=0 WHERE id='untouched'");
    corrupted(&adapter, &change, "UPDATE checklist_items SET created_at=-1 WHERE id='untouched'",
        "UPDATE checklist_items SET created_at=0 WHERE id='untouched'");
    corrupted(&adapter, &change, "UPDATE checklist_items SET created_at=2,updated_at=1 WHERE id='untouched'",
        "UPDATE checklist_items SET created_at=0,updated_at=0 WHERE id='untouched'");
    corrupted(&adapter, &change, "UPDATE checklist_items SET updated_at=0.5 WHERE id='untouched'",
        "UPDATE checklist_items SET updated_at=0 WHERE id='untouched'");
    /* Current ordinal is an exact guarded no-op; sparse positions stay sparse. */
    edit(&change, snapshot, "cl0", "it1", 1); unchanged(&adapter, &change, 1, 1);
    edit(&change, snapshot, "cl2", NULL, 2); unchanged(&adapter, &change, 1, 1);
    /* On a separate card [0,10,MAX] yields free block[1,3], overlapping final
     * [0,2]. Ascending final writes must vacate each destination in time. */
    sql(db, "INSERT INTO cards VALUES('c3','b','s','l','Staging',2,0,1);"
        "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('stage','b','c3','Stage',0)");
    sql(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES"
        "('st0','b','c3','stage','Start',0),('st1','b','c3','stage','Middle',10),"
        "('st2','b','c3','stage','End',2147483647)");
    memset(&change, 0, sizeof(change)); change.action = WENA_CHECKLIST_REORDER_ITEM;
    change.checklist_id = "stage"; change.item_id = "st2";
    change.expected_card_version = change.expected_checklist_version = change.expected_item_version = 1;
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c3", &change, 999));
    assert(number(db, "SELECT group_concat(id,',')='st2,st0,st1' FROM "
        "(SELECT id FROM checklist_items WHERE card_id='c3' ORDER BY position)") == 1);
    assert(number(db, "SELECT max(position)=2 AND min(position)=0 AND min(version)=2 "
        "FROM checklist_items WHERE card_id='c3'") == 1);
    sql(db, "DELETE FROM checklist_items WHERE card_id='c3';DELETE FROM checklists WHERE card_id='c3';DELETE FROM cards WHERE id='c3'");
    /* Move maximum-position item to start, compact all selected-checklist rows. */
    edit(&change, snapshot, "cl0", "it3", 0); stale = change;
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 1));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(snapshot->card_version == 2 && snapshot->checklist_versions[0] == 2);
    assert(!strcmp(snapshot->items[0].id, "it3") && !strcmp(snapshot->items[1].id, "it0") &&
        !strcmp(snapshot->items[2].id, "it1") && !strcmp(snapshot->items[3].id, "it2"));
    for (index = 0; index < 4; ++index) assert(snapshot->items[index].position == index);
    assert(snapshot->item_versions[0] == 2 && snapshot->item_versions[1] == 2 &&
        snapshot->item_versions[2] == 1 && snapshot->item_versions[3] == 2);
    assert(snapshot->items[4].position == 17 && snapshot->item_versions[4] == 1);
    assert(number(db, "SELECT version FROM cards WHERE id='c2'") == 1);
    unchanged(&adapter, &stale, 20, 0);
    edit(&change, snapshot, "cl0", "it3", 3); unchanged(&adapter, &change, 1, 0);
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 2));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!strcmp(snapshot->items[3].id, "it3") && snapshot->card_version == 3);
    edit(&change, snapshot, "cl2", NULL, 0);
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 1));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!strcmp(snapshot->checklists[0].id, "cl2") &&
        !strcmp(snapshot->checklists[1].id, "cl0") &&
        !strcmp(snapshot->checklists[2].id, "cl1"));
    for (index = 0; index < 3; ++index) assert(snapshot->checklists[index].position == index);
    edit(&change, snapshot, "cl2", NULL, 2);
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 2));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!strcmp(snapshot->checklists[2].id, "cl2"));
    /* A selected object's revision, card aggregate, bounds and scope all guard
     * no-op and changed targets before any transient order is published. */
    edit(&change, snapshot, "cl0", "it0", 2);
    --change.expected_card_version; unchanged(&adapter, &change, 20, 0); ++change.expected_card_version;
    --change.expected_checklist_version; unchanged(&adapter, &change, 20, 0); ++change.expected_checklist_version;
    --change.expected_item_version; unchanged(&adapter, &change, 20, 0); ++change.expected_item_version;
    change.item_id = "missing"; unchanged(&adapter, &change, 20, 0);
    change.item_id = "foreign-item"; unchanged(&adapter, &change, 20, 0);
    change.item_id = "untouched"; unchanged(&adapter, &change, 20, 0); change.item_id = "it0";
    change.checklist_id = "other"; unchanged(&adapter, &change, 20, 0); change.checklist_id = "cl0";
    change.target_position = 4; unchanged(&adapter, &change, 20, 0);
    change.target_position = 1024; unchanged(&adapter, &change, 20, 0); change.target_position = 2;
    assert(!wena_checklist_mutation_save(&adapter, "foreign", "c", &change));
    assert(!wena_checklist_mutation_save(&adapter, "b", "c2", &change));
    assert(wena_checklist_mutation_init(&other, db, "unknown", "b"));
    assert(!wena_checklist_mutation_save(&other, "b", "c", &change));
    sql(db, "UPDATE cards SET archived=1 WHERE id='c'");
    assert(!wena_checklist_mutation_save(&adapter, "b", "c", &change));
    sql(db, "UPDATE cards SET archived=0 WHERE id='c'");
    /* Reject late final-position, aggregate and metadata updates, checking the
     * complete snapshot and metadata count after every rollback. */
    sql(db, "CREATE TRIGGER reject_final BEFORE UPDATE ON checklist_items WHEN NEW.version>OLD.version "
        "AND NEW.position=2 BEGIN SELECT RAISE(ABORT,'final'); END");
    unchanged(&adapter, &change, 20, 0); sql(db, "DROP TRIGGER reject_final");
    sql(db, "CREATE TRIGGER reject_parent BEFORE UPDATE ON checklists BEGIN SELECT RAISE(ABORT,'parent'); END");
    unchanged(&adapter, &change, 20, 0); sql(db, "DROP TRIGGER reject_parent");
    sql(db, "CREATE TRIGGER reject_card BEFORE UPDATE ON cards BEGIN SELECT RAISE(ABORT,'card'); END");
    unchanged(&adapter, &change, 20, 0); sql(db, "DROP TRIGGER reject_card");
    sql(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END");
    unchanged(&adapter, &change, 20, 0); sql(db, "DROP TRIGGER reject_metadata");
    /* Full ownership scan refuses children corrupted outside the selected list. */
    sql(db, "PRAGMA foreign_keys=OFF;UPDATE checklist_items SET board_id='foreign' WHERE id='untouched'");
    assert(!wena_checklist_mutation_save(&adapter, "b", "c", &change));
    sql(db, "UPDATE checklist_items SET board_id='b' WHERE id='untouched';PRAGMA foreign_keys=ON");
    sql(db, "PRAGMA foreign_keys=OFF;UPDATE checklist_items SET checklist_id='missing' WHERE id='untouched'");
    assert(!wena_checklist_mutation_save(&adapter, "b", "c", &change));
    sql(db, "UPDATE checklist_items SET checklist_id='cl1' WHERE id='untouched';PRAGMA foreign_keys=ON");
    /* A genuine concurrent sibling mutation increments the card boundary. */
    stale = change; assert(sqlite3_open(argv[4], &second) == SQLITE_OK);
    sql(second, "UPDATE checklist_items SET title='Concurrent',version=version+1 WHERE id='untouched';"
        "UPDATE cards SET version=version+1 WHERE id='c'"); assert(sqlite3_close(second) == SQLITE_OK);
    unchanged(&adapter, &stale, 20, 0);
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    edit(&change, snapshot, "cl0", "it0", 2);
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 20));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot)); *saved = *snapshot;
    assert(sqlite3_close(db) == SQLITE_OK); assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    sql(db, "PRAGMA foreign_keys=ON"); assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!memcmp(saved, snapshot, sizeof(*saved)));
    edit(&change, snapshot, "cl0", "it0", 0); unchanged(&adapter, &change, 20, 0);
    /* Duplicate rows must be rejected even for an otherwise no-op ordinal.
     * A test-only view exposes duplicates that the real UNIQUE schema forbids. */
    sql(db, "PRAGMA foreign_keys=OFF;ALTER TABLE checklist_items RENAME TO saved_items;"
        "CREATE VIEW checklist_items AS SELECT * FROM saved_items UNION ALL "
        "SELECT * FROM saved_items WHERE id='it0'");
    edit(&change, snapshot, "cl0", "it0", 2);
    assert(!wena_checklist_mutation_save(&adapter, "b", "c", &change));
    sql(db, "DROP VIEW checklist_items;ALTER TABLE saved_items RENAME TO checklist_items;PRAGMA foreign_keys=ON");
    /* Raw malformed ordinals and duplicate fields cannot reach storage. */
    memset(&command, 0, sizeof(command)); command.operation = WENA_DOMAIN_REORDER_CHECKLIST;
    strcpy(command.user_id, "u"); strcpy(command.route, "/b/b/native"); command.request_version = 80;
    sprintf(command.form_body, "cardId=c&expectedVersion=%lu&checklistId=cl0&expectedChecklistVersion=%lu&targetPosition=-1",
        snapshot->card_version, snapshot->checklist_versions[0]); command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    strcat(command.form_body, "&targetPosition=0"); command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    /* Bounds include maximum collections; hidden children remain ordered. */
    sql(db, "WITH RECURSIVE n(x) AS (SELECT 3 UNION ALL SELECT x+1 FROM n WHERE x<63) "
        "INSERT INTO checklists(id,board_id,card_id,title,position) SELECT 'list-'||x,'b','c','Extra',x FROM n");
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot) && snapshot->checklist_count == 64);
    edit(&change, snapshot, "cl0", NULL, 63);
    assert(wena_checklist_mutation_save(&adapter, "b", "c", &change));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!strcmp(snapshot->checklists[63].id, "cl0"));
    sql(db, "WITH RECURSIVE n(x) AS (SELECT 4 UNION ALL SELECT x+1 FROM n WHERE x<1022) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "SELECT 'extra-'||x,'b','c','cl0','Extra',x FROM n;UPDATE checklists SET hide_all_items=1 WHERE id='cl0'");
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot) && snapshot->item_count == 1024);
    edit(&change, snapshot, "cl0", "it0", 1022);
    assert(wena_checklist_mutation_save(&adapter, "b", "c", &change));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!strcmp(snapshot->items[1022].id, "it0"));
    sql(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "VALUES('overflow','b','c','cl0','Overflow',1023)");
    edit(&change, snapshot, "cl0", "it0", 0);
    assert(!wena_checklist_mutation_save(&adapter, "b", "c", &change));
    sql(db, "DELETE FROM checklist_items WHERE id='overflow'");
    /* A terminal sibling version blocks only a move that changes its position. */
    version = snapshot->item_versions[0];
    sprintf(query, "UPDATE checklist_items SET version=%lu WHERE id='%s'", WENA_VERSION_READ_MAX, snapshot->items[0].id); sql(db, query);
    assert(!wena_checklist_mutation_save(&adapter, "b", "c", &change));
    sprintf(query, "UPDATE checklist_items SET version=%lu WHERE id='%s'", version, snapshot->items[0].id); sql(db, query);
    wena_checklist_snapshot_free(snapshot); wena_checklist_snapshot_free(saved);
    assert(sqlite3_close(db) == SQLITE_OK);
    puts("Checklist and item ordering: scoped no-op,staging,versions,rollback,replay,limits,reopen passed");
    return 0;
}
