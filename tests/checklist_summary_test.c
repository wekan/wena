#include "../client/features/checklists/summary.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Work { unsigned long statements, steps, scans, sorts; } Work;
static int trace(unsigned int type, void *context, void *statement, void *elapsed)
{
    Work *work; sqlite3_stmt *query;
    (void)elapsed;
    if (type != SQLITE_TRACE_PROFILE) return 0;
    work = (Work *)context; query = (sqlite3_stmt *)statement;
    ++work->statements;
    work->steps += (unsigned long)sqlite3_stmt_status(query, SQLITE_STMTSTATUS_VM_STEP, 0);
    work->scans += (unsigned long)sqlite3_stmt_status(query, SQLITE_STMTSTATUS_FULLSCAN_STEP, 0);
    work->sorts += (unsigned long)sqlite3_stmt_status(query, SQLITE_STMTSTATUS_SORT, 0);
    return 0;
}
static void execute(sqlite3 *db, const char *sql)
{
    int result;
    result = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (result != SQLITE_OK) fprintf(stderr, "SQL failed: %s: %s\n", sql, sqlite3_errmsg(db));
    assert(result == SQLITE_OK);
}
static void load(sqlite3 *db, WenaChecklistBoardSummary *summary)
{
    assert(wena_checklist_summary_load(db, "actor", "board", 1, summary));
    assert(summary->enabled && !strcmp(summary->board_id, "board"));
}
static Work measure(sqlite3 *db, WenaChecklistBoardSummary *summary)
{
    Work result;
    memset(&result, 0, sizeof(result));
    assert(sqlite3_trace_v2(db, SQLITE_TRACE_PROFILE, trace, &result) == SQLITE_OK);
    load(db, summary);
    assert(sqlite3_trace_v2(db, 0, NULL, NULL) == SQLITE_OK);
    return result;
}
static void unchanged_failure(sqlite3 *db, WenaChecklistBoardSummary *summary,
    WenaChecklistBoardSummary *before)
{
    memset(summary, 0x5a, sizeof(*summary)); *before = *summary;
    assert(!wena_checklist_summary_load(db, "actor", "board", 1, summary));
    assert(!memcmp(summary, before, sizeof(*summary)));
    assert(sqlite3_get_autocommit(db));
}
static void seed(sqlite3 *db)
{
    execute(db, "INSERT INTO actors VALUES('actor','Actor',1);"
        "INSERT INTO boards VALUES('board','Board',7);INSERT INTO boards VALUES('other','Other',1);"
        "INSERT INTO swimlanes VALUES('lane','board','Lane',0,1);INSERT INTO swimlanes VALUES('other-lane','other','Other',0,1);"
        "INSERT INTO lists VALUES('list','board','List',0,1);INSERT INTO lists VALUES('other-list','other','Other',0,1)");
    execute(db, "INSERT INTO cards VALUES('a-empty','board','lane','list','No checklist',0,0,11);"
        "INSERT INTO cards VALUES('b-todo','board','lane','list','Mixed',1,0,12);"
        "INSERT INTO cards VALUES('c-zero','board','lane','list','Empty checklist',2,0,13);"
        "INSERT INTO cards VALUES('d-done','board','lane','list','Archived done',3,1,14);"
        "INSERT INTO cards VALUES('other-card','other','other-lane','other-list','Other',0,0,1)");
    execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('check','board','b-todo','Mixed',0);"
        "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('zero','board','c-zero','Zero',0);"
        "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('done','board','d-done','Done',0);"
        "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('foreign','other','other-card','Foreign',0)");
    execute(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('item','board','b-todo','check','Todo',0,0);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('item-done','board','b-todo','check','Done',1,1);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('archived-done','board','d-done','done','Done',0,1)");
}
static void baseline(WenaChecklistBoardSummary *summary)
{
    const WenaChecklistCardSummary *card;
    assert(summary->card_count == 4 && summary->board_version == 7);
    card = wena_checklist_summary_find(summary, "a-empty");
    assert(card && !card->checklist_count && !card->progress.total && !card->progress.is_finished);
    card = wena_checklist_summary_find(summary, "b-todo");
    assert(card && card->card_version == 12 && card->checklist_count == 1);
    assert(card->progress.total == 2 && card->progress.finished == 1 && card->progress.percent == 50 && !card->progress.is_finished);
    card = wena_checklist_summary_find(summary, "c-zero");
    assert(card && card->checklist_count == 1 && !card->progress.total && !card->progress.is_finished);
    card = wena_checklist_summary_find(summary, "d-done");
    assert(card && card->archived && card->progress.total == 1 && card->progress.finished == 1 && card->progress.percent == 100 && card->progress.is_finished);
    assert(!wena_checklist_summary_find(summary, "missing"));
    assert(!wena_checklist_summary_find(summary, "../bad"));
}
static sqlite3 *writer;
static int write_once, commit_failure;
static int authorize(void *context, int action, const char *first,
    const char *second, const char *database, const char *trigger)
{
    (void)context; (void)second; (void)database; (void)trigger;
    if (commit_failure && action == SQLITE_TRANSACTION && first && !strcmp(first, "COMMIT")) return SQLITE_DENY;
    if (write_once && action == SQLITE_READ && first && !strcmp(first, "checklist_items")) {
        write_once = 0;
        execute(writer, "BEGIN;UPDATE checklist_items SET is_finished=1 WHERE id='item';"
            "UPDATE cards SET version=version+1 WHERE id='b-todo';COMMIT");
    }
    return SQLITE_OK;
}
static void corruption(sqlite3 *db, WenaChecklistBoardSummary *summary,
    WenaChecklistBoardSummary *before)
{
    static const char *cases[][2] = {
        {"UPDATE checklists SET board_id='other' WHERE id='check'", "UPDATE checklists SET board_id='board' WHERE id='check'"},
        {"UPDATE checklist_items SET board_id='other' WHERE id='item'", "UPDATE checklist_items SET board_id='board' WHERE id='item'"},
        {"UPDATE checklist_items SET card_id='other-card' WHERE id='item'", "UPDATE checklist_items SET card_id='b-todo' WHERE id='item'"},
        {"UPDATE checklist_items SET board_id='other',card_id='other-card' WHERE id='item'", "UPDATE checklist_items SET board_id='board',card_id='b-todo' WHERE id='item'"},
        {"UPDATE checklist_items SET card_id='missing' WHERE id='item'", "UPDATE checklist_items SET card_id='b-todo' WHERE id='item'"},
        {"UPDATE checklist_items SET checklist_id='zero' WHERE id='item'", "UPDATE checklist_items SET checklist_id='check' WHERE id='item'"},
        {"UPDATE checklist_items SET checklist_id='missing' WHERE id='item'", "UPDATE checklist_items SET checklist_id='check' WHERE id='item'"},
        {"UPDATE checklist_items SET checklist_id='foreign' WHERE id='item'", "UPDATE checklist_items SET checklist_id='check' WHERE id='item'"},
        {"UPDATE checklist_items SET is_finished=2 WHERE id='item'", "UPDATE checklist_items SET is_finished=0 WHERE id='item'"},
        {"UPDATE checklist_items SET is_finished=0.5 WHERE id='item'", "UPDATE checklist_items SET is_finished=0 WHERE id='item'"},
        {"UPDATE checklist_items SET is_finished='invalid' WHERE id='item'", "UPDATE checklist_items SET is_finished=0 WHERE id='item'"},
        {"UPDATE checklist_items SET title=CAST(x'c080' AS TEXT) WHERE id='item'", "UPDATE checklist_items SET title='Todo' WHERE id='item'"},
        {"UPDATE checklists SET title='Bad'||char(10) WHERE id='check'", "UPDATE checklists SET title='Mixed' WHERE id='check'"},
        {"UPDATE checklists SET hide_all_items=2 WHERE id='check'", "UPDATE checklists SET hide_all_items=0 WHERE id='check'"},
        {"UPDATE checklists SET hide_checked_items='invalid' WHERE id='check'", "UPDATE checklists SET hide_checked_items=0 WHERE id='check'"},
        {"UPDATE checklists SET show_on_minicard=2 WHERE id='check'", "UPDATE checklists SET show_on_minicard=NULL WHERE id='check'"},
        {"UPDATE checklist_items SET position=-1 WHERE id='item'", "UPDATE checklist_items SET position=0 WHERE id='item'"},
        {"UPDATE checklists SET created_at=1 WHERE id='check'", "UPDATE checklists SET created_at=0 WHERE id='check'"},
        {"UPDATE checklist_items SET updated_at=-1 WHERE id='item'", "UPDATE checklist_items SET updated_at=0 WHERE id='item'"},
        {"UPDATE checklist_items SET version=0 WHERE id='item'", "UPDATE checklist_items SET version=1 WHERE id='item'"},
        {"UPDATE cards SET version=0 WHERE id='b-todo'", "UPDATE cards SET version=12 WHERE id='b-todo'"},
        {"UPDATE boards SET version=0 WHERE id='board'", "UPDATE boards SET version=7 WHERE id='board'"},
        {"UPDATE cards SET archived=2 WHERE id='d-done'", "UPDATE cards SET archived=1 WHERE id='d-done'"}
    };
    size_t index;
    execute(db, "PRAGMA foreign_keys=OFF;PRAGMA ignore_check_constraints=ON");
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        execute(db, cases[index][0]); unchanged_failure(db, summary, before);
        execute(db, cases[index][1]); load(db, summary); baseline(summary);
    }
    /* Rows unreachable from any actual board card or checklist are outside this
     * selected-parent projection, as in the existing native card adapter. */
    execute(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "VALUES('unreachable','board','missing','missing','Unreachable',0)");
    load(db, summary); baseline(summary);
    execute(db, "DELETE FROM checklist_items WHERE id='unreachable';PRAGMA ignore_check_constraints=OFF;PRAGMA foreign_keys=ON");
}
static void limits(sqlite3 *db, WenaChecklistBoardSummary *summary,
    WenaChecklistBoardSummary *before)
{
    const WenaChecklistCardSummary *card;
    Work many;
    execute(db, "WITH RECURSIVE n(x) AS(VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<63) "
        "INSERT INTO checklists(id,board_id,card_id,title,position) SELECT 'bound-check-'||x,'board','b-todo','Checklist',x FROM n");
    load(db, summary); card = wena_checklist_summary_find(summary, "b-todo"); assert(card && card->checklist_count == 64);
    execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('bound-check-64','board','b-todo','Overflow',64)");
    unchanged_failure(db, summary, before); execute(db, "DELETE FROM checklists WHERE id LIKE 'bound-check-%'");
    execute(db, "WITH RECURSIVE n(x) AS(VALUES(2) UNION ALL SELECT x+1 FROM n WHERE x<1023) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) "
        "SELECT 'bound-item-'||x,'board','b-todo','check','Item',x,1 FROM n");
    load(db, summary); card = wena_checklist_summary_find(summary, "b-todo");
    assert(card && card->progress.total == 1024 && card->progress.finished == 1023);
    execute(db, "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "VALUES('bound-item-1024','board','b-todo','check','Overflow',1024)");
    unchanged_failure(db, summary, before); execute(db, "DELETE FROM checklist_items WHERE id LIKE 'bound-item-%'");
    execute(db, "WITH RECURSIVE n(x) AS(VALUES(4) UNION ALL SELECT x+1 FROM n WHERE x<2047) "
        "INSERT INTO cards SELECT 'bound-card-'||x,'board','lane','list','Card',x,0,1 FROM n");
    many = measure(db, summary); assert(summary->card_count == 2048 && many.statements == 7);
    execute(db, "INSERT INTO cards VALUES('bound-card-2048','board','lane','list','Overflow',2048,0,1)");
    unchanged_failure(db, summary, before); execute(db, "DELETE FROM cards WHERE id LIKE 'bound-card-%'");
    load(db, summary); baseline(summary);
}
static void unrelated_work(sqlite3 *db, WenaChecklistBoardSummary *summary)
{
    Work before, after, unindexed;
    before = measure(db, summary);
    execute(db, "WITH RECURSIVE n(x) AS(VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<10000) "
        "INSERT INTO checklists(id,board_id,card_id,title,position) SELECT 'unrelated-check-'||x,'other','other-card','Other',x FROM n");
    execute(db, "WITH RECURSIVE n(x) AS(VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<10000) "
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) "
        "SELECT 'unrelated-item-'||x,'other','other-card','unrelated-check-'||x,'Other',0 FROM n");
    after = measure(db, summary); baseline(summary);
    printf("Checklist summary query work: %lu/%lu statements, %lu/%lu VM steps, %lu/%lu scan steps\n",
        before.statements, after.statements, before.steps, after.steps, before.scans, after.scans);
    assert(before.statements == 7 && after.statements == before.statements);
    assert(after.steps < before.steps + 100 && after.scans == before.scans);
    execute(db, "DROP INDEX checklist_items_card_order_idx");
    unindexed = measure(db, summary);
    assert(unindexed.scans >= 10000 && unindexed.steps > after.steps + 10000);
    execute(db, "CREATE INDEX checklist_items_card_order_idx ON checklist_items(card_id, checklist_id, position, id);");
    execute(db, "DELETE FROM checklist_items WHERE id LIKE 'unrelated-item-%';DELETE FROM checklists WHERE id LIKE 'unrelated-check-%'");
}
static void contents(sqlite3 *db)
{
    WenaChecklistBoardContents *view, *before;
    const WenaChecklistContents *list;
    Work work;
    int shown;
    view = NULL;
    memset(&work, 0, sizeof(work));
    assert(sqlite3_trace_v2(db, SQLITE_TRACE_PROFILE, trace, &work) == SQLITE_OK);
    assert(wena_checklist_contents_load(db, "actor", "board", &view));
    baseline(&view->summary);
    /* The same six queries plus BEGIN/COMMIT, independent of card count. */
    assert(work.statements == 7);
    memset(&work, 0, sizeof(work));
    list = wena_checklist_contents_find(view, "b-todo");
    assert(list && !list->next && list->version == 1 && list->item_count == 2);
    assert(!strcmp(list->checklist.title, "Mixed"));
    assert(!strcmp(list->items[0].title, "Todo") && !list->items[0].is_finished);
    assert(!strcmp(list->items[1].title, "Done") && list->items[1].is_finished);
    assert(wena_checklist_shown_at_minicard(&list->checklist, 1, &shown) && shown);
    assert(wena_checklist_shown_at_minicard(&list->checklist, 0, &shown) && !shown);
    assert(!wena_checklist_contents_find(view, "a-empty"));
    assert(!wena_checklist_contents_find(view, "other-card"));
    assert(!wena_checklist_contents_find(view, "../invalid"));
    list = wena_checklist_contents_find(view, "c-zero");
    assert(list && !list->item_count && !list->items);
    assert(!work.statements);
    assert(sqlite3_trace_v2(db, 0, NULL, NULL) == SQLITE_OK);
    before = view;
    assert(!wena_checklist_contents_load(db, "missing", "board", &view) && view == before);
    execute(db, "BEGIN");
    assert(!wena_checklist_contents_load(db, "actor", "board", &view) && view == before);
    assert(!sqlite3_get_autocommit(db)); execute(db, "ROLLBACK");
    execute(db, "PRAGMA ignore_check_constraints=ON;UPDATE checklist_items SET title='' WHERE id='item'");
    assert(!wena_checklist_contents_load(db, "actor", "board", &view) && view == before);
    assert(!strcmp(wena_checklist_contents_find(view, "b-todo")->items[0].title, "Todo"));
    execute(db, "UPDATE checklist_items SET title='Todo' WHERE id='item';PRAGMA ignore_check_constraints=OFF");
    commit_failure = 1; assert(sqlite3_set_authorizer(db, authorize, NULL) == SQLITE_OK);
    assert(!wena_checklist_contents_load(db, "actor", "board", &view) && view == before);
    commit_failure = 0; assert(sqlite3_set_authorizer(db, NULL, NULL) == SQLITE_OK);
    execute(db, "INSERT INTO checklists(id,board_id,card_id,title,position,show_on_minicard) VALUES('aaa','board','b-todo','Second',9,0);"
        "INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position,is_finished) VALUES('extra','board','b-todo','aaa','Last',7,0)");
    assert(wena_checklist_contents_load(db, "actor", "board", &view));
    list = wena_checklist_contents_find(view, "b-todo");
    assert(list && !strcmp(list->checklist.id, "check") && list->item_count == 2);
    list = list->next;
    assert(list && !list->next && !strcmp(list->checklist.id, "aaa") && list->item_count == 1);
    assert(!strcmp(list->items[0].title, "Last") && list->items[0].position == 7);
    assert(wena_checklist_shown_at_minicard(&list->checklist, 1, &shown) && !shown);
    execute(db, "DELETE FROM checklist_items WHERE id='extra';DELETE FROM checklists WHERE id='aaa'");
    assert(wena_checklist_contents_load(db, "actor", "other", &view));
    assert(!wena_checklist_contents_find(view, "b-todo"));
    assert(wena_checklist_contents_find(view, "other-card"));
    wena_checklist_contents_free(view); wena_checklist_contents_free(NULL);
}
int main(int argc, char **argv)
{
    FILE *file; long length; unsigned char *bundle; char hash[65];
    sqlite3 *db; WenaChecklistBoardSummary *summary, *before;
    const WenaChecklistCardSummary *card; Work work;
    assert(argc == 3); file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0); length = ftell(file); assert(length > 0); rewind(file);
    bundle = (unsigned char *)malloc((size_t)length); assert(bundle);
    assert(fread(bundle, 1, (size_t)length, file) == (size_t)length); assert(fclose(file) == 0);
    wena_sha256_hex(bundle, (size_t)length, hash);
    assert(wena_sqlite_open(argv[2], bundle, (size_t)length, hash, &db)); seed(db); contents(db);
    summary = wena_checklist_summary_create(); before = wena_checklist_summary_create(); assert(summary && before);
    memset(&work, 0, sizeof(work)); assert(sqlite3_trace_v2(db, SQLITE_TRACE_PROFILE, trace, &work) == SQLITE_OK);
    assert(wena_checklist_summary_load(NULL, "actor", "board", 0, summary));
    assert(!summary->enabled && !summary->card_count && !wena_checklist_summary_find(summary, "b-todo") && !work.statements);
    assert(sqlite3_trace_v2(db, 0, NULL, NULL) == SQLITE_OK);
    execute(db, "INSERT INTO boards VALUES('empty-board','Empty',1)");
    assert(wena_checklist_summary_load(db, "actor", "empty-board", 1, summary));
    assert(summary->enabled && !summary->card_count && summary->board_version == 1);
    load(db, summary); baseline(summary); *before = *summary;
    assert(!wena_checklist_summary_load(db, "missing", "board", 1, summary)); assert(!memcmp(summary, before, sizeof(*summary)));
    assert(!wena_checklist_summary_load(db, "actor", "missing", 1, summary)); assert(!memcmp(summary, before, sizeof(*summary)));
    assert(!wena_checklist_summary_load(db, "actor", "board", 2, summary)); assert(!memcmp(summary, before, sizeof(*summary)));
    execute(db, "BEGIN"); assert(!wena_checklist_summary_load(db, "actor", "board", 1, summary));
    assert(!sqlite3_get_autocommit(db) && !memcmp(summary, before, sizeof(*summary))); execute(db, "ROLLBACK");
    execute(db, "UPDATE checklists SET hide_checked_items=1,hide_all_items=1,show_on_minicard=0");
    load(db, summary); baseline(summary);
    execute(db, "UPDATE checklists SET show_on_minicard=1"); load(db, summary); baseline(summary);
    execute(db, "UPDATE checklists SET hide_checked_items=0,hide_all_items=0,show_on_minicard=NULL");
    corruption(db, summary, before); limits(db, summary, before); unrelated_work(db, summary);
    commit_failure = 1; assert(sqlite3_set_authorizer(db, authorize, NULL) == SQLITE_OK);
    unchanged_failure(db, summary, before); commit_failure = 0; assert(sqlite3_set_authorizer(db, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_open_v2(argv[2], &writer, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    write_once = 1; assert(sqlite3_set_authorizer(db, authorize, NULL) == SQLITE_OK);
    load(db, summary); baseline(summary); assert(!write_once);
    assert(sqlite3_set_authorizer(db, NULL, NULL) == SQLITE_OK); load(db, summary);
    card = wena_checklist_summary_find(summary, "b-todo"); assert(card && card->card_version == 13 && card->progress.is_finished);
    assert(sqlite3_close(writer) == SQLITE_OK); writer = NULL; *before = *summary;
    assert(sqlite3_close(db) == SQLITE_OK); assert(wena_sqlite_open(argv[2], bundle, (size_t)length, hash, &db));
    load(db, summary); assert(!memcmp(summary, before, sizeof(*summary))); assert(wena_sqlite_integrity(db));
    assert(sqlite3_close(db) == SQLITE_OK); free(bundle); wena_checklist_summary_free(summary); wena_checklist_summary_free(before);
    puts("Checklist summary opt-in, complete counts, scope, limits, atomicity and query-work tests passed"); return 0;
}
