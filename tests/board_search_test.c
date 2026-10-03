/* WeKan's board Search over Wena's tables (server/board_search.c). */
#include "../server/board_search.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static sqlite3 *db;
static const char *const seed[] = {
    "INSERT INTO boards(id, title, version) VALUES ('b1', 'Board', 1), ('b2', 'Other', 1)",
    "INSERT INTO swimlanes(id, board_id, title, position, version) VALUES ('s1', 'b1', 'Default', 0, 1),"
    " ('s2', 'b2', 'Default', 0, 1)",
    "INSERT INTO lists(id, board_id, title, position, version) VALUES ('l1', 'b1', 'Backlog', 0, 1),"
    " ('l2', 'b1', 'Release plan', 1, 1), ('l3', 'b2', 'Release elsewhere', 0, 1)",
    "INSERT INTO cards(id, board_id, swimlane_id, list_id, title, position, version) VALUES"
    " ('c1', 'b1', 's1', 'l1', 'Write the RELEASE notes', 0, 1), ('c2', 'b1', 's1', 'l1', 'Fix the build', 1, 1)",
    "INSERT INTO cards(id, board_id, swimlane_id, list_id, title, position, version) VALUES"
    " ('c3', 'b1', 's1', 'l2', '100% done_x', 0, 1), ('c4', 'b2', 's2', 'l3', 'Release on the other board', 0, 1)",
    "INSERT INTO card_descriptions(card_id, board_id, description) VALUES ('c2', 'b1', 'needed for the release')",
    NULL};
static WenaId lists[8], cards[8];
static size_t list_count, card_count;

static int search(const char *board, const char *term)
{
    return wena_board_search(db, board, term, lists, 8, &list_count, cards, 8, &card_count);
}

int main(void)
{
    unsigned char *bundle;
    size_t length;
    char sha[65];
    WenaId small[1];
    size_t i;
    assert(wena_sqlite_compiled_bundle(&bundle, &length, sha));
    assert(wena_sqlite_open(":memory:", bundle, length, sha, &db));
    free(bundle);
    for (i = 0; seed[i] != NULL; ++i) assert(sqlite3_exec(db, seed[i], NULL, NULL, NULL) == SQLITE_OK);

    /* Titles and descriptions, ignoring case, newest first; only this
     * board's. */
    assert(search("b1", "release"));
    assert(list_count == 1 && !strcmp(lists[0], "l2"));
    assert(card_count == 2 && !strcmp(cards[0], "c2") && !strcmp(cards[1], "c1"));
    /* WeKan trims the term. */
    assert(search("b1", "  backlog\t") && list_count == 1 && !strcmp(lists[0], "l1") && card_count == 0);
    /* LIKE's own characters are searched for as themselves. */
    assert(search("b1", "100%") && card_count == 1 && !strcmp(cards[0], "c3"));
    assert(search("b1", "e_x") && card_count == 1 && !strcmp(cards[0], "c3"));
    assert(search("b1", "%") && card_count == 1);
    assert(search("b1", "_") && card_count == 1);
    /* Negative: an empty term finds nothing; another board's rows stay
     * out; nothing past the capacity is written; missing arguments fail. */
    assert(search("b1", "   ") && list_count == 0 && card_count == 0);
    assert(search("b1", "elsewhere") && list_count == 0 && card_count == 0);
    assert(search("b2", "release") && list_count == 1 && !strcmp(lists[0], "l3") && card_count == 1 &&
           !strcmp(cards[0], "c4"));
    assert(wena_board_search(db, "b1", "release", lists, 8, &list_count, small, 1, &card_count) && card_count == 1);
    assert(!wena_board_search(db, "b1", NULL, lists, 8, &list_count, cards, 8, &card_count));
    assert(!wena_board_search(NULL, "b1", "x", lists, 8, &list_count, cards, 8, &card_count));
    sqlite3_close(db);
    return 0;
}
