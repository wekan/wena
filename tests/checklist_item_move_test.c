#include "../client/features/checklist_mutation.h"
#include "../models/checklist_item_titles.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WENA_TEST_CROSS_BOARD
#define TARGET_BOARD "destination"
#define TARGET_LIST "dl"
#define TARGET_LANE "ds"
#define TARGET_ARGUMENT TARGET_BOARD
#else
#define TARGET_BOARD "b"
#define TARGET_LIST "l"
#define TARGET_LANE "s"
#define TARGET_ARGUMENT NULL
#endif

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

static void change_for(WenaChecklistEdit *edit, sqlite3 *db)
{
    memset(edit, 0, sizeof(*edit));
    edit->target_board_id = TARGET_ARGUMENT;
    edit->action = WENA_CHECKLIST_MOVE_ITEM;
    edit->item_id = "i1"; edit->target_checklist_id = "dest";
    edit->expected_item_version = (unsigned long)number(db, "SELECT version FROM checklist_items WHERE id='i1'");
    edit->expected_target_checklist_version = (unsigned long)number(db, "SELECT version FROM checklists WHERE id='dest'");
    edit->checklist_id = "cl"; edit->target_card_id = "dst";
    edit->expected_card_version = (unsigned long)number(db, "SELECT version FROM cards WHERE id='src'");
    edit->expected_target_card_version = (unsigned long)number(db, "SELECT version FROM cards WHERE id='dst'");
    edit->expected_checklist_version = (unsigned long)number(db, "SELECT version FROM checklists WHERE id='cl'");
}

/* Serialize all logical rows, including metadata and flags, so a rejected
 * operation cannot hide an unrelated partial update behind a selected snapshot. */
static char *dump(sqlite3 *db)
{
    sqlite3_stmt *statement;
    const char *tables[] = {"cards", "checklists", "checklist_items", "idempotency_keys"};
    char query[128], *output;
    const unsigned char *value;
    size_t used, length, table;
    int step, column;
    output = (char *)calloc(1, 1024u * 1024u); assert(output); used = 0;
    for (table = 0; table < 4; ++table) {
        sprintf(query, "SELECT * FROM %s ORDER BY 1,2,3", tables[table]);
        assert(sqlite3_prepare_v2(db, query, -1, &statement, NULL) == SQLITE_OK);
        while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
            for (column = 0; column < sqlite3_column_count(statement); ++column) {
                value = sqlite3_column_text(statement, column);
                length = (size_t)sqlite3_column_bytes(statement, column);
                assert(used + length + 4 < 1024u * 1024u);
                output[used++] = (char)('0' + sqlite3_column_type(statement, column));
                if (length) { memcpy(output + used, value, length); used += length; }
                output[used++] = '|';
            }
            output[used++] = '\n';
        }
        assert(step == SQLITE_DONE); assert(sqlite3_finalize(statement) == SQLITE_OK);
    }
    return output;
}

static void rejected(WenaChecklistMutation *adapter, WenaChecklistEdit *edit,
    unsigned long request)
{
    char *before, *after;
    sqlite3 *db;
    db = adapter->persistence.database;
    before = dump(db);
    assert(!wena_checklist_mutation_save_request(adapter, "b", "src", edit, request));
    after = dump(db); assert(!strcmp(before, after)); free(before); free(after);
    assert(sqlite3_get_autocommit(db));
    assert(number(db, "PRAGMA defer_foreign_keys") == 0);
}

