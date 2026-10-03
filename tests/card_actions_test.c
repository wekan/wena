/* WeKan's Card Actions popup in Wena: its items in WeKan's order and groups,
 * and each one carried out against SQLite - Move to Top and Bottom reorder the
 * card in its own column, Move Card opens the panel, Archive archives it -
 * with the negatives: missing adapters disable items, unknown cards fail and
 * write nothing. */
#include "../client/features/card_actions.h"
#include "../client/features/card_mutation.h"
#include "../server/sqlite_board.h"
#include "../imports/ui/page_contract.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void execute(sqlite3 *db,const char *sql)
{ assert(sqlite3_exec(db,sql,NULL,NULL,NULL)==SQLITE_OK); }
static int scalar(sqlite3 *db,const char *sql)
{ sqlite3_stmt *q;int value;assert(sqlite3_prepare_v2(db,sql,-1,&q,NULL)==SQLITE_OK);assert(sqlite3_step(q)==SQLITE_ROW);value=sqlite3_column_int(q,0);sqlite3_finalize(q);return value; }
int main(int argc,char **argv)
{
    sqlite3 *db;WenaSqliteBoardSnapshot *snapshot;WenaCardMutation adapter;
    WenaCardMoveState move;WenaCardDetailsState details;WenaBoardLayout layout;
    WenaWekanMenuItem items[WENA_CARD_ACTION_COUNT];int keys;
    assert(argc==2);assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    execute(db,"PRAGMA foreign_keys=ON;"
        "INSERT INTO cards VALUES('one','board','first-lane','first-list','One',0,0,1);"
        "INSERT INTO cards VALUES('two','board','first-lane','first-list','Two',1,0,1);"
        "INSERT INTO cards VALUES('three','board','first-lane','first-list','Three',2,0,1);"
        "INSERT INTO cards VALUES('other','board','second-lane','first-list','Other',0,0,1);");
    snapshot=(WenaSqliteBoardSnapshot*)calloc(1,sizeof(*snapshot));assert(snapshot);
    assert(wena_sqlite_board_load(db,"board",snapshot));
    assert(wena_card_mutation_init(&adapter,db,"actor","board",snapshot->cards,snapshot->card_count));
    memset(&layout,0,sizeof(layout));layout.board=&snapshot->board;layout.cards=snapshot->cards;layout.card_count=snapshot->card_count;
    layout.lists=snapshot->lists;layout.list_count=snapshot->list_count;layout.swimlanes=snapshot->swimlanes;layout.swimlane_count=snapshot->swimlane_count;

    /* Without adapters every item is there, in WeKan's order, and disabled. */
    wena_card_move_init(&move,NULL,NULL,NULL);wena_card_details_init(&details);
    assert(wena_card_actions_items(items,&move,&details)==WENA_CARD_ACTION_COUNT);
    assert(!strcmp(items[0].text,"Move to Top")&&!strcmp(items[1].text,"Move to Bottom"));
    assert(!strcmp(items[2].text,"Move Card")&&!strcmp(items[3].text,"Move Card to Archive"));
    assert(!items[0].separator_before&&!items[1].separator_before&&items[2].separator_before&&items[3].separator_before);
    assert(!items[0].enabled&&!items[1].enabled&&!items[2].enabled&&!items[3].enabled);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_ARCHIVE,&move,&details,&layout,"one")==WENA_CARD_ACTIONS_FAILED);

    wena_card_move_init(&move,wena_card_mutation_load,wena_card_mutation_move,&adapter);
    wena_card_move_set_reorder_adapter(&move,wena_card_mutation_reorder);
    wena_card_details_set_title_adapter(&details,wena_card_mutation_load,wena_card_mutation_save,&adapter);
    wena_card_details_set_archive_adapter(&details,wena_card_mutation_archive);
    assert(wena_card_actions_items(items,&move,&details)==WENA_CARD_ACTION_COUNT);
    assert(items[0].enabled&&items[1].enabled&&items[2].enabled&&items[3].enabled);
    keys=scalar(db,"SELECT count(*) FROM idempotency_keys");

    /* Move to Bottom, then Move to Top: within its own list and swimlane. */
    assert(wena_card_actions_apply(WENA_CARD_ACTION_MOVE_TO_BOTTOM,&move,&details,&layout,"one")==WENA_CARD_ACTIONS_DONE);
    assert(scalar(db,"SELECT position FROM cards WHERE id='one'")==2);
    assert(scalar(db,"SELECT position FROM cards WHERE id='other'")==0);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_MOVE_TO_TOP,&move,&details,&layout,"three")==WENA_CARD_ACTIONS_DONE);
    assert(scalar(db,"SELECT position FROM cards WHERE id='three'")==0);
    assert(!move.visible);
    /* Move Card opens the panel for that card. */
    assert(wena_card_actions_apply(WENA_CARD_ACTION_MOVE,&move,&details,&layout,"two")==WENA_CARD_ACTIONS_OPEN_MOVE);
    assert(move.visible&&!strcmp(move.card_id,"two"));wena_card_move_close(&move);
    /* Archive. */
    assert(wena_card_actions_apply(WENA_CARD_ACTION_ARCHIVE,&move,&details,&layout,"two")==WENA_CARD_ACTIONS_DONE);
    assert(scalar(db,"SELECT archived FROM cards WHERE id='two'")==1);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys+3);
    /* Negative: unknown cards and actions change nothing. */
    assert(wena_card_actions_apply(WENA_CARD_ACTION_MOVE_TO_TOP,&move,&details,&layout,"missing")==WENA_CARD_ACTIONS_FAILED);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_ARCHIVE,&move,&details,&layout,"missing")==WENA_CARD_ACTIONS_FAILED);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_MOVE,&move,&details,&layout,"missing")==WENA_CARD_ACTIONS_FAILED&&!move.visible);
    assert(wena_card_actions_apply(WENA_CARD_ACTION_COUNT,&move,&details,&layout,"one")==WENA_CARD_ACTIONS_FAILED);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys+3);
    wena_card_move_close(&move);assert(sqlite3_close(db)==SQLITE_OK);free(snapshot);
    puts("Card Actions: WeKan's items and groups, top, bottom, move, archive and negatives passed");return 0;
}
