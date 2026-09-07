#include "../client/features/card_move.h"
#include "../client/features/card_mutation.h"
#include "../server/sqlite_board.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void execute(sqlite3 *db,const char *sql)
{ assert(sqlite3_exec(db,sql,NULL,NULL,NULL)==SQLITE_OK); }
static int scalar(sqlite3 *db,const char *sql)
{ sqlite3_stmt *q;int value;assert(sqlite3_prepare_v2(db,sql,-1,&q,NULL)==SQLITE_OK);assert(sqlite3_step(q)==SQLITE_ROW);value=sqlite3_column_int(q,0);sqlite3_finalize(q);return value; }
static void frame(WenaCardMoveState *state,WenaBoardLayout *layout,const char *button,const char *option)
{ struct nk_context c;memset(&c,0,sizeof(c));c.button_to_press=button;c.combo_item_to_press=option;assert(wena_card_move_render(&c,state,layout,800,600)); }
int main(int argc,char **argv)
{
    sqlite3 *db;WenaSqliteBoardSnapshot *snapshot;WenaCardMutation adapter;
    WenaCardMoveState state;WenaBoardLayout layout;int keys;
    assert(argc==2);assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    execute(db,"PRAGMA foreign_keys=ON;"
        "INSERT INTO cards VALUES('out','board','first-lane','first-list','Out',0,0,1);"
        "INSERT INTO cards VALUES('old','board','first-lane','first-list','Old',1,1,1);"
        "INSERT INTO cards VALUES('card','board','first-lane','first-list','Card',2,0,1);"
        "INSERT INTO cards VALUES('last','board','first-lane','first-list','Last',3,0,1);");
    snapshot=(WenaSqliteBoardSnapshot*)calloc(1,sizeof(*snapshot));assert(snapshot);
    assert(wena_sqlite_board_load(db,"board",snapshot));
    assert(wena_card_mutation_init(&adapter,db,"actor","board",snapshot->cards,snapshot->card_count));
    /* Produce a normal sparse source column using the existing append move. */
    assert(wena_card_mutation_move(&adapter,"board","out",1,"second-list","first-lane"));
    assert(scalar(db,"SELECT min(position) FROM cards WHERE list_id='first-list'")==1);
    memset(&layout,0,sizeof(layout));layout.board=&snapshot->board;layout.cards=snapshot->cards;layout.card_count=snapshot->card_count;
    layout.lists=snapshot->lists;layout.list_count=snapshot->list_count;layout.swimlanes=snapshot->swimlanes;layout.swimlane_count=snapshot->swimlane_count;
    wena_card_move_init(&state,wena_card_mutation_load,wena_card_mutation_move,&adapter);
    wena_card_move_set_reorder_adapter(&state,wena_card_mutation_reorder);
    keys=scalar(db,"SELECT count(*) FROM idempotency_keys");
    assert(wena_card_move_open(&state,&layout,"card"));assert(state.order_count==3);
    frame(&state,&layout,NULL,"2. Card [card]");frame(&state,&layout,"Save",NULL);
    assert(!state.visible&&scalar(db,"SELECT position FROM cards WHERE id='card'")==2);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"1. Old (Archived) [old]");frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible&&state.order==NULL);
    assert(wena_card_move_open(&state,&layout,"card"));frame(&state,&layout,NULL,"1. Old (Archived) [old]");
    execute(db,"UPDATE cards SET version=2 WHERE id='card'");frame(&state,&layout,"Save",NULL);
    assert(state.error&&state.visible&&state.reorder_choice==1);
    frame(&state,&layout,"Cancel",NULL);
    assert(wena_card_move_open(&state,&layout,"card"));frame(&state,&layout,NULL,"1. Old (Archived) [old]");
    execute(db,"UPDATE cards SET position=4 WHERE id='old'");frame(&state,&layout,"Save",NULL);
    assert(state.error&&state.visible&&scalar(db,"SELECT position FROM cards WHERE id='card'")==2);
    execute(db,"UPDATE cards SET position=1 WHERE id='old';CREATE TRIGGER fail_reorder BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'failure');END;");
    frame(&state,&layout,"Save",NULL);assert(state.error&&state.visible);
    assert(scalar(db,"SELECT position FROM cards WHERE id='card'")==2);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys);
    execute(db,"DROP TRIGGER fail_reorder");frame(&state,&layout,"Save",NULL);
    assert(!state.visible&&state.order==NULL);
    assert(scalar(db,"SELECT position FROM cards WHERE id='card'")==0);
    assert(scalar(db,"SELECT position FROM cards WHERE id='old'")==1);
    assert(scalar(db,"SELECT archived FROM cards WHERE id='old'")==1);
    assert(scalar(db,"SELECT position FROM cards WHERE id='last'")==2);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
    wena_card_move_close(&state);assert(sqlite3_close(db)==SQLITE_OK);
    assert(sqlite3_open(argv[1],&db)==SQLITE_OK);assert(wena_sqlite_board_load(db,"board",snapshot));
    assert(scalar(db,"SELECT position FROM cards WHERE id='card'")==0);
    assert(scalar(db,"SELECT version FROM cards WHERE id='card'")==3);
    assert(scalar(db,"SELECT position FROM cards WHERE id='out'")==0);
    assert(sqlite3_close(db)==SQLITE_OK);free(snapshot);
    puts("Indexed card movement UI gap, archive, no-op, conflict, rollback and reopen passed");return 0;
}
