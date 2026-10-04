/* WeKan's records for the board views (server/wekan_views.c), from documents
 * as FerretDB stores them. */
#include "../server/wekan_views.h"
#include "../server/wekan_sync.h"
#include "../server/ferretdb_sqlite.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void x(sqlite3 *db, const char *sql)
{
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "%s\n%s\n", sql, sqlite3_errmsg(db));
        assert(0);
    }
}

/* A document as FerretDB stores it: the JSON with its "$s"; *At is a date. */
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

int main(int argc, char **argv)
{
    unsigned char *bundle;
    size_t length;
    char sha[65], path[600];
    sqlite3 *db;
    WenaViewData data;
    const WenaViewCard *card;
    assert(argc == 2);
    assert(wena_sqlite_compiled_bundle(&bundle, &length, sha));
    assert(wena_sqlite_open(":memory:", bundle, length, sha, &db));
    free(bundle);
    sprintf(path, "%s/wekan.sqlite", argv[1]);
    assert(wena_wekan_sync_attach(db, path));
    /* Negative: a file without activities or history loads, with none. */
    doc(db, "boards", "{\"_id\":\"b1\",\"title\":\"Board\",\"labels\":[{\"_id\":\"g1\",\"name\":\"Bug\",\"color\":\"red\"}],"
        "\"mapImageAttachmentId\":\"img1\",\"color\":\"belize\",\"members\":[{\"userId\":\"u1\",\"isActive\":true},"
        "{\"userId\":\"u9\",\"isActive\":false}]}");
    doc(db, "attachments", "{\"_id\":\"img1\",\"versions\":{\"original\":{\"path\":\"/files/attachments/img1-original.png\"}}}");
    assert(wena_wekan_views_load(db, "b1", &data) && data.card_count == 0 && data.activity_count == 0 &&
           data.change_count == 0 && data.label_count == 1 && !strcmp(data.board_title, "Board"));
    /* The Map's image file; the board's color and active members. */
    assert(!strcmp(data.map_image, "img1") && !strcmp(data.map_image_path, "/files/attachments/img1-original.png") &&
           !strcmp(data.board_color, "belize") && data.active_members == 1);
    wena_view_data_free(&data);
    doc(db, "users", "{\"_id\":\"u1\",\"username\":\"ada\",\"profile\":{\"fullname\":\"Ada L\"}}");
    doc(db, "lists", "{\"_id\":\"l2\",\"title\":\"Done\",\"boardId\":\"b1\",\"sort\":2,\"wipLimit\":{\"value\":3,\"enabled\":true}}");
    doc(db, "lists", "{\"_id\":\"l1\",\"title\":\"To Do\",\"boardId\":\"b1\",\"sort\":1}");
    doc(db, "lists", "{\"_id\":\"l3\",\"title\":\"Old\",\"boardId\":\"b1\",\"sort\":0,\"archived\":true}");
    doc(db, "lists", "{\"_id\":\"lx\",\"title\":\"Elsewhere\",\"boardId\":\"b2\",\"sort\":0}");
    doc(db, "swimlanes", "{\"_id\":\"s1\",\"title\":\"Default\",\"boardId\":\"b1\",\"sort\":0}");
    doc(db, "cards", "{\"_id\":\"c1\",\"title\":\"First\",\"boardId\":\"b1\",\"listId\":\"l1\",\"swimlaneId\":\"s1\",\"sort\":1,"
        "\"createdAt\":1700000000000,\"dueAt\":1700500000000,\"spentTime\":2.5,\"assignees\":[\"u1\"],\"members\":[\"u1\"],"
        "\"labelIds\":[\"g1\"],\"vote\":{\"positive\":[\"u1\",\"u2\"],\"negative\":[\"u3\"]},\"poker\":{\"estimation\":\"5\"},"
        "\"cardDependencies\":[{\"cardId\":\"c2\",\"type\":\"blocks\"},{\"cardId\":\"c9\",\"type\":\"relates\"}],"
        "\"description\":\"Text\",\"mapX\":12.5,\"mapY\":40,\"cardNumber\":7}");
    doc(db, "cards", "{\"_id\":\"c2\",\"title\":\"Second\",\"boardId\":\"b1\",\"listId\":\"l2\",\"swimlaneId\":\"s1\",\"sort\":2,"
        "\"createdAt\":1700100000000,\"endAt\":1700200000000,\"archived\":true,\"archivedAt\":1700300000000}");
    doc(db, "cards", "{\"_id\":\"cx\",\"title\":\"Other board\",\"boardId\":\"b2\",\"listId\":\"lx\",\"createdAt\":1700000000000}");
    doc(db, "activities", "{\"_id\":\"a2\",\"activityType\":\"moveCard\",\"boardId\":\"b1\",\"cardId\":\"c2\",\"listId\":\"l2\","
        "\"oldListId\":\"l1\",\"userId\":\"u1\",\"createdAt\":1700150000000}");
    doc(db, "activities", "{\"_id\":\"a1\",\"activityType\":\"createCard\",\"boardId\":\"b1\",\"cardId\":\"c2\",\"listId\":\"l1\","
        "\"userId\":\"u1\",\"createdAt\":1700100000000}");
    doc(db, "activities", "{\"_id\":\"ax\",\"activityType\":\"createCard\",\"boardId\":\"b2\",\"cardId\":\"cx\",\"createdAt\":1}");
    doc(db, "changeHistory", "{\"_id\":\"h1\",\"boardId\":\"b1\",\"entityType\":\"card\",\"entityId\":\"c1\",\"group\":\"position\","
        "\"userId\":\"u1\",\"previousContent\":{\"listId\":\"l2\"},\"newContent\":{\"listId\":\"l1\"},\"createdAt\":1700400000000}");
    doc(db, "changeHistory", "{\"_id\":\"h2\",\"boardId\":\"b1\",\"entityType\":\"card\",\"entityId\":\"c1\",\"group\":\"dependencies\","
        "\"previousContent\":{\"field\":\"cardDependencies\",\"value\":[]},\"newContent\":{\"field\":\"cardDependencies\","
        "\"value\":[{\"cardId\":\"c2\",\"type\":\"blocks\"}]},\"createdAt\":1700410000000}");
    doc(db, "changeHistory", "{\"_id\":\"h3\",\"boardId\":\"b1\",\"entityType\":\"card\",\"entityId\":\"c2\",\"group\":\"dates\","
        "\"previousContent\":{\"field\":\"archived\",\"value\":false},\"newContent\":{\"field\":\"archived\",\"value\":true},"
        "\"createdAt\":1700300000000}");
    doc(db, "changeHistory", "{\"_id\":\"h4\",\"boardId\":\"b1\",\"entityType\":\"card\",\"entityId\":\"c3\",\"group\":\"lifecycle\","
        "\"previousContent\":{\"document\":{\"_id\":\"c3\",\"title\":\"Removed\",\"boardId\":\"b1\",\"listId\":\"l1\","
        "\"createdAt\":1700000000000}},\"createdAt\":1700600000000}");
    doc(db, "changeHistory", "{\"_id\":\"h5\",\"boardId\":\"b1\",\"entityType\":\"card\",\"entityId\":\"c1\",\"group\":\"title\","
        "\"previousContent\":{\"field\":\"title\",\"value\":\"F\"},\"newContent\":{\"field\":\"title\",\"value\":\"First\"},"
        "\"createdAt\":1700000000001}");

    if (!wena_wekan_views_load(db, "b1", &data)) { fprintf(stderr, "%s\n", sqlite3_errmsg(db)); assert(0); }
    /* Cards of this board, archived too, and the removed one's snapshot;
     * not another board's. */
    assert(data.card_count == 3);
    card = wena_view_card(&data, "c1");
    assert(card != NULL && !strcmp(card->title, "First") && !strcmp(card->list_id, "l1") && card->created_at.set &&
           card->created_at.ms == 1700000000000.0 && card->due_at.set && !card->end_at.set && card->spent_time == 2.5 &&
           card->assignee_count == 1 && !strcmp(card->assignees[0], "u1") && card->label_count == 1 &&
           card->votes_positive == 2 && card->votes_negative == 1 && card->has_poker && card->poker == 5.0 &&
           card->dependency_count == 1 && card->dependencies[0].blocks && !strcmp(card->description, "Text"));
    assert(card->map_x.set && card->map_x.ms == 12.5 && card->map_y.set && card->map_y.ms == 40.0 && card->card_number == 7);
    card = wena_view_card(&data, "c2");
    assert(card != NULL && card->archived && card->archived_at.set && card->end_at.set && !card->has_poker && !card->map_x.set);
    card = wena_view_card(&data, "c3");
    assert(card != NULL && !strcmp(card->title, "Removed") && card->deleted_at.set && card->deleted_at.ms == 1700600000000.0);
    assert(wena_view_card(&data, "cx") == NULL);
    /* Lists not archived, by sort; the labels, the users. */
    assert(data.list_count == 2 && !strcmp(data.lists[0].id, "l1") && !strcmp(data.lists[1].id, "l2") &&
           data.lists[1].wip_enabled && data.lists[1].wip_value == 3);
    assert(data.swimlane_count == 1 && !strcmp(wena_view_user_name(&data, "u1"), "Ada L") &&
           !strcmp(wena_view_user_name(&data, "nobody"), "nobody") && !strcmp(wena_view_label(&data, "g1")->name, "Bug"));
    /* Activities by time; the history rows the charts replay - not a title
     * change - with their kind, lists, values and dependencies. */
    assert(data.activity_count == 2 && !strcmp(data.activities[0].type, "createCard") &&
           !strcmp(data.activities[1].old_list_id, "l1"));
    assert(data.change_count == 4);
    assert(data.changes[0].kind == WENA_VIEW_CHANGE_FIELD && !strcmp(data.changes[0].field, "archived") &&
           !data.changes[0].has_old && data.changes[0].has_new && data.changes[0].new_value == 1.0);
    assert(data.changes[1].kind == WENA_VIEW_CHANGE_POSITION && !strcmp(data.changes[1].old_list_id, "l2") &&
           !strcmp(data.changes[1].new_list_id, "l1"));
    assert(data.changes[2].kind == WENA_VIEW_CHANGE_FIELD && !strcmp(data.changes[2].field, "cardDependencies") &&
           data.changes[2].old_dependency_count == 0 && data.changes[2].new_dependency_count == 1 &&
           !strcmp(data.change_dependencies[data.changes[2].new_dependency_start].card_id, "c2"));
    assert(data.changes[3].kind == WENA_VIEW_CHANGE_LIFECYCLE && data.changes[3].removed);
    wena_view_data_free(&data);
    /* The calendar of every board: the user's boards' open cards. */
    doc(db, "boards", "{\"_id\":\"b3\",\"title\":\"Mine\",\"members\":[{\"userId\":\"u1\",\"isActive\":true}]}");
    doc(db, "cards", "{\"_id\":\"c4\",\"title\":\"Mine card\",\"boardId\":\"b3\",\"dueAt\":1700000000000}");
    doc(db, "cards", "{\"_id\":\"c5\",\"title\":\"Archived\",\"boardId\":\"b3\",\"archived\":true}");
    assert(wena_wekan_views_load_all(db, "u1", &data));
    /* The open cards of both of Ada's boards, each named by its board, and
     * the boards with their lists, for Bigboard. */
    assert(data.card_count == 2 && wena_view_card(&data, "c4") != NULL && !strcmp(wena_view_card(&data, "c4")->board_title, "Mine") &&
           !strcmp(wena_view_card(&data, "c1")->board_title, "Board") && wena_view_card(&data, "c5") == NULL);
    assert(data.board_count == 2 && data.list_count == 2 && !strcmp(data.lists[0].board_id, "b1"));
    wena_view_data_free(&data);
    /* Negative: missing arguments. */
    assert(!wena_wekan_views_load(NULL, "b1", &data) && !wena_wekan_views_load(db, NULL, &data));
    sqlite3_close(db);
    puts("WeKan views: cards with their fields, removed snapshots, lists, users, activities and history passed");
    return 0;
}
