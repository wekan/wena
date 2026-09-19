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
static void edit(WenaChecklistEdit *edit, WenaChecklistSnapshot *snapshot,
    const char *text)
{
    memset(edit, 0, sizeof(*edit));
    edit->action = WENA_CHECKLIST_ADD_ITEMS;
    edit->checklist_id = "cl";
    edit->expected_card_version = snapshot->card_version;
    edit->expected_checklist_version = snapshot->checklist_versions[0];
    edit->batch_text = text; edit->batch_length = strlen(text);
}
static void rejected(WenaChecklistMutation *adapter, WenaChecklistEdit *edit)
{
    sqlite3 *db;
    int count, card, checklist, keys;
    db = adapter->persistence.database;
    count = number(db, "SELECT count(*) FROM checklist_items");
    card = number(db, "SELECT version FROM cards WHERE id='c'");
    checklist = number(db, "SELECT version FROM checklists WHERE id='cl'");
    keys = number(db, "SELECT count(*) FROM idempotency_keys");
    assert(!wena_checklist_mutation_save_request(adapter, "b", "c", edit, 100));
    assert(count == number(db, "SELECT count(*) FROM checklist_items"));
    assert(card == number(db, "SELECT version FROM cards WHERE id='c'"));
    assert(checklist == number(db, "SELECT version FROM checklists WHERE id='cl'"));
    assert(keys == number(db, "SELECT count(*) FROM idempotency_keys"));
    assert(sqlite3_get_autocommit(db));
}

