/* WeKan's documents in FerretDB's wekan.sqlite and Wena's tables: import,
 * then export of only what Wena changed, with WeKan's other fields kept, new
 * documents with WeKan's defaults, and deletes. */
#include "../server/wekan_sync.h"
#include "../server/ferretdb_sqlite.h"
#include "../server/sqlite_storage.h"
#include "../server/sqlite_board.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char text[16384];
static const char *q(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *s = NULL;
    const unsigned char *v;
    text[0] = '\0';
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK) {
        fprintf(stderr, "%s\n%s\n", sql, sqlite3_errmsg(db));
        assert(0);
    }
    if (sqlite3_step(s) == SQLITE_ROW && (v = sqlite3_column_text(s, 0)) != NULL) strcpy(text, (const char *)v);
    sqlite3_finalize(s);
    return text;
}
static void x(sqlite3 *db, const char *sql)
{
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "%s\n%s\n", sql, sqlite3_errmsg(db));
        assert(0);
    }
}
/* A document as FerretDB stores it: the JSON with its "$s". */
static void doc(sqlite3 *db, const char *collection, const char *json)
{
    char table[WENA_FERRETDB_TABLE_CAPACITY], sql[4096];
    assert(wena_ferretdb_collection(db, "fdb", collection, 1, table, sizeof(table)));
    sprintf(sql, "INSERT INTO fdb.\"%s\" VALUES (json_set(json('%s'), '$.\"$s\"', json((SELECT json_object('p', "
                 "json_group_object(key, json(CASE WHEN key LIKE '%%At' THEN '{\"t\":\"date\"}' ELSE "
                 "wena_sjson_element(json('%s') -> ('$.\"' || key || '\"')) END)), '$k', json_group_array(key)) "
                 "FROM json_each(json('%s'))))))", table, json, json, json);
    x(db, sql);
}

static void count_entry(void *context, const char *id, int value)
{
    if (!strcmp(id, "li1") && value == 1) ++*(int *)context;
}