int main(int argc, char **argv)
{
    sqlite3 *db, *second;
    WenaChecklistMutation adapter, unknown, target_adapter;
    WenaChecklistSnapshot *source, *target;
    WenaChecklistEdit edit, stale;
    WenaDomainCommand command;
    WenaRegionResponse response;
    char query[256], *before, *after;
    size_t index;
    const char *triggers[] = {
        "CREATE TRIGGER failure BEFORE UPDATE ON checklist_items BEGIN SELECT RAISE(ABORT,'item'); END",
        "CREATE TRIGGER failure BEFORE UPDATE ON checklist_items BEGIN SELECT RAISE(IGNORE); END",
        "CREATE TRIGGER failure BEFORE UPDATE ON checklists WHEN OLD.id='dest' BEGIN SELECT RAISE(ABORT,'destination parent'); END",
        "CREATE TRIGGER failure BEFORE UPDATE ON cards WHEN OLD.id='dst' BEGIN SELECT RAISE(ABORT,'destination card'); END",
        "CREATE TRIGGER failure BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END"
    };
    assert(argc == 5); assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    sql(db, "PRAGMA foreign_keys=ON"); schema(db, argv[1]); schema(db, argv[2]); schema(db, argv[3]);
    sql(db, "INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);"
        "INSERT INTO boards VALUES('foreign','Other',1);"
        "INSERT INTO boards VALUES('destination','Destination',1);"
        "INSERT INTO lists VALUES('dl','destination','List',0,1);"
        "INSERT INTO swimlanes VALUES('ds','destination','Lane',0,1);"
        "INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);"
        "INSERT INTO lists VALUES('fl','foreign','List',0,1);INSERT INTO swimlanes VALUES('fs','foreign','Lane',0,1)");
    sql(db, "INSERT INTO cards VALUES('src','b','s','l','Source',0,0,1);"
        "INSERT INTO cards VALUES('dst','" TARGET_BOARD "','" TARGET_LANE "','" TARGET_LIST "','Destination',1,0,1);"
        "INSERT INTO cards VALUES('other','foreign','fs','fl','Other',0,0,1)");
    sql(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES"
        "('cl','b','src','Source list',2),('keep','b','src','Other list',9),"
        "('dest','" TARGET_BOARD "','dst','Destination',5),('foreign-list','foreign','other','Other',0)");
    sql(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES"
        "('i1','b','src','cl','Suomi \303\244',2,1),('i2','b','src','cl','Keep sibling',18,0),"
        "('di','" TARGET_BOARD "','dst','dest','Existing',7,0)");
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_init(&target_adapter, db, "u", TARGET_BOARD));
    source = wena_checklist_snapshot_create(); target = wena_checklist_snapshot_create(); assert(source && target);
    change_for(&edit, db);
    edit.target_board_id = ""; rejected(&adapter, &edit, 1);
    edit.target_board_id = "missing"; rejected(&adapter, &edit, 1);
    edit.target_board_id = "b&targetBoardId=destination"; rejected(&adapter, &edit, 1);
    change_for(&edit, db);

    edit.target_checklist_id = "cl"; rejected(&adapter, &edit, 1);
    edit.target_checklist_id = "missing"; rejected(&adapter, &edit, 1);
    edit.target_checklist_id = "keep"; rejected(&adapter, &edit, 1);
    edit.target_checklist_id = "foreign-list"; edit.target_card_id = "other"; rejected(&adapter, &edit, 1);
    change_for(&edit, db); edit.target_card_id = "missing"; rejected(&adapter, &edit, 1);
    change_for(&edit, db); edit.item_id = "di"; rejected(&adapter, &edit, 1);
    change_for(&edit, db); edit.item_id = "missing"; rejected(&adapter, &edit, 1);
    change_for(&edit, db); ++edit.expected_card_version; rejected(&adapter, &edit, 1);
    change_for(&edit, db); ++edit.expected_checklist_version; rejected(&adapter, &edit, 1);
    change_for(&edit, db); ++edit.expected_item_version; rejected(&adapter, &edit, 1);
    change_for(&edit, db); ++edit.expected_target_card_version; rejected(&adapter, &edit, 1);
    change_for(&edit, db); ++edit.expected_target_checklist_version; rejected(&adapter, &edit, 1);
    change_for(&edit, db); edit.expected_target_checklist_version = WENA_VERSION_READ_MAX; rejected(&adapter, &edit, 1);
    change_for(&edit, db);
    assert(wena_checklist_mutation_init(&unknown, db, "unknown", "b")); rejected(&unknown, &edit, 1);
    sql(db, "UPDATE cards SET archived=1 WHERE id='dst'"); rejected(&adapter, &edit, 1);
    sql(db, "UPDATE cards SET archived=0 WHERE id='dst';UPDATE cards SET archived=1 WHERE id='src'");
    rejected(&adapter, &edit, 1); sql(db, "UPDATE cards SET archived=0 WHERE id='src'");
    for (index = 0; index < sizeof(triggers)/sizeof(triggers[0]); ++index) {
        sql(db, triggers[index]); rejected(&adapter, &edit, 1); sql(db, "DROP TRIGGER failure");
    }
    sql(db, "UPDATE checklist_items SET title=char(1) WHERE id='di'"); rejected(&adapter, &edit, 1);
    sql(db, "UPDATE checklist_items SET title='Existing' WHERE id='di'");
    sql(db, "PRAGMA foreign_keys=OFF;UPDATE checklist_items SET card_id='src' WHERE id='di'");
    rejected(&adapter, &edit, 1); sql(db, "UPDATE checklist_items SET card_id='dst' WHERE id='di';PRAGMA foreign_keys=ON");
    sql(db, "UPDATE checklist_items SET position=2147483647 WHERE id='di'"); rejected(&adapter, &edit, 1);
    sql(db, "UPDATE checklist_items SET position=7 WHERE id='di'");
    /* Across cards, the combined target must remain within its read capacity. */
    sql(db, "WITH RECURSIVE n(x) AS(SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<1023) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "SELECT 'extra'||x,'" TARGET_BOARD "','dst','dest','Extra',x+100 FROM n");
    rejected(&adapter, &edit, 1); sql(db, "DELETE FROM checklist_items WHERE id LIKE 'extra%'");
    stale = edit; assert(sqlite3_open(argv[4], &second) == SQLITE_OK);
    sql(second, "UPDATE checklist_items SET title='Concurrent',version=version+1 WHERE id='di';"
        "UPDATE checklists SET version=version+1 WHERE id='dest';UPDATE cards SET version=version+1 WHERE id='dst'");
    assert(sqlite3_close(second) == SQLITE_OK); rejected(&adapter, &stale, 1); change_for(&edit, db);
    memset(&command, 0, sizeof(command)); command.operation = WENA_DOMAIN_MOVE_CHECKLIST_ITEM;
    command.request_version = 100; strcpy(command.user_id, "u"); strcpy(command.route, "/b/b/native");
    strcpy(command.form_body, "cardId=src&expectedVersion=1&checklistId=cl&expectedChecklistVersion=1&itemId=i1&expectedItemVersion=1&targetCardId=dst&expectedTargetVersion=2&targetChecklistId=dest&expectedTargetChecklistVersion=2&targetChecklistId=cl");
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    /* Explicit invalid/duplicate destination board fields must never fall back
     * to the source board or leave any partial writes behind. */
    *strrchr(command.form_body, '&') = 0;
#ifdef WENA_TEST_CROSS_BOARD
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
#endif
    strcat(command.form_body, "&targetBoardId=");
    command.form_body_length = strlen(command.form_body);
    before = dump(db);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    strcat(command.form_body, TARGET_BOARD "&targetBoardId=" TARGET_BOARD);
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    after = dump(db); assert(!strcmp(before, after)); free(before); free(after);
    assert(wena_checklist_mutation_save_request(&adapter, "b", "src", &edit, 1));
    assert(wena_checklist_mutation_load(&adapter, "b", "src", source));
    assert(wena_checklist_mutation_load(&target_adapter, TARGET_BOARD, "dst", target));
    assert(source->card_version == 2 && source->item_count == 1 && source->items[0].position == 18);
    assert(source->checklist_versions[0] == 2 && source->checklist_versions[1] == 1);
    assert(target->card_version == 3 && target->item_count == 2 && target->checklist_versions[0] == 3);
    assert(!strcmp(target->items[1].id, "i1") && target->items[1].position == 8 && target->items[1].is_finished);
    assert(!strcmp(target->items[1].title, "Suomi \303\244") && target->item_versions[1] == 2);
    assert(number(db, "SELECT count(*) FROM pragma_foreign_key_check") == 0);
    before = dump(db); assert(sqlite3_close(db) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &db) == SQLITE_OK); sql(db, "PRAGMA foreign_keys=ON");
    after = dump(db); assert(!strcmp(before, after)); free(before); free(after);
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_init(&target_adapter, db, "u", TARGET_BOARD));
    edit.target_board_id = "b";
    edit.target_card_id = "src"; edit.target_checklist_id = "keep"; edit.checklist_id = "dest";
    edit.expected_card_version = 3; edit.expected_target_card_version = 2;
    edit.expected_checklist_version = 3; edit.expected_target_checklist_version = 1; edit.expected_item_version = 2;
