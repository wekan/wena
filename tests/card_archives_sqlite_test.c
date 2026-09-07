#include "../client/features/card_archives.h"
#include <nuklear.h>
#include "../client/features/card_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void frame(WenaCardArchivesState *state, WenaBoardLayout *layout,
    const char *button, const char *option)
{
    struct nk_context context; memset(&context,0,sizeof(context));
    context.button_to_press=button; context.combo_item_to_press=option;
    assert(wena_card_archives_render(&context,state,layout,800,600));
    assert(context.begin_count==context.end_count);
}
int main(int argc, char **argv)
{
    WenaBoard board; WenaCard card; WenaBoardLayout layout;
    WenaCardArchivesState state; WenaCardMutation adapter;
    sqlite3 *db; sqlite3_stmt *query;
    assert(argc==2); assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    assert(sqlite3_exec(db,"PRAGMA foreign_keys=ON; INSERT INTO cards VALUES('card','board','first-lane','first-list','Archived',0,1,1);",NULL,NULL,NULL)==SQLITE_OK);
    assert(wena_board_init(&board,"board","Board",0));
    assert(wena_card_init(&card,"card","board","first-lane","first-list","Archived",0,1));
    memset(&layout,0,sizeof(layout)); layout.board=&board; layout.cards=&card; layout.card_count=1;
    assert(wena_card_mutation_init(&adapter,db,"actor","board",&card,1));
    wena_card_archives_init(&state,wena_card_mutation_load_archived,wena_card_mutation_restore,&adapter);
    assert(wena_card_archives_open(&state,&layout));
    frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible && card.archived);
    assert(wena_card_archives_open(&state,&layout));
    assert(sqlite3_exec(db,"UPDATE cards SET version=2",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Restore",NULL);
    assert(state.visible && state.error && card.archived && !strcmp(state.card_id,"card"));
    frame(&state,&layout,"Cancel",NULL);
    assert(wena_card_archives_open(&state,&layout));
    assert(sqlite3_exec(db,"CREATE TRIGGER reject_restore BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'fail'); END;",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Restore",NULL);
    assert(state.error && state.visible && card.archived);
    assert(sqlite3_exec(db,"DROP TRIGGER reject_restore",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Restore",NULL);
    assert(state.visible && !card.archived && state.card_id[0]==0);
    frame(&state,&layout,"Restore",NULL);
    assert(sqlite3_close(db)==SQLITE_OK);
    assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    assert(sqlite3_prepare_v2(db,"SELECT archived,version,(SELECT count(*) FROM idempotency_keys) FROM cards WHERE id='card'",-1,&query,NULL)==SQLITE_OK);
    assert(sqlite3_step(query)==SQLITE_ROW);
    assert(sqlite3_column_int(query,0)==0);
    assert(sqlite3_column_int(query,1)==3);
    assert(sqlite3_column_int(query,2)==1);
    sqlite3_finalize(query); assert(sqlite3_close(db)==SQLITE_OK);
    puts("Card Archives UI and SQLite restore integration passed");
    return 0;
}