int main(int argc, char **argv)
{
    sqlite3 *db, *second;
    WenaChecklistMutation adapter, other;
    WenaChecklistSnapshot *snapshot, *before;
    WenaChecklistEdit change;
    WenaDomainCommand command;
    WenaRegionResponse response;
    char maximum[WENA_CHECKLIST_BATCH_MAX_BYTES + 2u], query[512];
    char identities[8][65];
    char parsed[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    char untouched[WENA_CHECKLIST_BATCH_MAX_ITEMS][WENA_CHECKLIST_TITLE_CAPACITY];
    size_t parsed_count;
    size_t index, offset;
    const char *invalid[] = {"", " \t\r\n ", "A\n\300\257", "A\nB\001", "A\nB\tC",
        "A\nB\rC", "A\n\302\200", "1\n2\n3\n4\n5\n6\n7\n8\n9", "A\nB\177"};
    assert(argc == 5);
    memset(parsed, 73, sizeof(parsed)); memcpy(untouched, parsed, sizeof(parsed));
    parsed_count = 10;
    assert(!wena_checklist_item_batch_parse("Valid\nBad\001", 10, parsed, &parsed_count));
    assert(!parsed_count && !memcmp(parsed, untouched, sizeof(parsed)));
    assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    sql(db, "PRAGMA foreign_keys=ON");
    schema(db, argv[1]); schema(db, argv[2]); schema(db, argv[3]);
    sql(db, "INSERT INTO actors VALUES('u','User',1);INSERT INTO actors VALUES('v','Other',1);"
        "INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('other','Other',1);"
        "INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);"
        "INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);"
        "INSERT INTO cards VALUES('c2','b','s','l','Other',1,0,1)");
    sql(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('cl','b','c','Checklist',0);"
        "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('cl2','b','c2','Other',0)");
    snapshot = wena_checklist_snapshot_create(); before = wena_checklist_snapshot_create();
    assert(snapshot && before);
    assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    /* Validate the complete batch before any write; existing parser semantics
     * trim and preserve duplicates and original append order. */
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        edit(&change, snapshot, invalid[index]); rejected(&adapter, &change);
    }
    memset(maximum, 'x', 129); maximum[129] = 0;
    edit(&change, snapshot, maximum); rejected(&adapter, &change);
    edit(&change, snapshot, "One"); change.batch_length = 4; rejected(&adapter, &change);
    edit(&change, snapshot, "\t First \r\n\n \302\240Second\302\240 \nFirst");
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 1));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(snapshot->item_count == 3 && snapshot->card_version == 2 &&
        snapshot->checklist_versions[0] == 2);
    assert(!strcmp(snapshot->items[0].title, "First") &&
        !strcmp(snapshot->items[1].title, "Second") &&
        !strcmp(snapshot->items[2].title, "First"));
    for (index = 0; index < 3; ++index)
        assert(snapshot->items[index].position == index && snapshot->item_versions[index] == 1);
    assert(number(db, "SELECT count(*) FROM idempotency_keys") == 1);
    /* Same request never inserts twice, even with refreshed versions/content. */
    edit(&change, snapshot, "Replay");
    assert(!wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 1));
    /* Maximum8x128 bytes including all escaped reserved chars fits unchanged
     * transport capacity and is accepted as one version/metadata update. */
    offset = 0;
    for (index = 0; index < 8; ++index) {
        memset(maximum + offset, index % 2 ? '&' : '%', 128); offset += 128;
        if (index < 7) maximum[offset++] = '\n';
    }
    maximum[offset] = 0; assert(offset == WENA_CHECKLIST_BATCH_MAX_BYTES);
    edit(&change, snapshot, maximum);
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 2));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(snapshot->item_count == 11 && snapshot->card_version == 3 &&
        snapshot->checklist_versions[0] == 3);
    for (index = 0; index < 8; ++index) {
        assert(strlen(snapshot->items[index + 3].title) == 128);
        strcpy(identities[index], snapshot->items[index + 3].id);
        if (index) assert(strcmp(identities[index], identities[index - 1]));
    }
    maximum[offset++] = '\n'; maximum[offset] = 0;
    edit(&change, snapshot, maximum); rejected(&adapter, &change);
    edit(&change, snapshot, "One");
    --change.expected_card_version; rejected(&adapter, &change); ++change.expected_card_version;
    --change.expected_checklist_version; rejected(&adapter, &change); ++change.expected_checklist_version;
    change.checklist_id = "cl2"; rejected(&adapter, &change); change.checklist_id = "missing";
    rejected(&adapter, &change); change.checklist_id = "cl";
    assert(!wena_checklist_mutation_save(&adapter, "other", "c", &change));
    assert(!wena_checklist_mutation_save(&adapter, "b", "c2", &change));
    assert(wena_checklist_mutation_init(&other, db, "unknown", "b")); rejected(&other, &change);
    sql(db, "UPDATE cards SET archived=1 WHERE id='c'"); rejected(&adapter, &change);
    sql(db, "UPDATE cards SET archived=0 WHERE id='c'");
    /* Late child, parent and metadata failures roll back every prior row. */
    change.batch_text = "Before\nReject\nAfter"; change.batch_length = strlen(change.batch_text);
    sql(db, "CREATE TRIGGER reject_child BEFORE INSERT ON checklist_items WHEN NEW.title='Reject' "
        "BEGIN SELECT RAISE(ABORT,'child'); END"); rejected(&adapter, &change);
    sql(db, "DROP TRIGGER reject_child;CREATE TRIGGER reject_parent BEFORE UPDATE ON checklists "
        "BEGIN SELECT RAISE(ABORT,'parent'); END"); rejected(&adapter, &change);
    sql(db, "DROP TRIGGER reject_parent;CREATE TRIGGER reject_card BEFORE UPDATE ON cards "
        "BEGIN SELECT RAISE(ABORT,'card'); END"); rejected(&adapter, &change);
    sql(db, "DROP TRIGGER reject_card;CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'metadata'); END"); rejected(&adapter, &change);
    sql(db, "DROP TRIGGER reject_metadata");
    /* Position overflow is preflighted before a rejecting write trigger fires. */
    sql(db, "UPDATE checklist_items SET position=2147483647 WHERE position=10");
    rejected(&adapter, &change);
    sql(db, "UPDATE checklist_items SET position=10 WHERE position=2147483647");
    sql(db, "PRAGMA foreign_keys=OFF;UPDATE checklist_items SET board_id='other' WHERE position=10");
    rejected(&adapter, &change); sql(db, "UPDATE checklist_items SET board_id='b' WHERE position=10;PRAGMA foreign_keys=ON");
    /* Fresh connection changes card version and invalidates the entire batch. */
    assert(sqlite3_open(argv[4], &second) == SQLITE_OK);
    sql(second, "UPDATE cards SET version=version+1 WHERE id='c'");
    assert(sqlite3_close(second) == SQLITE_OK); rejected(&adapter, &change);
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    edit(&change, snapshot, "One");
    assert(wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 3));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot) && snapshot->item_count == 12);
    *before = *snapshot;
    /* Raw commands enforce decoding, UTF-8 and duplicate-field rejection. */
    memset(&command, 0, sizeof(command)); command.operation = WENA_DOMAIN_ADD_CHECKLIST_ITEMS;
    command.request_version = 50; strcpy(command.user_id, "u"); strcpy(command.route, "/b/b/native");
    sprintf(command.form_body, "cardId=c&expectedVersion=%lu&checklistId=cl&expectedChecklistVersion=%lu&titles=A%%0AB%%00", snapshot->card_version, snapshot->checklist_versions[0]);
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    sprintf(command.form_body, "cardId=c&expectedVersion=%lu&checklistId=cl&expectedChecklistVersion=%lu&titles=A&titles=B", snapshot->card_version, snapshot->checklist_versions[0]);
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    sprintf(command.form_body, "cardId=c&expectedVersion=%lu&checklistId=cl&expectedChecklistVersion=%lu&titles=%%C0%%AF", snapshot->card_version, snapshot->checklist_versions[0]);
    command.form_body_length = strlen(command.form_body);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &command, &response));
    assert(sqlite3_close(db) == SQLITE_OK); assert(sqlite3_open(argv[4], &db) == SQLITE_OK);
    sql(db, "PRAGMA foreign_keys=ON"); assert(wena_checklist_mutation_init(&adapter, db, "u", "b"));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    assert(!memcmp(before, snapshot, sizeof(*snapshot)));
    for (index = 0; index < 8; ++index) assert(!strcmp(identities[index], snapshot->items[index + 3].id));
    edit(&change, snapshot, "Replay reopened");
    assert(!wena_checklist_mutation_save_request(&adapter, "b", "c", &change, 2));
    /* Actor partitions request IDs. */
    assert(wena_checklist_mutation_init(&other, db, "v", "b"));
    assert(wena_checklist_mutation_save_request(&other, "b", "c", &change, 2));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot));
    /* Complete-card cap counts all checklists, including hidden children. */
    sql(db, "WITH RECURSIVE n(x) AS (SELECT 13 UNION ALL SELECT x+1 FROM n WHERE x<1022) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "SELECT 'extra-'||x,'b','c','cl','Extra',x FROM n");
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot) && snapshot->item_count == 1023);
    edit(&change, snapshot, "A\nB"); rejected(&adapter, &change);
    edit(&change, snapshot, "Last"); assert(wena_checklist_mutation_save(&adapter, "b", "c", &change));
    assert(wena_checklist_mutation_load(&adapter, "b", "c", snapshot) && snapshot->item_count == 1024);
    edit(&change, snapshot, "Full"); rejected(&adapter, &change);
    sprintf(query, "UPDATE cards SET version=%lu WHERE id='c'", (unsigned long)LONG_MAX - 1UL); sql(db, query);
    change.expected_card_version = (unsigned long)LONG_MAX - 1UL; rejected(&adapter, &change);
    wena_checklist_snapshot_free(snapshot); wena_checklist_snapshot_free(before);
    assert(sqlite3_close(db) == SQLITE_OK);
    puts("Atomic checklist batch scope,8-item bounds,order,replay,rollback and reopen passed");
    return 0;
}
