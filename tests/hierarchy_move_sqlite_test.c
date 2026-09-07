#include "../client/features/hierarchy_move.h"
#include "../client/features/hierarchy_move_mutation.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void frame(WenaHierarchyMoveState *state,WenaBoardLayout *layout,
    const char *button,const char *option)
{
    struct nk_context context; memset(&context,0,sizeof(context));
    context.button_to_press=button; context.combo_item_to_press=option;
    assert(wena_hierarchy_move_render(&context,state,layout,800,600));
}
static int scalar(sqlite3 *db,const char *sql)
{
    sqlite3_stmt *query; int value;
    assert(sqlite3_prepare_v2(db,sql,-1,&query,NULL)==SQLITE_OK);
    assert(sqlite3_step(query)==SQLITE_ROW); value=sqlite3_column_int(query,0);
    sqlite3_finalize(query); return value;
}
static void test_kind(sqlite3 *db,WenaSqliteBoardSnapshot *snapshot,WenaHierarchyKind kind)
{
    WenaHierarchyMoveState state; WenaHierarchyMoveMutation adapter;
    WenaBoardLayout layout; const char *id,*option,*version_sql,*position_sql;
    int keys;
    memset(&layout,0,sizeof(layout)); layout.board=&snapshot->board;
    layout.lists=snapshot->lists; layout.list_count=snapshot->list_count;
    layout.swimlanes=snapshot->swimlanes; layout.swimlane_count=snapshot->swimlane_count;
    id=kind==WENA_HIERARCHY_LIST?"first-list":"first-lane";
    option=kind==WENA_HIERARCHY_LIST?"2. Second [second-list]":"2. Second [second-lane]";
    version_sql=kind==WENA_HIERARCHY_LIST?"SELECT version FROM lists WHERE id='first-list'":"SELECT version FROM swimlanes WHERE id='first-lane'";
    position_sql=kind==WENA_HIERARCHY_LIST?"SELECT position FROM lists WHERE id='first-list'":"SELECT position FROM swimlanes WHERE id='first-lane'";
    assert(wena_hierarchy_move_mutation_init(&adapter,db,"actor","board",snapshot));
    wena_hierarchy_move_init(&state,wena_hierarchy_move_mutation_load,
        wena_hierarchy_move_mutation_move,&adapter);
    keys=scalar(db,"SELECT count(*) FROM idempotency_keys");
    assert(wena_hierarchy_move_open(&state,&layout,kind,id));
    frame(&state,&layout,"Save",NULL);
    assert(!state.visible && scalar(db,version_sql)==1);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys);
    assert(wena_hierarchy_move_open(&state,&layout,kind,id));
    frame(&state,&layout,NULL,option); frame(&state,&layout,"Cancel",NULL);
    assert(!state.visible && scalar(db,position_sql)==0);
    assert(wena_hierarchy_move_open(&state,&layout,kind,id));
    frame(&state,&layout,NULL,option);
    assert(sqlite3_exec(db,kind==WENA_HIERARCHY_LIST?
        "UPDATE lists SET version=2 WHERE id='first-list'":
        "UPDATE swimlanes SET version=2 WHERE id='first-lane'",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Save",NULL);
    assert(state.error && state.visible && scalar(db,position_sql)==0);
    frame(&state,&layout,"Cancel",NULL);
    assert(wena_hierarchy_move_open(&state,&layout,kind,id));
    frame(&state,&layout,NULL,option);
    assert(sqlite3_exec(db,"CREATE TRIGGER reject_move BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'failure'); END;",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Save",NULL);
    assert(state.error && state.target_position==1 && scalar(db,position_sql)==0);
    assert(sqlite3_exec(db,"DROP TRIGGER reject_move",NULL,NULL,NULL)==SQLITE_OK);
    frame(&state,&layout,"Save",NULL);
    assert(!state.visible && scalar(db,position_sql)==1 && scalar(db,version_sql)==3);
    assert(scalar(db,"SELECT count(*) FROM idempotency_keys")==keys+1);
    if(kind==WENA_HIERARCHY_LIST) {
        assert(!strcmp(snapshot->lists[1].id,id) && snapshot->lists[1].sort==1);
    } else assert(!strcmp(snapshot->swimlanes[1].id,id) && snapshot->swimlanes[1].sort==1);
}
int main(int argc,char **argv)
{
    sqlite3 *db; WenaSqliteBoardSnapshot *snapshot;
    assert(argc==2); assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    assert(sqlite3_exec(db,"PRAGMA foreign_keys=ON",NULL,NULL,NULL)==SQLITE_OK);
    snapshot=(WenaSqliteBoardSnapshot*)calloc(1,sizeof(*snapshot)); assert(snapshot);
    assert(wena_sqlite_board_load(db,"board",snapshot));
    test_kind(db,snapshot,WENA_HIERARCHY_LIST);
    test_kind(db,snapshot,WENA_HIERARCHY_SWIMLANE);
    assert(sqlite3_close(db)==SQLITE_OK);
    assert(sqlite3_open(argv[1],&db)==SQLITE_OK);
    assert(wena_sqlite_board_load(db,"board",snapshot));
    assert(!strcmp(snapshot->lists[1].id,"first-list"));
    assert(!strcmp(snapshot->swimlanes[1].id,"first-lane"));
    assert(sqlite3_close(db)==SQLITE_OK); free(snapshot);
    puts("Hierarchy reorder UI SQLite no-op, conflict, rollback and reopen passed");
    return 0;
}
