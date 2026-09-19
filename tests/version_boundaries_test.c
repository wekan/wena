#include "../client/features/card_mutation.h"
#include "../client/features/card_description_mutation.h"
#include "../client/features/hierarchy_mutation.h"
#include "../client/features/hierarchy_move_mutation.h"
#include "../client/features/checklist_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3 *db, const char *query)
{
    assert(sqlite3_exec(db, query, NULL, NULL, NULL) == SQLITE_OK);
}

static sqlite3_int64 number(sqlite3 *db, const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(db, query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    assert(sqlite3_column_type(statement, 0) == SQLITE_INTEGER);
    result = sqlite3_column_int64(statement, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return result;
}

static void fixture(sqlite3 *db, const char *path)
{
    FILE *file;
    char *text;
    long length;
    file = fopen(path, "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); text = (char *)malloc((size_t)length + 1); assert(text);
    assert(fread(text, 1, (size_t)length, file) == (size_t)length);
    assert(fclose(file) == 0); text[length] = 0; sql(db, text); free(text);
}

static void version(sqlite3 *db, const char *table, const char *id,
                    unsigned long value)
{
    char query[160];
    sprintf(query, "UPDATE %s SET version=%lu WHERE id='%s'", table, value, id);
    sql(db, query);
}

static void reset(sqlite3 *db)
{
    sql(db, "DELETE FROM idempotency_keys;DELETE FROM checklist_items;"
        "DELETE FROM checklists;DELETE FROM card_descriptions;DELETE FROM cards;"
        "DELETE FROM lists;DELETE FROM swimlanes;DELETE FROM boards;DELETE FROM actors;"
        "INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);"
        "INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('s2','b','Other',1,1);"
        "INSERT INTO lists VALUES('l','b','List',0,1),('l2','b','Other',1,1);");
    sql(db, "INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1),"
        "('c2','b','s','l','Sibling',1,0,1),('arch','b','s','l','Archived',2,1,1);"
        "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('k','b','c','Checklist',0);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "VALUES('i','b','c','k','Item',0);");
}

static void command(WenaDomainCommand *c, WenaDomainOperation operation,
                    const char *body)
{
    memset(c, 0, sizeof(*c)); c->operation = operation; c->request_version = 7;
    strcpy(c->route, "/b/b/native"); strcpy(c->user_id, "u");
    strcpy(c->form_body, body); c->form_body_length = strlen(body);
}

static void reject(WenaSqlitePersistence *adapter, WenaDomainCommand *c)
{
    WenaRegionResponse response;
    const unsigned char *bytes;
    size_t index;
    sqlite3_int64 keys;
    int changes;
    changes = sqlite3_total_changes(adapter->database);
    keys = number(adapter->database, "SELECT count(*) FROM idempotency_keys");
    memset(&response, 85, sizeof(response));
    assert(!wena_sqlite_persistence_apply(adapter, c, &response));
    bytes = (const unsigned char *)&response;
    for (index = 0; index < sizeof(response); ++index) assert(bytes[index] == 0);
    assert(sqlite3_total_changes(adapter->database) == changes);
    assert(number(adapter->database, "SELECT count(*) FROM idempotency_keys") == keys);
    assert(sqlite3_get_autocommit(adapter->database));
}

static void domain_boundaries(sqlite3 *db, WenaSqliteBoardSnapshot *snapshot)
{
    static const struct {
        WenaDomainOperation operation;
        const char *table, *id, *body;
    } cases[] = {
        {WENA_DOMAIN_EDIT_CARD_TITLE, "cards", "c", "cardId=c&title=Changed"},
        {WENA_DOMAIN_ARCHIVE_CARD, "cards", "c", "cardId=c"},
        {WENA_DOMAIN_RESTORE_CARD, "cards", "arch", "cardId=arch"},
        {WENA_DOMAIN_EDIT_CARD_DESCRIPTION, "cards", "c", "cardId=c&description=Changed"},
        {WENA_DOMAIN_EDIT_BOARD_TITLE, "boards", "b", "title=Changed"},
        {WENA_DOMAIN_EDIT_LIST_TITLE, "lists", "l", "listId=l&title=Changed"},
        {WENA_DOMAIN_EDIT_SWIMLANE_TITLE, "swimlanes", "s", "swimlaneId=s&title=Changed"},
        {WENA_DOMAIN_MOVE_CARD, "cards", "c", "cardId=c&targetListId=l2&targetSwimlaneId=s2"},
        {WENA_DOMAIN_MOVE_LIST, "lists", "l", "listId=l&targetPosition=1"},
        {WENA_DOMAIN_MOVE_SWIMLANE, "swimlanes", "s", "swimlaneId=s&targetPosition=1"}
    };
    WenaSqlitePersistence adapter;
    WenaDomainCommand c;
    WenaRegionResponse response;
    char body[512], query[160];
    size_t index;
    wena_sqlite_persistence_init(&adapter, db);
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        reset(db);
        version(db, "lists", "l2", WENA_VERSION_READ_MAX);
        version(db, "swimlanes", "s2", WENA_VERSION_READ_MAX);
        version(db, "cards", "c2", WENA_VERSION_READ_MAX);
        version(db, cases[index].table, cases[index].id, WENA_VERSION_MUTATE_MAX);
        sprintf(body, "%s&expectedVersion=%lu", cases[index].body, WENA_VERSION_MUTATE_MAX);
        command(&c, cases[index].operation, body);
        assert(wena_sqlite_persistence_apply(&adapter, &c, &response));
        assert(response.regions[0].version == WENA_VERSION_READ_MAX);
        sprintf(query, "SELECT version FROM %s WHERE id='%s'", cases[index].table, cases[index].id);
        assert(number(db, query) == (sqlite3_int64)WENA_VERSION_READ_MAX);
        assert(wena_sqlite_board_load(db, "b", snapshot));
        assert(number(db, "SELECT version FROM lists WHERE id='l2'") == (sqlite3_int64)WENA_VERSION_READ_MAX);
        assert(number(db, "SELECT version FROM swimlanes WHERE id='s2'") == (sqlite3_int64)WENA_VERSION_READ_MAX);
        assert(number(db, "SELECT version FROM cards WHERE id='c2'") == (sqlite3_int64)WENA_VERSION_READ_MAX);
        assert(number(db, "SELECT count(*) FROM idempotency_keys") == 1);
        sprintf(body, "%s&expectedVersion=%lu", cases[index].body, WENA_VERSION_READ_MAX);
        command(&c, cases[index].operation, body); c.request_version = 8;
        reject(&adapter, &c);
        sprintf(body, "%s&expectedVersion=%lu", cases[index].body, (unsigned long)LONG_MAX);
        command(&c, cases[index].operation, body); reject(&adapter, &c);
    }
}

static void checklist_boundaries(sqlite3 *db, WenaChecklistSnapshot *snapshot)
{
    static const struct { WenaDomainOperation operation; const char *fields; } cases[] = {
        {WENA_DOMAIN_CREATE_CHECKLIST, "title=Created"},
        {WENA_DOMAIN_RENAME_CHECKLIST, "title=Changed"},
        {WENA_DOMAIN_ADD_CHECKLIST_ITEM, "title=Added"},
        {WENA_DOMAIN_RENAME_CHECKLIST_ITEM, "title=Changed"},
        {WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED, "isFinished=1"},
        {WENA_DOMAIN_SET_CHECKLIST_FLAGS, "hideChecked=1&hideAll=0&showOnMinicard=-1"},
        {WENA_DOMAIN_DELETE_CHECKLIST, ""},
        {WENA_DOMAIN_DELETE_CHECKLIST_ITEM, ""},
        {WENA_DOMAIN_ADD_CHECKLIST_ITEMS, "titles=First%0ASecond"}
    };
    WenaSqlitePersistence persistence;
    WenaChecklistMutation adapter;
    WenaDomainCommand c;
    WenaRegionResponse response;
    char body[768];
    size_t index;
    wena_sqlite_persistence_init(&persistence, db);
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        reset(db);
        version(db, "cards", "c", WENA_VERSION_MUTATE_MAX);
        version(db, "checklists", "k", WENA_VERSION_MUTATE_MAX);
        version(db, "checklist_items", "i", WENA_VERSION_MUTATE_MAX);
        sprintf(body, "cardId=c&checklistId=k&itemId=i&expectedVersion=%lu&"
            "expectedChecklistVersion=%lu&expectedItemVersion=%lu%s%s",
            WENA_VERSION_MUTATE_MAX, WENA_VERSION_MUTATE_MAX, WENA_VERSION_MUTATE_MAX,
            cases[index].fields[0] ? "&" : "", cases[index].fields);
        command(&c, cases[index].operation, body);
        assert(wena_sqlite_persistence_apply(&persistence, &c, &response));
        assert(response.regions[0].version == WENA_VERSION_READ_MAX);
        assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
        assert(snapshot->card_version == WENA_VERSION_READ_MAX);
        if (cases[index].operation != WENA_DOMAIN_DELETE_CHECKLIST) {
            assert(snapshot->checklist_count >= 1);
            assert(snapshot->checklist_versions[0] <= WENA_VERSION_READ_MAX);
        }
        sprintf(body, "cardId=c&checklistId=k&itemId=i&expectedVersion=%lu&"
            "expectedChecklistVersion=%lu&expectedItemVersion=%lu%s%s",
            WENA_VERSION_READ_MAX, WENA_VERSION_READ_MAX, WENA_VERSION_READ_MAX,
            cases[index].fields[0] ? "&" : "", cases[index].fields);
        command(&c, cases[index].operation, body); c.request_version = 8;
        reject(&persistence, &c);
    }
    /* A terminal parent/item rejects even when the aggregate card can advance. */
    reset(db); version(db, "checklists", "k", WENA_VERSION_READ_MAX);
    sprintf(body, "cardId=c&checklistId=k&expectedVersion=1&expectedChecklistVersion=%lu&title=Added", WENA_VERSION_READ_MAX);
    command(&c, WENA_DOMAIN_ADD_CHECKLIST_ITEM, body); reject(&persistence, &c);
    version(db, "checklists", "k", 1); version(db, "checklist_items", "i", WENA_VERSION_READ_MAX);
    sprintf(body, "cardId=c&checklistId=k&itemId=i&expectedVersion=1&expectedChecklistVersion=1&expectedItemVersion=%lu&isFinished=1", WENA_VERSION_READ_MAX);
    command(&c, WENA_DOMAIN_SET_CHECKLIST_ITEM_FINISHED, body); reject(&persistence, &c);
}

static void adapters(sqlite3 *db, WenaSqliteBoardSnapshot *snapshot,
                     WenaChecklistSnapshot *checklists)
{
    WenaCardMutation cards;
    WenaCardDescriptionMutation description;
    WenaHierarchyMutation titles;
    WenaHierarchyMoveMutation moves;
    WenaChecklistMutation checklist;
    WenaChecklistEdit edit;
    char text[WENA_DESCRIPTION_CAPACITY];
    unsigned long actual, position;
    int changes;
    reset(db);
    version(db, "cards", "c", WENA_VERSION_MUTATE_MAX);
    version(db, "cards", "c2", WENA_VERSION_READ_MAX);
    version(db, "cards", "arch", WENA_VERSION_READ_MAX);
    assert(wena_sqlite_board_load(db, "b", snapshot));
    assert(wena_card_mutation_init(&cards, db, "u", "b", snapshot->cards, snapshot->card_count));
    assert(wena_card_mutation_reorder_request(&cards, "b", "c", WENA_VERSION_MUTATE_MAX, 10, 2));
    assert(number(db, "SELECT position FROM cards WHERE id='c'") == 2);
    assert(number(db, "SELECT version FROM cards WHERE id='c2'") == (sqlite3_int64)WENA_VERSION_READ_MAX);
    assert(number(db, "SELECT version FROM cards WHERE id='arch'") == (sqlite3_int64)WENA_VERSION_READ_MAX);
    assert(wena_card_mutation_load(&cards, "b", "c", text, sizeof(text), &actual));
    assert(actual == WENA_VERSION_READ_MAX);
    changes = sqlite3_total_changes(db);
    assert(!wena_card_mutation_save_request(&cards, "b", "c", actual, 11, "Card"));
    assert(!wena_card_mutation_archive_request(&cards, "b", "c", actual, 11));
    assert(!wena_card_mutation_move_request(&cards, "b", "c", actual, 11, "l2", "s2"));
    assert(!wena_card_mutation_reorder_request(&cards, "b", "c", actual, 11, 2));
    assert(!wena_card_mutation_restore_request(&cards, "b", "arch", actual, 11));
    assert(wena_card_description_mutation_init(&description, db, "u", "b"));
    assert(wena_card_description_mutation_load(&description, "b", "c", text, sizeof(text), &actual));
    assert(actual == WENA_VERSION_READ_MAX && !text[0]);
    assert(!wena_card_description_mutation_save_request(&description, "b", "c", actual, 11, ""));
    assert(sqlite3_total_changes(db) == changes);
    version(db, "boards", "b", WENA_VERSION_READ_MAX);
    version(db, "lists", "l", WENA_VERSION_READ_MAX);
    version(db, "swimlanes", "s", WENA_VERSION_READ_MAX);
    assert(wena_hierarchy_mutation_init(&titles, db, "u", "b", snapshot));
    assert(wena_hierarchy_move_mutation_init(&moves, db, "u", "b", snapshot));
    changes = sqlite3_total_changes(db);
    assert(wena_hierarchy_mutation_load(&titles, "b", WENA_HIERARCHY_BOARD, "b", text, sizeof(text), &actual));
    assert(actual == WENA_VERSION_READ_MAX);
    assert(!wena_hierarchy_mutation_save_request(&titles, "b", WENA_HIERARCHY_BOARD, "b", actual, 11, "Board"));
    assert(wena_hierarchy_mutation_load(&titles, "b", WENA_HIERARCHY_LIST, "l", text, sizeof(text), &actual));
    assert(!wena_hierarchy_mutation_save_request(&titles, "b", WENA_HIERARCHY_LIST, "l", actual, 11, "List"));
    assert(wena_hierarchy_move_mutation_load(&moves, "b", WENA_HIERARCHY_LIST, "l", &actual, &position));
    assert(actual == WENA_VERSION_READ_MAX && position == 0);
    assert(!wena_hierarchy_move_mutation_move_request(&moves, "b", WENA_HIERARCHY_LIST, "l", actual, 11, 0));
    assert(wena_hierarchy_mutation_load(&titles, "b", WENA_HIERARCHY_SWIMLANE, "s", text, sizeof(text), &actual));
    assert(!wena_hierarchy_mutation_save_request(&titles, "b", WENA_HIERARCHY_SWIMLANE, "s", actual, 11, "Lane"));
    assert(wena_hierarchy_move_mutation_load(&moves, "b", WENA_HIERARCHY_SWIMLANE, "s", &actual, &position));
    assert(!wena_hierarchy_move_mutation_move_request(&moves, "b", WENA_HIERARCHY_SWIMLANE, "s", actual, 11, 1));
    assert(sqlite3_total_changes(db) == changes);
    version(db, "checklists", "k", WENA_VERSION_READ_MAX);
    version(db, "checklist_items", "i", WENA_VERSION_READ_MAX);
    assert(wena_checklist_mutation_init(&checklist, db, "u", "b"));
    assert(wena_checklist_mutation_load(&checklist, "b", "c", checklists));
    assert(checklists->card_version == WENA_VERSION_READ_MAX && checklists->checklist_versions[0] == WENA_VERSION_READ_MAX && checklists->item_versions[0] == WENA_VERSION_READ_MAX);
    memset(&edit, 0, sizeof(edit)); edit.action = WENA_CHECKLIST_RENAME;
    edit.expected_card_version = edit.expected_checklist_version = WENA_VERSION_READ_MAX;
    edit.checklist_id = "k"; edit.title = "Checklist";
    changes = sqlite3_total_changes(db);
    assert(!wena_checklist_mutation_save_request(&checklist, "b", "c", &edit, 11));
    assert(sqlite3_total_changes(db) == changes);
}

static void rollback(sqlite3 *db)
{
    WenaSqlitePersistence adapter;
    WenaDomainCommand c;
    WenaRegionResponse response;
    char body[512];
    reset(db); version(db, "cards", "c", WENA_VERSION_MUTATE_MAX);
    version(db, "checklists", "k", WENA_VERSION_MUTATE_MAX);
    sql(db, "CREATE TRIGGER fail_request BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'injected'); END");
    wena_sqlite_persistence_init(&adapter, db);
    sprintf(body, "cardId=c&checklistId=k&expectedVersion=%lu&expectedChecklistVersion=%lu&title=Rollback", WENA_VERSION_MUTATE_MAX, WENA_VERSION_MUTATE_MAX);
    command(&c, WENA_DOMAIN_ADD_CHECKLIST_ITEM, body);
    assert(!wena_sqlite_persistence_apply(&adapter, &c, &response));
    assert(response.region_count == 0 && response.request_version == 0);
    assert(number(db, "SELECT count(*) FROM checklist_items") == 1);
    assert(number(db, "SELECT version FROM cards WHERE id='c'") == (sqlite3_int64)WENA_VERSION_MUTATE_MAX);
    assert(number(db, "SELECT version FROM checklists WHERE id='k'") == (sqlite3_int64)WENA_VERSION_MUTATE_MAX);
    assert(number(db, "SELECT count(*) FROM idempotency_keys") == 0);
    assert(sqlite3_get_autocommit(db)); sql(db, "DROP TRIGGER fail_request");
    assert(wena_sqlite_persistence_apply(&adapter, &c, &response));
    assert(response.regions[0].version == WENA_VERSION_READ_MAX);
    reject(&adapter, &c);
}

static void invalid_readers(sqlite3 *db, WenaSqliteBoardSnapshot *snapshot)
{
    WenaCardMutation adapter;
    char text[129];
    unsigned long actual;
    reset(db); assert(wena_card_mutation_init(&adapter, db, "u", "b", NULL, 0));
    version(db, "cards", "c", (unsigned long)LONG_MAX);
    strcpy(text, "unchanged"); actual = 77;
    assert(!wena_card_mutation_load(&adapter, "b", "c", text, sizeof(text), &actual));
    assert(!strcmp(text, "unchanged") && actual == 77);
    assert(!wena_sqlite_board_load(db, "b", snapshot));
    sql(db, "UPDATE cards SET version=1.5 WHERE id='c'");
    assert(!wena_card_mutation_load(&adapter, "b", "c", text, sizeof(text), &actual));
    assert(!strcmp(text, "unchanged") && actual == 77);
    assert(!wena_sqlite_board_load(db, "b", snapshot));
    version(db, "cards", "c", 1);
    version(db, "boards", "b", (unsigned long)LONG_MAX);
    assert(!wena_sqlite_board_load(db, "b", snapshot));
}

int main(int argc, char **argv)
{
    sqlite3 *db;
    WenaSqliteBoardSnapshot *snapshot;
    WenaChecklistSnapshot *checklists;
    WenaChecklistMutation adapter;
    assert(argc == 5);
    assert(WENA_VERSION_MUTATE_MAX + 1UL == WENA_VERSION_READ_MAX);
    assert(WENA_VERSION_READ_MAX < (unsigned long)LONG_MAX);
    assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    fixture(db, argv[1]); fixture(db, argv[2]); fixture(db, argv[3]);
    sql(db, "PRAGMA foreign_keys=ON");
    snapshot = (WenaSqliteBoardSnapshot *)calloc(1, sizeof(*snapshot)); assert(snapshot);
    checklists = wena_checklist_snapshot_create(); assert(checklists);
    domain_boundaries(db, snapshot); checklist_boundaries(db, checklists);
    adapters(db, snapshot, checklists); invalid_readers(db, snapshot); rollback(db);
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    assert(wena_sqlite_board_load(db, "b", snapshot));
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", checklists));
    assert(checklists->card_version == WENA_VERSION_READ_MAX);
    assert(checklists->checklist_versions[0] == WENA_VERSION_READ_MAX && checklists->item_count == 2);
    assert(number(db, "SELECT count(*) FROM idempotency_keys") == 1);
    assert(sqlite3_close(db) == SQLITE_OK);
    free(snapshot); wena_checklist_snapshot_free(checklists);
    puts("entity version boundaries: final increment, terminal reads, sibling preservation, rollback and reopen passed");
    return 0;
}
