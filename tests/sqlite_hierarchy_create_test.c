#include "../server/sqlite_persistence.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void execute(sqlite3 *db, const char *sql)
{
    assert(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
}

static int scalar(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *statement;
    int result;
    assert(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    result = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return result;
}

static void command(WenaDomainCommand *c, WenaDomainOperation operation,
                    unsigned long request, const char *body)
{
    memset(c, 0, sizeof(*c)); c->operation = operation;
    c->request_version = request;
    strcpy(c->user_id, "u1"); strcpy(c->route, "/b/b1/native");
    strcpy(c->form_body, body); c->form_body_length = strlen(body);
}

int main(int argc, char **argv)
{
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE *file;
    unsigned char *sql;
    long length;
    sqlite3 *db, *writer;
    WenaSqlitePersistence store;
    WenaDomainCommand c;
    WenaRegionResponse response;
    WenaDomainOperation operation;
    char path[512], query[256], body[160], first_id[65], last_list_id[65];
    const char *table;
    const char *invalid[] = {"title=", "title=%", "title=%00", "title=%0a",
        "title=%7f", "title=%C2%80", "title=%C0%AF", "title=%ED%A0%80",
        "title=One&title=Two", "title=Valid&broken"};
    size_t i;
    int initial_keys;
    assert(argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length); fclose(file);
    sprintf(path, "%s/hierarchy-create.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    execute(db, "INSERT INTO actors VALUES('u1','One',1);"
        "INSERT INTO actors VALUES('u2','Two',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO boards VALUES('b2','Other',1);"
        "INSERT INTO lists VALUES('l1','b1','List',0,1);"
        "INSERT INTO swimlanes VALUES('s1','b1','Lane',0,1)");
    last_list_id[0] = 0;
    for (operation = WENA_DOMAIN_CREATE_LIST; operation <= WENA_DOMAIN_CREATE_SWIMLANE;
         operation = (WenaDomainOperation)((int)operation + 1)) {
        table = operation == WENA_DOMAIN_CREATE_LIST ? "lists" : "swimlanes";
        wena_sqlite_persistence_init(&store, db);
        initial_keys = scalar(db, "SELECT count(*) FROM idempotency_keys");
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            command(&c, operation, 1, invalid[i]);
            assert(!wena_sqlite_persistence_apply(&store, &c, &response));
            assert(!store.created_hierarchy_id[0]);
            assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == initial_keys);
        }
        strcpy(body, "title="); memset(body + 6, 'x', 129); body[135] = 0;
        command(&c, operation, 1, body);
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        command(&c, operation, 1, "title=Valid"); strcpy(c.user_id, "unknown");
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        strcpy(c.user_id, "u1"); strcpy(c.route, "/b/missing/native");
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        command(&c, operation, 1, "title=A%26B+%2B+%25%3D+%C3%A4");
        assert(wena_sqlite_persistence_apply(&store, &c, &response));
        assert(store.created_hierarchy_position == 1 && strlen(store.created_hierarchy_id) == 64);
        assert(!strcmp(response.regions[0].content, "A&B + %= \303\244"));
        strcpy(first_id, store.created_hierarchy_id);
        if (operation == WENA_DOMAIN_CREATE_LIST) strcpy(last_list_id, first_id);
        else assert(strcmp(last_list_id, first_id));
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        assert(!store.created_hierarchy_id[0] && store.created_hierarchy_position == 0);
        command(&c, operation, 2, "title=Rollback");
        execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
            "BEGIN SELECT RAISE(ABORT,'injected'); END;");
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        sprintf(query, "SELECT count(*) FROM %s WHERE board_id='b1'", table);
        assert(scalar(db, query) == 2);
        assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == initial_keys + 1);
        execute(db, "DROP TRIGGER reject_metadata");
        command(&c, operation, 1, "title=Other+actor"); strcpy(c.user_id, "u2");
        assert(wena_sqlite_persistence_apply(&store, &c, &response));
        assert(store.created_hierarchy_position == 2 && strcmp(first_id, store.created_hierarchy_id));
        command(&c, operation, 1, "title=Other+board"); strcpy(c.route, "/b/b2/native");
        assert(wena_sqlite_persistence_apply(&store, &c, &response));
        assert(store.created_hierarchy_position == 0 && strcmp(first_id, store.created_hierarchy_id));
        assert(sqlite3_close(db) == SQLITE_OK);
        assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
        wena_sqlite_persistence_init(&store, db);
        sprintf(query, "SELECT count(*) FROM %s WHERE id='%s' AND board_id='b1' AND position=1 AND version=1", table, first_id);
        assert(scalar(db, query) == 1);
        command(&c, operation, 1, "title=Old+replay");
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        /* A competing connection holds the writer lock: failed contention must
         * not reserve a request or position; retry appends after its commit. */
        assert(wena_sqlite_open(path, sql, (size_t)length, hash, &writer));
        execute(writer, "BEGIN IMMEDIATE");
        assert(sqlite3_busy_timeout(db, 0) == SQLITE_OK);
        command(&c, operation, 2, "title=Retry");
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        sprintf(query, "INSERT INTO %s VALUES('concurrent-%s','b1','Concurrent',3,1)", table, table);
        execute(writer, query); execute(writer, "COMMIT");
        assert(sqlite3_close(writer) == SQLITE_OK);
        assert(wena_sqlite_persistence_apply(&store, &c, &response));
        assert(store.created_hierarchy_position == 4);
        sprintf(query, "SELECT count(DISTINCT position)=count(*) FROM %s WHERE board_id='b1'", table);
        assert(scalar(db, query));
        sprintf(query, "UPDATE %s SET position=9007199254740991 WHERE id='concurrent-%s'", table, table);
        execute(db, query);
        command(&c, operation, 3, "title=Overflow");
        assert(!wena_sqlite_persistence_apply(&store, &c, &response));
        sprintf(query, "UPDATE %s SET position=3 WHERE id='concurrent-%s'", table, table);
        execute(db, query);
        strcpy(body, "title="); memset(body + 6, 'x', 128); body[134] = 0;
        command(&c, operation, 3, body);
        assert(wena_sqlite_persistence_apply(&store, &c, &response));
        assert(store.created_hierarchy_position == 5 && response.regions[0].content_length == 128);
    }
    assert(sqlite3_close(db) == SQLITE_OK); free(sql);
    puts("SQLite hierarchy creation tests passed");
    return 0;
}
