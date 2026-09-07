#include "../client/features/card_move.h"
#include <nuklear.h>
#include "../client/features/card_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void frame(WenaCardMoveState *state, WenaBoardLayout *layout,
    const char *button, const char *option)
{
    struct nk_context context; memset(&context,0,sizeof(context));
    context.button_to_press=button; context.combo_item_to_press=option;
    assert(wena_card_move_render(&context,state,layout,800,600));
    assert(context.begin_count==context.end_count);
}
int main(int argc, char **argv)
{
    WenaBoard board; WenaList lists[2]; WenaSwimlane lanes[2]; WenaCard card;
    WenaBoardLayout layout; WenaCardMoveState state; WenaCardMutation adapter;
    sqlite3 *db; sqlite3_stmt *query;
    assert(argc==2); assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    assert(sqlite3_exec(db,"PRAGMA foreign_keys=ON; INSERT INTO cards VALUES('card','board','first-lane','first-list','Card',0,0,1);",NULL,NULL,NULL)==SQLITE_OK);
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_list_init(&lists[0],"first-list","board","","Repeated",0,0));
    assert(wena_list_init(&lists[1],"second-list","board","","Repeated",1,0));
    assert(wena_swimlane_init(&lanes[0],"first-lane","board","Repeated",0,0));
    assert(wena_swimlane_init(&lanes[1],"second-lane","board","Repeated",1,0));
    assert(wena_card_init(&card,"card","board","first-lane","first-list","Card",0,0));
    memset(&layout,0,sizeof(layout));
    layout.board=&board; layout.lists=lists; layout.list_count=2;
    layout.swimlanes=lanes; layout.swimlane_count=2; layout.cards=&card; layout.card_count=1;
    assert(wena_card_mutation_init(&adapter,db,"actor","board",&card,1));
    wena_card_move_init(&state,wena_card_mutation_load,wena_card_mutation_move,&adapter);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"Repeated [second-lane]");
    frame(&state,&layout,NULL,"Repeated [second-list]");
    frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible && !strcmp(card.list_id,"first-list"));
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"Repeated [second-lane]");
    frame(&state,&layout,NULL,"Repeated [second-list]");
    assert(sqlite3_exec(db,"UPDATE cards SET version=2",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Save",NULL);
    assert(state.visible && state.error && !strcmp(card.list_id,"first-list"));
    frame(&state,&layout,"Cancel",NULL);
    assert(wena_card_move_open(&state,&layout,"card"));
    frame(&state,&layout,NULL,"Repeated [second-lane]");
    frame(&state,&layout,NULL,"Repeated [second-list]");
    assert(sqlite3_exec(db,"CREATE TRIGGER reject_move BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'fail'); END;",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Save",NULL);
    assert(state.error && state.visible && !strcmp(card.list_id,"first-list"));
    assert(sqlite3_exec(db,"DROP TRIGGER reject_move",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Save",NULL);
    assert(!state.visible && !strcmp(card.list_id,"second-list"));
    assert(!strcmp(card.swimlane_id,"second-lane"));
    assert(sqlite3_close(db)==SQLITE_OK);
    assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    assert(sqlite3_prepare_v2(db,"SELECT list_id,swimlane_id,version FROM cards WHERE id='card'",-1,&query,NULL)==SQLITE_OK);
    assert(sqlite3_step(query)==SQLITE_ROW);
    assert(!strcmp((const char*)sqlite3_column_text(query,0),"second-list"));
    assert(!strcmp((const char*)sqlite3_column_text(query,1),"second-lane"));
    assert(sqlite3_column_int(query,2)==3);
    sqlite3_finalize(query); assert(sqlite3_close(db)==SQLITE_OK);
    puts("Card movement UI and SQLite integration passed");
    return 0;
}
