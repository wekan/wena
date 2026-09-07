#include "../client/features/card_mutation.h"
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
    int value;
    assert(sqlite3_prepare_v2(db, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    value = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

static void command(WenaDomainCommand *c, unsigned long request, const char *body)
{
    memset(c, 0, sizeof(*c));
    c->operation = WENA_DOMAIN_CREATE_CARD;
    c->request_version = request;
    strcpy(c->user_id, "u1"); strcpy(c->route, "/b/b1/legacy");
    strcpy(c->form_body, body); c->form_body_length = strlen(body);
}

int main(int argc, char **argv)
{
    const char *hash = "e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE *file;
    unsigned char *sql;
    long length;
    sqlite3 *db;
    WenaCardMutation adapter, other;
    WenaCard cards[8], other_cards[4], before[8];
    WenaDomainCommand c;
    WenaRegionResponse response;
    char path[512], first_id[65], second_id[65], title[130];
    size_t count, other_count, i;
    int committed;
    const char *invalid[] = {
        "targetListId=l2&title=Partial", "targetSwimlaneId=s2&title=Partial",
        "targetListId=&targetSwimlaneId=s2&title=Empty",
        "targetListId=l2&targetSwimlaneId=%&title=Malformed",
        "targetListId=l2&targetListId=l1&targetSwimlaneId=s2&title=Duplicate",
        "targetListId=foreign-list&targetSwimlaneId=s2&title=Wrong",
        "targetListId=l2&targetSwimlaneId=foreign-lane&title=Wrong",
        "targetListId=missing&targetSwimlaneId=s2&title=Missing",
        "title=%C2%80", "title=Bad%C2%85Title", "title=%C2%9F",
        "title=Bad\302\205Title"
    };
    assert(argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0);
    rewind(file); sql = (unsigned char *)malloc((size_t)length); assert(sql);
    assert(fread(sql, 1, (size_t)length, file) == (size_t)length); fclose(file);
    sprintf(path, "%s/create.sqlite", argv[2]);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    execute(db, "INSERT INTO actors VALUES('u1','One',1);"
        "INSERT INTO actors VALUES('u2','Two',1);"
        "INSERT INTO boards VALUES('b1','Board',1);"
        "INSERT INTO boards VALUES('b2','Other',1);"
        "INSERT INTO swimlanes VALUES('s1','b1','First',0,1);"
        "INSERT INTO swimlanes VALUES('s2','b1','Second',1,1);"
        "INSERT INTO lists VALUES('l1','b1','First',0,1);"
        "INSERT INTO lists VALUES('l2','b1','Second',1,1);");
    execute(db, "INSERT INTO swimlanes VALUES('foreign-lane','b2','Other',0,1);"
        "INSERT INTO lists VALUES('foreign-list','b2','Other',0,1);"
        "INSERT INTO cards VALUES('existing','b1','s2','l2','Existing',10,1,1);");
    memset(cards, 0, sizeof(cards));
    assert(wena_card_init(&cards[0], "existing", "b1", "s2", "l2", "Existing", 10, 1));
    count = 1;
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", "Unconfigured"));
    assert(wena_card_mutation_set_create_cache(&adapter, &count, 8));
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", ""));
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", "bad\n"));
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", "\300\257"));
    memset(title, 'x', sizeof(title)); title[129] = 0;
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", title));
    assert(!wena_card_mutation_create(&adapter, "b2", "l2", "s2", "Scope"));
    assert(!wena_card_mutation_create(&adapter, "b1", "foreign-list", "s2", "Scope"));
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "foreign-lane", "Scope"));
    assert(!wena_card_mutation_create(&adapter, "b1", "missing", "s2", "Missing"));
    assert(count == 1 && scalar(db, "SELECT count(*) FROM idempotency_keys") == 0);
    assert(wena_card_mutation_create_request(&adapter, "b1", "l2", "s2", 1, "A&B + %= \303\244"));
    assert(count == 2 && adapter.card_count == 2);
    assert(!strcmp(cards[1].title, "A&B + %= \303\244"));
    assert(!strcmp(cards[1].list_id, "l2") && !strcmp(cards[1].swimlane_id, "s2"));
    assert(cards[1].sort == 11 && !cards[1].archived && strlen(cards[1].id) == 64);
    strcpy(first_id, cards[1].id);
    assert(!wena_card_mutation_create_request(&adapter, "b1", "l2", "s2", 1, "Replay"));
    assert(count == 2 && !adapter.persistence.created_card_id[0]);
    memcpy(before, cards, sizeof(cards));
    execute(db, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'injected'); END;");
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", "Rollback"));
    assert(!memcmp(before, cards, sizeof(cards)) && count == 2);
    assert(scalar(db, "SELECT count(*) FROM cards") == 2);
    assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == 1);
    execute(db, "DROP TRIGGER reject_metadata");
    assert(wena_card_mutation_set_create_cache(&adapter, &count, count));
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", "Full"));
    assert(scalar(db, "SELECT count(*) FROM cards") == 2);
    assert(wena_card_mutation_set_create_cache(&adapter, &count, 8));
    ++count;
    assert(!wena_card_mutation_create(&adapter, "b1", "l2", "s2", "Count mismatch"));
    --count;
    assert(wena_card_mutation_init(&other, db, "unknown", "b1", cards, count));
    assert(wena_card_mutation_set_create_cache(&other, &count, 8));
    assert(!wena_card_mutation_create(&other, "b1", "l2", "s2", "Unknown actor"));
    assert(wena_card_mutation_init(&other, db, "u2", "b1", cards, count));
    assert(wena_card_mutation_set_create_cache(&other, &count, 8));
    assert(wena_card_mutation_create_request(&other, "b1", "l2", "s2", 1, "Other actor"));
    assert(count == 3 && strcmp(cards[2].id, first_id));
    strcpy(second_id, cards[2].id);
    other_count = 0;
    assert(wena_card_mutation_init(&other, db, "u1", "b2", other_cards, 0));
    assert(wena_card_mutation_set_create_cache(&other, &other_count, 4));
    assert(wena_card_mutation_create_request(&other, "b2", "foreign-list", "foreign-lane", 1, "Other board"));
    assert(other_count == 1 && strcmp(other_cards[0].id, first_id) && strcmp(other_cards[0].id, second_id));
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(wena_sqlite_open(path, sql, (size_t)length, hash, &db));
    assert(wena_card_mutation_init(&adapter, db, "u1", "b1", cards, count));
    assert(wena_card_mutation_set_create_cache(&adapter, &count, 8));
    assert(!wena_card_mutation_create_request(&adapter, "b1", "l2", "s2", 1, "Old replay"));
    title[128] = 0;
    assert(wena_card_mutation_create(&adapter, "b1", "l2", "s2", title));
    assert(count == 4 && strlen(cards[3].title) == 128 && cards[3].sort == 13);
    committed = scalar(db, "SELECT count(*) FROM idempotency_keys");
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        command(&c, 1, invalid[i]);
        assert(!wena_sqlite_persistence_apply(&adapter.persistence, &c, &response));
        assert(!adapter.persistence.created_card_id[0]);
        assert(scalar(db, "SELECT count(*) FROM idempotency_keys") == committed);
    }
    /* The legacy no-parent path is intentionally supported and independently
     * idempotent; it also gets an opaque actor/route-scoped unique identity. */
    command(&c, 1, "title=Legacy");
    assert(wena_sqlite_persistence_apply(&adapter.persistence, &c, &response));
    assert(strlen(adapter.persistence.created_card_id) == 64);
    assert(scalar(db, "SELECT count(*) FROM cards WHERE list_id='l1' AND swimlane_id='s1'") == 1);
    assert(!wena_sqlite_persistence_apply(&adapter.persistence, &c, &response));
    assert(!adapter.persistence.created_card_id[0]);
    assert(sqlite3_close(db) == SQLITE_OK); free(sql);
    puts("native card creation persistence tests passed");
    return 0;
}