int main(int argc, char **argv)
{
    unsigned char *bundle;
    size_t length;
    char sha[65], path[600], user[64];
    sqlite3 *db;
    WenaSqliteBoardSnapshot *snapshot;
    assert(argc == 2);
    assert(wena_sqlite_compiled_bundle(&bundle, &length, sha));
    assert(wena_sqlite_open(":memory:", bundle, length, sha, &db));
    sprintf(path, "%s/wekan.sqlite", argv[1]);
    assert(wena_wekan_sync_attach(db, path));
    /* What a WeKan installation has: a user, a board with a label and a
     * member, a swimlane, two lists (one archived, with a WIP limit), cards
     * (one too long a title, one linked card, one in a missing list), a
     * description longer than Wena keeps, a checklist with two items. */
    doc(db, "users", "{\"_id\":\"u1\",\"username\":\"ada\",\"profile\":{\"fullname\":\"Ada L\"},\"isAdmin\":true,\"createdAt\":1}");
    doc(db, "users", "{\"_id\":\"u2\",\"username\":\"bob\",\"createdAt\":2}");
    doc(db, "boards", "{\"_id\":\"b1\",\"title\":\"Board\",\"slug\":\"board\",\"archived\":false,\"type\":\"board\","
        "\"permission\":\"private\",\"members\":[{\"userId\":\"u1\",\"isAdmin\":true,\"isActive\":true,\"isWorker\":false}],"
        "\"labels\":[{\"_id\":\"l1\",\"name\":\"Urgent\",\"color\":\"red\"},{\"_id\":\"l2\",\"name\":\"\",\"color\":\"#123456\"}],"
        "\"allowsCardCollapse\":false,\"description\":\"kept\",\"stars\":2}");
    doc(db, "boards", "{\"_id\":\"tmpl\",\"title\":\"Templates\",\"type\":\"template-container\"}");
    doc(db, "boards", "{\"_id\":\"old\",\"title\":\"Old\",\"type\":\"board\",\"archived\":true}");
    doc(db, "swimlanes", "{\"_id\":\"s1\",\"title\":\"Default\",\"boardId\":\"b1\",\"sort\":0,\"archived\":false}");
    doc(db, "lists", "{\"_id\":\"li2\",\"title\":\"Done\",\"boardId\":\"b1\",\"sort\":5,\"archived\":true,\"archivedAt\":1700000000000,"
        "\"width\":5000,\"color\":\"green\",\"wipLimit\":{\"value\":3,\"enabled\":true,\"soft\":false}}");
    doc(db, "lists", "{\"_id\":\"li1\",\"title\":\"To Do\",\"boardId\":\"b1\",\"sort\":1.5,\"width\":300}");
    doc(db, "cards", "{\"_id\":\"c1\",\"title\":\"First\",\"boardId\":\"b1\",\"listId\":\"li1\",\"swimlaneId\":\"s1\",\"sort\":2,"
        "\"labelIds\":[\"l1\",\"gone\"],\"members\":[\"u1\"],\"assignees\":[],\"type\":\"cardType-card\",\"dueAt\":1700000000000,"
        "\"customFields\":[{\"_id\":\"f\",\"value\":\"x\"}]}");
    doc(db, "cards", "{\"_id\":\"c2\",\"title\":\"0123456789012345678901234567890123456789012345678901234567890123456789"
        "012345678901234567890123456789012345678901234567890123456789\\nmore\",\"boardId\":\"b1\",\"listId\":\"li1\","
        "\"swimlaneId\":\"s1\",\"sort\":1,\"description\":\"line one\\nline two\"}");
    doc(db, "cards", "{\"_id\":\"c3\",\"title\":\"Linked\",\"boardId\":\"b1\",\"listId\":\"li1\",\"swimlaneId\":\"s1\",\"type\":\"cardType-linkedCard\"}");
    doc(db, "cards", "{\"_id\":\"c4\",\"title\":\"Lost\",\"boardId\":\"b1\",\"listId\":\"nowhere\",\"swimlaneId\":\"s1\"}");
    doc(db, "checklists", "{\"_id\":\"k1\",\"cardId\":\"c1\",\"boardId\":\"b1\",\"title\":\"Release\",\"sort\":0}");
    doc(db, "checklistItems", "{\"_id\":\"i1\",\"checklistId\":\"k1\",\"cardId\":\"c1\",\"boardId\":\"b1\",\"title\":\"Build\",\"sort\":0,\"isFinished\":true}");
    doc(db, "checklistItems", "{\"_id\":\"i2\",\"checklistId\":\"k1\",\"cardId\":\"c1\",\"boardId\":\"b1\",\"title\":\"Test\",\"sort\":1,\"isFinished\":false}");

    if (!wena_wekan_sync_import(db)) { fprintf(stderr, "%s\n", wena_wekan_sync_error()); assert(0); }
    /* Read in: only the board WeKan shows, its rows in WeKan's order. */
    assert(!strcmp(q(db, "SELECT group_concat(id) FROM (SELECT id FROM boards ORDER BY id)"), "b1"));
    assert(!strcmp(q(db, "SELECT display_name FROM actors WHERE id='u1'"), "Ada L"));
    assert(!strcmp(q(db, "SELECT display_name FROM actors WHERE id='u2'"), "bob"));
    assert(!strcmp(q(db, "SELECT group_concat(id || ':' || position) FROM (SELECT * FROM lists ORDER BY position)"), "li1:0,li2:1"));
    assert(!strcmp(q(db, "SELECT archived || ',' || archived_at FROM list_archive_state WHERE list_id='li2'"), "1,1700000000"));
    assert(!strcmp(q(db, "SELECT color FROM list_colors WHERE list_id='li2'"), "green"));
    assert(!strcmp(q(db, "SELECT value || enabled || soft FROM list_wip_limits WHERE list_id='li2'"), "310"));
    assert(!strcmp(q(db, "SELECT group_concat(id || ':' || position) FROM (SELECT * FROM cards ORDER BY position)"), "c2:0,c1:1"));
    assert(!strcmp(q(db, "SELECT length(CAST(title AS BLOB)) <= 128 AND instr(title, char(10)) = 0 FROM cards WHERE id='c2'"), "1"));
    assert(!strcmp(q(db, "SELECT description FROM card_descriptions WHERE card_id='c2'"), "line one\nline two"));
    assert(!strcmp(q(db, "SELECT group_concat(label_id) FROM card_labels WHERE card_id='c1'"), "l1"));
    assert(!strcmp(q(db, "SELECT name || color FROM labels WHERE id='l2'"), ""));
    assert(!strcmp(q(db, "SELECT allow_collapse FROM board_card_collapse_settings WHERE board_id='b1'"), "0"));
    assert(!strcmp(q(db, "SELECT group_concat(actor_id) FROM card_people WHERE card_id='c1' AND field='members'"), "u1"));
    assert(!strcmp(q(db, "SELECT group_concat(title || is_finished) FROM (SELECT * FROM checklist_items ORDER BY position)"), "Build1,Test0"));
    /* Negative: linked cards, cards in a list that is not there, templates and archived boards stay out. */
    assert(!strcmp(q(db, "SELECT count(*) FROM cards WHERE id IN ('c3','c4')"), "0"));
    snapshot = (WenaSqliteBoardSnapshot *)calloc(1, sizeof(*snapshot));
    assert(snapshot != NULL && wena_sqlite_board_load(db, "b1", snapshot));
    assert(snapshot->list_count == 2 && snapshot->card_count == 2);
    /* WeKan's list width; negative: one out of WeKan's 100..1000 stays the layout's default. */
    assert(!strcmp(snapshot->lists[0].id, "li1") && snapshot->lists[0].width == 300u);
    assert(!strcmp(snapshot->lists[1].id, "li2") && snapshot->lists[1].width == 0u);

    /* Nothing changed: nothing written. */
    assert(wena_wekan_sync_export(db, "u1") == 0);
    /* Changes: a title, a label, an archived card, an item ticked, an item
     * deleted, a new card and a new checklist. */
    x(db, "UPDATE cards SET title='First renamed', version=version+1 WHERE id='c1'");
    x(db, "UPDATE labels SET name='Very urgent' WHERE id='l1'");
    x(db, "UPDATE cards SET archived=1 WHERE id='c2'");
    x(db, "INSERT INTO card_archive_state(card_id, board_id, archived_at) VALUES ('c2','b1',1700000100)");
    x(db, "UPDATE checklist_items SET is_finished=1 WHERE id='i2'");
    x(db, "DELETE FROM checklist_items WHERE id='i1'");
    x(db, "INSERT INTO cards(id, board_id, swimlane_id, list_id, title, position, version) VALUES ('c9','b1','s1','li1','New',2,1)");
    x(db, "INSERT INTO card_labels VALUES ('b1','c9','l1')");
    assert(wena_wekan_sync_export(db, "u1") == 6);
    /* The renamed card: only its title (and modifiedAt) changed; WeKan's
     * fields Wena does not keep are as they were, and so are the labelIds
     * of a label Wena did not have. */
    assert(!strcmp(q(db, "SELECT _ferretdb_sjson ->> 'title' FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c1\"'"), "First renamed"));
    assert(!strcmp(q(db, "SELECT (_ferretdb_sjson -> 'customFields') || (_ferretdb_sjson ->> 'dueAt') || "
                         "(_ferretdb_sjson -> '$.\"$s\".p.dueAt') || (_ferretdb_sjson -> 'labelIds') "
                         "FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c1\"'"),
                   "[{\"_id\":\"f\",\"value\":\"x\"}]1700000000000{\"t\":\"date\"}[\"l1\",\"gone\"]"));
    assert(!strcmp(q(db, "SELECT json_array_length(_ferretdb_sjson -> '$.\"$s\".\"$k\"') = (SELECT count(*) - 1 FROM "
                         "json_each(_ferretdb_sjson)) FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c1\"'"), "1"));
    /* The long title Wena cut stayed whole in WeKan: it was not changed. */
    assert(strstr(q(db, "SELECT _ferretdb_sjson ->> 'title' FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c2\"'"), "\nmore") != NULL);
    assert(!strcmp(q(db, "SELECT (_ferretdb_sjson ->> 'archived') || ',' || (_ferretdb_sjson ->> 'archivedAt') || ',' || "
                         "(_ferretdb_sjson -> '$.\"$s\".p.archivedAt') FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c2\"'"),
                   "1,1700000100000,{\"t\":\"date\"}"));
    /* The label renamed inside the board, its other fields and the other labels kept. */
    assert(!strcmp(q(db, "SELECT _ferretdb_sjson -> 'labels' FROM fdb.boards_7c666488 WHERE _ferretdb_sjson->'_id' = '\"b1\"'"),
                   "[{\"_id\":\"l1\",\"name\":\"Very urgent\",\"color\":\"red\"},{\"_id\":\"l2\",\"name\":\"\",\"color\":\"\"}]"));
    assert(!strcmp(q(db, "SELECT (_ferretdb_sjson ->> 'description') || (_ferretdb_sjson ->> 'stars') || "
                         "(_ferretdb_sjson -> '$.members[0].isWorker') FROM fdb.boards_7c666488 WHERE _ferretdb_sjson->'_id' = '\"b1\"'"),
                   "kept2false"));
    /* The new card: WeKan's required fields, its creator, its label. */
    assert(!strcmp(q(db, "SELECT (_ferretdb_sjson ->> 'type') || ',' || (_ferretdb_sjson ->> 'userId') || ',' || "
                         "(_ferretdb_sjson -> 'labelIds') || ',' || (_ferretdb_sjson -> 'requesters') || ',' || "
                         "(_ferretdb_sjson -> '$.\"$s\".p.createdAt') || ',' || (_ferretdb_sjson ->> 'swimlaneId') "
                         "FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c9\"'"),
                   "cardType-card,u1,[\"l1\"],[],{\"t\":\"date\"},s1"));
    assert(!strcmp(q(db, "SELECT _ferretdb_sjson -> '$.\"$s\".\"$k\"[0]' FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c9\"'"), "\"_id\""));
    assert(!strcmp(q(db, "SELECT _ferretdb_sjson ->> 'isFinished' FROM fdb.checklistitems_ef324221 WHERE _ferretdb_sjson->'_id' = '\"i2\"'"), "1"));
    assert(!strcmp(q(db, "SELECT count(*) FROM fdb.checklistitems_ef324221 WHERE _ferretdb_sjson->'_id' = '\"i1\"'"), "0"));
    /* And once written, not again. */
    assert(wena_wekan_sync_export(db, "u1") == 0);
    /* Negative: a failure writes nothing - the transaction is rolled back. */
    x(db, "UPDATE cards SET title='Not written' WHERE id='c1'");
    x(db, "CREATE TRIGGER fdb.refuse BEFORE UPDATE ON cards_81f16044 BEGIN SELECT RAISE(ABORT, 'no'); END");
    assert(wena_wekan_sync_export(db, "u1") == -1);
    assert(!strcmp(q(db, "SELECT _ferretdb_sjson ->> 'title' FROM fdb.cards_81f16044 WHERE _ferretdb_sjson->'_id' = '\"c1\"'"), "First renamed"));
    x(db, "DROP TRIGGER fdb.refuse");
    assert(wena_wekan_sync_export(db, "u1") == 1);

    /* All Boards: Ada's boards - the one she can open, an archived one, a
     * template container - and not those she is no member of or WeKan's
     * helper boards; starring writes her profile.starredBoards. */
    {
        WenaWekanBoardTile tiles[8];
        size_t count;
        char made[64];
        doc(db, "boards", "{\"_id\":\"arch\",\"title\":\"Archived one\",\"type\":\"board\",\"archived\":true,"
            "\"members\":[{\"userId\":\"u1\",\"isAdmin\":true,\"isActive\":true}]}");
        doc(db, "boards", "{\"_id\":\"tc\",\"title\":\"Templates\",\"type\":\"template-container\","
            "\"members\":[{\"userId\":\"u1\",\"isAdmin\":true,\"isActive\":true}]}");
        doc(db, "boards", "{\"_id\":\"help\",\"title\":\"^Templates^\",\"type\":\"board\","
            "\"members\":[{\"userId\":\"u1\",\"isAdmin\":true,\"isActive\":true}]}");
        doc(db, "boards", "{\"_id\":\"other\",\"title\":\"Bobs\",\"type\":\"board\","
            "\"members\":[{\"userId\":\"u2\",\"isAdmin\":true,\"isActive\":true}]}");
        doc(db, "boards", "{\"_id\":\"left\",\"title\":\"Left\",\"type\":\"board\","
            "\"members\":[{\"userId\":\"u1\",\"isAdmin\":false,\"isActive\":false}]}");
        assert(wena_wekan_sync_boards(db, "u1", tiles, 8, &count) && count == 3);
        assert(!strcmp(tiles[0].id, "arch") && tiles[0].archived && !tiles[0].openable);
        assert(!strcmp(tiles[1].id, "b1") && tiles[1].openable && !strcmp(tiles[1].color, "belize") && !tiles[1].starred);
        assert(!strcmp(tiles[2].id, "tc") && tiles[2].template_board);
        assert(wena_wekan_sync_star(db, "u1", "b1", 1) && wena_wekan_sync_star(db, "u1", "tc", 1));
        assert(wena_wekan_sync_boards(db, "u1", tiles, 8, &count) && tiles[1].starred && tiles[2].starred);
        assert(wena_wekan_sync_star(db, "u1", "b1", 0));
        assert(!strcmp(q(db, "SELECT _ferretdb_sjson -> '$.profile' FROM fdb.users_5e7cc513 WHERE _ferretdb_sjson->'_id' = '\"u1\"'"),
                       "{\"fullname\":\"Ada L\",\"starredBoards\":[\"tc\"]}"));
        /* Negative: a user without a profile object is not given a broken one. */
        assert(!wena_wekan_sync_star(db, "u2", "b1", 1));
        /* A new board: WeKan's fields, its Default swimlane, Ada its admin. */
        assert(wena_wekan_sync_new_board(db, "u1", "Fresh", made, sizeof(made)) && strlen(made) == 17);
        assert(!strcmp(q(db, "SELECT count(*) FROM board_members WHERE board_id = (SELECT id FROM boards WHERE title='Fresh')"), "1"));
        assert(wena_wekan_sync_boards(db, "u1", tiles, 8, &count) && count == 4 && !strcmp(tiles[2].title, "Fresh") &&
               tiles[2].openable);
        assert(!strcmp(q(db, "SELECT (x->>'type') || (x -> '$.members[0].isAdmin') || (x->>'slug') || "
                             "(SELECT y->>'title' FROM (SELECT _ferretdb_sjson AS y FROM fdb.swimlanes_d9d57a4c) "
                             "WHERE y->>'boardId' = x->>'_id') FROM (SELECT _ferretdb_sjson AS x FROM fdb.boards_7c666488) "
                             "WHERE x->>'title' = 'Fresh'"), "boardtruefreshDefault"));
        /* WeKan's board defaults, so WeKan shows the board's parts. */
        assert(!strcmp(q(db, "SELECT (x->>'allowsDescriptionText') || (x->>'allowsChecklists') || (x->>'allowsComments') "
                             "|| (x->>'allowsCardNumber') || (x -> '$.\"$s\".p.allowsChecklists.t') FROM "
                             "(SELECT _ferretdb_sjson AS x FROM fdb.boards_7c666488) WHERE x->>'title' = 'Fresh'"),
                       "1110\"bool\""));
        /* Checklists on minicards: WeKan's allowsChecklistsOnMinicard, on for a new board. */
        assert(!strcmp(q(db, "SELECT (x->>'allowsChecklistsOnMinicard') || (SELECT show_checklists FROM board_minicard_settings "
                             "WHERE board_id = x->>'_id') FROM (SELECT _ferretdb_sjson AS x FROM fdb.boards_7c666488) "
                             "WHERE x->>'title' = 'Fresh'"), "11"));
        assert(wena_wekan_sync_export(db, "u1") == 0);
    }
    /* The user's language and per-board maps, where WeKan keeps them. */
    {
        char language[64];
        int seen = 0;
        assert(!wena_wekan_sync_language(db, "u1", language, sizeof(language)));
        assert(wena_wekan_sync_set_language(db, "u1", "fi"));
        assert(wena_wekan_sync_language(db, "u1", language, sizeof(language)) && !strcmp(language, "fi"));
        assert(wena_wekan_sync_set_profile_board_map(db, "u1", "collapsedLists", "b1", "{\"li1\":true}"));
        assert(wena_wekan_sync_set_profile_board_map(db, "u1", "collapsedLists", "other", "{\"x\":false}"));
        assert(wena_wekan_sync_set_profile_board_map(db, "u1", "swimlaneHeights", "b1", "{\"s1\":480}"));
        assert(wena_wekan_sync_profile_board_map(db, "u1", "collapsedLists", "b1", count_entry, &seen) && seen == 1);
        assert(!strcmp(q(db, "SELECT (_ferretdb_sjson -> '$.profile.collapsedLists') || "
                             "(_ferretdb_sjson -> '$.\"$s\".p.profile.\"$s\".p.swimlaneHeights') FROM fdb.users_5e7cc513 "
                             "WHERE _ferretdb_sjson->'_id' = '\"u1\"'"),
                       "{\"b1\":{\"li1\":true},\"other\":{\"x\":false}}{\"t\":\"object\",\"$s\":{\"p\":{\"b1\":"
                       "{\"t\":\"object\",\"$s\":{\"p\":{\"s1\":{\"t\":\"int\"}},\"$k\":[\"s1\"]}}},\"$k\":[\"b1\"]}}"));
        /* Replacing one board's map keeps the others. */
        assert(wena_wekan_sync_set_profile_board_map(db, "u1", "collapsedLists", "b1", "{}"));
        assert(!strcmp(q(db, "SELECT _ferretdb_sjson -> '$.profile.collapsedLists' FROM fdb.users_5e7cc513 "
                             "WHERE _ferretdb_sjson->'_id' = '\"u1\"'"), "{\"b1\":{},\"other\":{\"x\":false}}"));
        /* Negative: only WeKan's own fields. */
        assert(!wena_wekan_sync_set_profile_board_map(db, "u1", "services", "b1", "{}"));
    }
    /* The user: asked for by username, else the admin, and one is made in a
     * file without users. */
    assert(wena_wekan_sync_user(db, "bob", user, sizeof(user)) && !strcmp(user, "u2"));
    assert(wena_wekan_sync_user(db, NULL, user, sizeof(user)) && !strcmp(user, "u1"));
    assert(wena_wekan_sync_user(db, "nobody", user, sizeof(user)) && !strcmp(user, "u1"));
    x(db, "DELETE FROM fdb.users_5e7cc513");
    assert(wena_wekan_sync_user(db, NULL, user, sizeof(user)) && strlen(user) == 17);
    assert(!strcmp(q(db, "SELECT (_ferretdb_sjson ->> 'username') || (_ferretdb_sjson ->> 'isAdmin') FROM fdb.users_5e7cc513"), "admin1"));
    free(snapshot);
    free(bundle);
    assert(sqlite3_close(db) == SQLITE_OK);
    puts("WeKan sync: import, changed fields only, WeKan's fields kept, new documents, deletes, rollback and the user passed");
    return 0;
}
