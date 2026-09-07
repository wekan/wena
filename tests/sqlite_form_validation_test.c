#include "../server/sqlite_persistence.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *prefixes[] = {
    "", "title=Created", "cardId=c1&title=Changed", "cardId=c1",
    "title=Changed", "listId=l1&title=Changed", "swimlaneId=s1&title=Changed",
    "cardId=c1&targetListId=l2&targetSwimlaneId=s2",
    "listId=l1&targetPosition=0", "swimlaneId=s1&targetPosition=0"
};
static const char *bad_numbers[] = {
    "1junk", "%2B1", "+1", "-1", "-0", "1.0", "1e0", "1+", "0",
    "", "99999999999999999999999999999999", "%091", "1%00",
    "1&expectedVersion=1"
};
static void command(WenaDomainCommand *c, int operation, const char *version)
{
    memset(c, 0, sizeof(*c));
    c->operation = (WenaDomainOperation)operation;
    c->request_version = 1;
    strcpy(c->route, "/b/b1/demo");
    strcpy(c->user_id, "u1");
    strcpy(c->form_body, prefixes[operation]);
    if (operation != WENA_DOMAIN_CREATE_CARD) {
        strcat(c->form_body, "&expectedVersion=");
        strcat(c->form_body, version);
    }
    c->form_body_length = strlen(c->form_body);
}
static void rejected(WenaSqlitePersistence *p, WenaDomainCommand *c)
{
    WenaRegionResponse response;
    unsigned char *bytes;
    size_t i;
    int changes;
    changes = sqlite3_total_changes(p->database);
    memset(&response, 0xa5, sizeof(response));
    assert(!wena_sqlite_persistence_apply(p, c, &response));
    bytes = (unsigned char *)&response;
    for (i = 0; i < sizeof(response); ++i) assert(bytes[i] == 0);
    assert(sqlite3_total_changes(p->database) == changes);
    assert(sqlite3_get_autocommit(p->database));
}
static void reset(sqlite3 *db)
{
    assert(sqlite3_exec(db,
        "DELETE FROM idempotency_keys;DELETE FROM cards;"
        "UPDATE boards SET title='Board',version=1;"
        "UPDATE lists SET title='List',version=1;"
        "UPDATE swimlanes SET title='Lane',version=1;"
        "INSERT INTO cards VALUES('c1','b1','s1','l1','Card',0,0,1);",
        NULL, NULL, NULL) == SQLITE_OK);
}
static int key_count(sqlite3 *db)
{
    sqlite3_stmt *statement;
    int result;
    assert(sqlite3_prepare_v2(db, "SELECT count(*) FROM idempotency_keys", -1,
                            &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    result = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return result;
}
int main(int argc, char **argv)
{
    FILE *file;
    char *schema, boundary[64], query[256];
    long length;
    sqlite3 *db;
    WenaSqlitePersistence persistence;
    WenaDomainCommand c;
    WenaRegionResponse response;
    int operation;
    size_t i;
    assert(argc == 2);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); schema = (char *)malloc((size_t)length + 1); assert(schema);
    assert(fread(schema, 1, (size_t)length, file) == (size_t)length);
    fclose(file); schema[length] = 0;
    assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
    assert(sqlite3_exec(db, schema, NULL, NULL, NULL) == SQLITE_OK); free(schema);
    assert(sqlite3_exec(db,
        "INSERT INTO actors VALUES('u1','User',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO lists VALUES('l1','b1','List',0,1);"
        "INSERT INTO lists VALUES('l2','b1','List',1,1);"
        "INSERT INTO swimlanes VALUES('s1','b1','Lane',0,1);"
        "INSERT INTO swimlanes VALUES('s2','b1','Lane',1,1);",
        NULL, NULL, NULL) == SQLITE_OK);
    wena_sqlite_persistence_init(&persistence, db);
    for (operation = WENA_DOMAIN_CREATE_CARD; operation <= WENA_DOMAIN_MOVE_SWIMLANE; ++operation) {
        reset(db);
        if (operation != WENA_DOMAIN_CREATE_CARD) {
            for (i = 0; i < sizeof(bad_numbers) / sizeof(bad_numbers[0]); ++i) {
                command(&c, operation, bad_numbers[i]); rejected(&persistence, &c);
            }
            sprintf(boundary, "%lu", (unsigned long)LONG_MAX);
            command(&c, operation, boundary); rejected(&persistence, &c);
            sprintf(boundary, "%lu", ULONG_MAX);
            command(&c, operation, boundary); rejected(&persistence, &c);
        }
        command(&c, operation, "1"); c.request_version = 0; rejected(&persistence, &c);
        c.request_version = (unsigned long)LONG_MAX + 1UL; rejected(&persistence, &c);
        c.request_version = ULONG_MAX; rejected(&persistence, &c);
        command(&c, operation, "1"); memset(c.route, 'x', sizeof(c.route)); rejected(&persistence, &c);
        command(&c, operation, "1"); memset(c.user_id, 'x', sizeof(c.user_id)); rejected(&persistence, &c);
        command(&c, operation, "1"); c.user_id[0] = 0; rejected(&persistence, &c);
        command(&c, operation, "1"); c.route[0] = 0; rejected(&persistence, &c);
        command(&c, operation, "1"); c.form_body_length = sizeof(c.form_body); rejected(&persistence, &c);
        assert(key_count(db) == 0);
        command(&c, operation, "1"); assert(wena_sqlite_persistence_apply(&persistence, &c, &response));
        assert(response.request_version == 1); assert(key_count(db) == 1);
        rejected(&persistence, &c);
    }
    for (operation = WENA_DOMAIN_MOVE_LIST; operation <= WENA_DOMAIN_MOVE_SWIMLANE; ++operation) {
        reset(db);
        for (i = 0; i < sizeof(bad_numbers) / sizeof(bad_numbers[0]); ++i) {
            if (!strcmp(bad_numbers[i], "0")) continue;
            command(&c, operation, "1");
            sprintf(c.form_body, "%s&targetPosition=%s&expectedVersion=1",
                    operation == WENA_DOMAIN_MOVE_LIST ? "listId=l1" : "swimlaneId=s1", bad_numbers[i]);
            c.form_body_length = strlen(c.form_body); rejected(&persistence, &c);
        }
    }
    reset(db);
    command(&c, WENA_DOMAIN_EDIT_CARD_TITLE, "%30%30%31");
    assert(wena_sqlite_persistence_apply(&persistence, &c, &response));
    assert(response.regions[0].version == 2);
    /* Boundary expected version is valid through LONG_MAX - 1, without
     * signed SQLite overflow on the successful increment. */
    reset(db);
    sprintf(query, "UPDATE cards SET version=%lu WHERE id='c1'", (unsigned long)LONG_MAX - 1UL);
    assert(sqlite3_exec(db, query, NULL, NULL, NULL) == SQLITE_OK);
    sprintf(boundary, "%lu", (unsigned long)LONG_MAX - 1UL);
    command(&c, WENA_DOMAIN_EDIT_CARD_TITLE, boundary); c.request_version = (unsigned long)LONG_MAX;
    assert(wena_sqlite_persistence_apply(&persistence, &c, &response));
    assert(response.regions[0].version == (unsigned long)LONG_MAX);
    assert(response.request_version == (unsigned long)LONG_MAX);
    sprintf(boundary, "%lu", (unsigned long)LONG_MAX);
    command(&c, WENA_DOMAIN_EDIT_CARD_TITLE, boundary); rejected(&persistence, &c);
    memset(&response, 0xa5, sizeof(response));
    assert(!wena_sqlite_persistence_apply(&persistence, NULL, &response));
    assert(response.request_version == 0 && response.region_count == 0);
    assert(!wena_sqlite_persistence_apply(&persistence, &c, NULL));
    assert(sqlite3_close(db) == SQLITE_OK);
    puts("SQLite strict form validation checks passed");
    return 0;
}