#ifndef WENA_TEST_CROSS_BOARD
    assert(!wena_checklist_mutation_save_request(&adapter, "b", "dst", &edit, 1));
#endif
    assert(wena_checklist_mutation_save_request(&target_adapter, TARGET_BOARD, "dst", &edit, 2));
    /* Same-card transfer is possible at full capacity; its aggregate advances once. */
    sql(db, "WITH RECURSIVE n(x) AS(SELECT 1 UNION ALL SELECT x+1 FROM n WHERE x<1022) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "SELECT 'extra'||x,'b','src','cl','Extra',x+100 FROM n");
    edit.checklist_id = "keep"; edit.target_checklist_id = "cl";
    edit.expected_card_version = edit.expected_target_card_version = 3;
    edit.expected_checklist_version = edit.expected_target_checklist_version = 2;
    edit.expected_item_version = 3;
    assert(wena_checklist_mutation_save_request(&adapter, "b", "src", &edit, 3));
    assert(wena_checklist_mutation_load(&adapter, "b", "src", source));
    assert(source->card_version == 4 && source->item_count == 1024);
    assert(number(db, "SELECT version FROM checklists WHERE id='keep'") == 3);
    assert(number(db, "SELECT version FROM checklists WHERE id='cl'") == 3);
    /* Terminal selected/parent revisions remain refused without wraparound. */
    edit.checklist_id = "cl"; edit.target_checklist_id = "keep";
    edit.expected_card_version = edit.expected_target_card_version = 4;
    edit.expected_checklist_version = edit.expected_target_checklist_version = 3;
    edit.expected_item_version = 4;
    sprintf(query, "UPDATE checklist_items SET version=%lu WHERE id='i1'", WENA_VERSION_READ_MAX);
    sql(db, query); rejected(&adapter, &edit, 4);
    sql(db, "UPDATE checklist_items SET version=4 WHERE id='i1'");
    assert(wena_checklist_mutation_save_request(&adapter, "b", "src", &edit, 4));
    assert(number(db, "SELECT position FROM checklist_items WHERE id='i1'") == 0);
    wena_checklist_snapshot_free(source); wena_checklist_snapshot_free(target);
    assert(sqlite3_close(db) == SQLITE_OK);
    puts("Checklist item transfer: same/cross-card scope, capacity, revisions, rollback, replay and reopen passed");
    return 0;
}
