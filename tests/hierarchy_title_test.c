#include "../client/features/card_archives.h"
#include "../client/features/card_selection_panel.h"
#include "../client/features/labels/mutation.h"
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/hierarchy_title.h"
#include "../client/features/hierarchy_mutation.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void execute(sqlite3 *database, const char *sql)
{
    assert(sqlite3_exec(database, sql, NULL, NULL, NULL) == SQLITE_OK);
}

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}

static void render(struct nk_context *context, WenaHierarchyTitleState *state,
                    WenaBoardLayout *layout)
{
    const struct nk_command *command;
    if (state->visible)
        assert(wena_hierarchy_title_render(context, state, layout, 640.0f, 480.0f));
    assert(context->current == NULL);
    nk_foreach(command,context){(void)command;}
}

static void frame(struct nk_context *context, WenaHierarchyTitleState *state,
                   WenaBoardLayout *layout)
{
    nk_clear(context); nk_input_begin(context); nk_input_end(context);
    render(context, state, layout);
}

static struct nk_vec2 label_center(struct nk_context *context, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, context) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                            (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing control %s\n", label);
    assert(0);
    return nk_vec2(0.0f, 0.0f);
}

static void click_at(struct nk_context *context, WenaHierarchyTitleState *state,
                      WenaBoardLayout *layout, struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(context); nk_input_begin(context);
        nk_input_motion(context, (int)point.x, (int)point.y);
        nk_input_button(context, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(context);
        render(context, state, layout);
    }
}

static void click(struct nk_context *context, WenaHierarchyTitleState *state,
                   WenaBoardLayout *layout, const char *label)
{
    click_at(context, state, layout, label_center(context, label));
}

/* The desktop consumes intents in the frame where a mouse press emits them. */
static unsigned int click_action(struct nk_context *context,WenaHierarchyTitleState *state,
    WenaBoardLayout *layout,const char *label)
{
    struct nk_vec2 point;int down;unsigned int action;action=0;point=label_center(context,label);
    for(down=1;down>=0;--down){
        nk_clear(context);nk_input_begin(context);
        nk_input_motion(context,(int)point.x,(int)point.y);
        nk_input_button(context,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);
        nk_input_end(context);render(context,state,layout);
        if(state->requested_action){assert(!action);action=state->requested_action;}
    }
    return action;
}
static int scalar(sqlite3 *database, const char *sql)
{
    sqlite3_stmt *statement;
    int value;
    assert(sqlite3_prepare_v2(database, sql, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    value = sqlite3_column_int(statement, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return value;
}

static void creation_tests(sqlite3 *database, WenaHierarchyMutation *adapter,
    WenaSqliteBoardSnapshot *snapshot, struct nk_context *context,
    WenaHierarchyTitleState *state, WenaBoardLayout *layout)
{
    size_t count;
    int keys;
    int index;
    char full_title[129];
    char title[129];
    unsigned long version;
    WenaHierarchyKind kind;
    const char *created_id;
    const char *count_sql;

    assert(!wena_hierarchy_mutation_create(adapter, "board", WENA_HIERARCHY_BOARD, "No board creation"));
    assert(!wena_hierarchy_mutation_create(adapter, "other", WENA_HIERARCHY_LIST, "Wrong scope"));
    for (index = 0; index < 2; ++index) {
        kind = index == 0 ? WENA_HIERARCHY_LIST : WENA_HIERARCHY_SWIMLANE;
        count_sql = index == 0 ? "SELECT count(*) FROM lists" : "SELECT count(*) FROM swimlanes";
        count = index == 0 ? snapshot->list_count : snapshot->swimlane_count;
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, ""));
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "Bad\n"));
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "\300\257"));
        assert(!wena_hierarchy_mutation_create_request(adapter, "board", kind, 0, "Invalid request"));
        assert(wena_hierarchy_mutation_create_request(adapter, "board", kind, 44, "Created & + % = \303\244"));
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 1);
        created_id = index == 0 ? snapshot->lists[count].id : snapshot->swimlanes[count].id;
        assert(created_id[0] && strcmp(created_id, "pending") != 0);
        assert(index == 0 ? snapshot->lists[count].sort == 1.0 : snapshot->swimlanes[count].sort == 1.0);
        if (index == 0) assert(snapshot->lists[count].swimlane_id[0] == '\0');
        assert(wena_hierarchy_mutation_load(adapter, "board", kind, created_id, title, sizeof(title), &version));
        assert(version == 1 && strcmp(title, "Created & + % = \303\244") == 0);
        assert(!wena_hierarchy_mutation_create_request(adapter, "board", kind, 44, "Replay"));
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 1);
        keys = scalar(database, "SELECT count(*) FROM idempotency_keys");
        execute(database, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'injected'); END;");
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "Rollback"));
        execute(database, "DROP TRIGGER reject_metadata;");
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 1);
        assert(scalar(database, count_sql) == (int)count + 2);
        assert(scalar(database, "SELECT count(*) FROM idempotency_keys") == keys);
        if (index == 0) snapshot->list_count = WENA_SQLITE_BOARD_MAX_LISTS;
        else snapshot->swimlane_count = WENA_SQLITE_BOARD_MAX_SWIMLANES;
        assert(!wena_hierarchy_mutation_create(adapter, "board", kind, "Capacity"));
        assert(scalar(database, count_sql) == (int)count + 2);
        if (index == 0) snapshot->list_count = count + 1;
        else snapshot->swimlane_count = count + 1;
        memset(full_title, 'x', sizeof(full_title) - 1); full_title[128] = '\0';
        assert(wena_hierarchy_mutation_create(adapter, "board", kind, full_title));
        assert((index == 0 ? snapshot->list_count : snapshot->swimlane_count) == count + 2);
    }
    layout->list_count = snapshot->list_count;
    layout->swimlane_count = snapshot->swimlane_count;
    wena_hierarchy_title_set_create_adapter(state, wena_hierarchy_mutation_create);
    assert(!wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_BOARD));
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_LIST));
    assert(state->creating && state->visible && state->title_length == 0);
    count = snapshot->list_count;
    frame(context, state, layout);
    click(context, state, layout, "Save");
    assert(state->visible && state->error && snapshot->list_count == count);
    click(context, state, layout, "Cancel");
    assert(!state->visible && snapshot->list_count == count);
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_LIST));
    frame(context, state, layout);
    click_at(context, state, layout, nk_vec2(200.0f, 145.0f));
    nk_clear(context); nk_input_begin(context);
    nk_input_unicode(context, (nk_rune)'N'); nk_input_end(context);
    render(context, state, layout);
    assert(state->title_length == 1);
    click(context, state, layout, "Save");
    assert(!state->visible && snapshot->list_count == count + 1);
    assert(strcmp(snapshot->lists[count].title, "N") == 0);
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_SWIMLANE));
    count = snapshot->swimlane_count;
    frame(context, state, layout);
    click_at(context, state, layout, nk_vec2(200.0f, 145.0f));
    for (index = 0; index < 160; ++index) {
        nk_clear(context); nk_input_begin(context);
        nk_input_unicode(context, (nk_rune)'x'); nk_input_end(context);
        render(context, state, layout);
    }
    assert(state->title_length ==
        WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY) - 1);
    click(context, state, layout, "Save");
    assert(state->visible && state->error && snapshot->swimlane_count == count);
    click(context, state, layout, "Cancel");
    assert(!state->visible && snapshot->swimlane_count == count);
    assert(wena_hierarchy_title_open_create(state, layout, WENA_HIERARCHY_LIST));
    strcpy(snapshot->board.id, "other");
    assert(!wena_hierarchy_title_render(context, state, layout, 640.0f, 480.0f));
    assert(!state->visible);
    strcpy(snapshot->board.id, "board");
}

static void editor_key(struct nk_context *context,WenaHierarchyTitleState *state,WenaBoardLayout *layout,enum nk_keys key,int down)
{
 nk_clear(context);nk_input_begin(context);nk_input_key(context,key,down);nk_input_end(context);render(context,state,layout);
}
static void editor_text(struct nk_context *context,WenaHierarchyTitleState *state,WenaBoardLayout *layout,const char *text)
{
 size_t i;editor_key(context,state,layout,NK_KEY_TEXT_SELECT_ALL,1);editor_key(context,state,layout,NK_KEY_TEXT_SELECT_ALL,0);
 for(i=0;text[i];++i){nk_clear(context);nk_input_begin(context);nk_input_unicode(context,(nk_rune)(unsigned char)text[i]);nk_input_end(context);render(context,state,layout);}
}
static void open_color(struct nk_context *context,WenaHierarchyTitleState *state,WenaBoardLayout *layout,WenaHierarchyKind kind,const char *id)
{
 assert(wena_hierarchy_title_open(state,layout,kind,id));frame(context,state,layout);click(context,state,layout,"Select Color");
 assert(state->visible&&state->editing_color);frame(context,state,layout);
}
static void color_tests(sqlite3 *db,WenaHierarchyMutation *adapter,WenaSqliteBoardSnapshot *snapshot,
    struct nk_context *context,WenaHierarchyTitleState *state,WenaBoardLayout *layout)
{
 int k,keys;WenaHierarchyKind kind;const char *id;char *cached;char query[128];struct nk_vec2 point;
 wena_hierarchy_title_set_color_adapters(state,wena_hierarchy_mutation_color_load,wena_hierarchy_mutation_color_save);
 for(k=0;k<2;++k){kind=k?WENA_HIERARCHY_SWIMLANE:WENA_HIERARCHY_LIST;id=k?"lane":"list";
  cached=k?snapshot->swimlanes[0].color:snapshot->lists[0].color;
  open_color(context,state,layout,kind,id);click(context,state,layout,"red");click(context,state,layout,"Save");
  assert(!state->visible&&!strcmp(cached,"red"));
  keys=scalar(db,"SELECT count(*) FROM idempotency_keys");
  open_color(context,state,layout,kind,id);click(context,state,layout,"green");click(context,state,layout,"Cancel");
  assert(!state->visible&&!strcmp(cached,"red")&&scalar(db,"SELECT count(*) FROM idempotency_keys")==keys);
  open_color(context,state,layout,kind,id);click(context,state,layout,"Default");click(context,state,layout,"Save");
  assert(!state->visible&&!cached[0]);
  open_color(context,state,layout,kind,id);point=label_center(context,"Custom color");point.y+=28;click_at(context,state,layout,point);
  editor_text(context,state,layout,"#1234567");assert(state->color_input.color_length==8);
  keys=scalar(db,"SELECT count(*) FROM idempotency_keys");
  editor_key(context,state,layout,NK_KEY_ENTER,1);assert(state->visible&&scalar(db,"SELECT count(*) FROM idempotency_keys")==keys);
  editor_key(context,state,layout,NK_KEY_ENTER,0);click(context,state,layout,"Save");assert(state->error&&state->visible&&!cached[0]);
  click_at(context,state,layout,point);editor_text(context,state,layout,"#123AbC");
  execute(db,"CREATE TRIGGER color_late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
  click(context,state,layout,"Save");assert(state->visible&&state->error&&!cached[0]);execute(db,"DROP TRIGGER color_late");
  click(context,state,layout,"Save");assert(!state->visible&&!strcmp(cached,"#123AbC"));
  open_color(context,state,layout,kind,id);click(context,state,layout,"white");
  sprintf(query,"UPDATE %s SET version=version+1 WHERE id='%s'",k?"swimlanes":"lists",id);execute(db,query);
  click(context,state,layout,"Save");assert(state->visible&&state->error&&!strcmp(cached,"#123AbC"));
  click(context,state,layout,"Cancel");open_color(context,state,layout,kind,id);
  editor_key(context,state,layout,NK_KEY_TEXT_RESET_MODE,1);assert(!state->visible);
  editor_key(context,state,layout,NK_KEY_TEXT_RESET_MODE,0);
  assert(wena_hierarchy_title_open(state,layout,kind,id));
  memset(state->title_input,'x',sizeof(state->title_input));state->title_length=(int)sizeof(state->title_input);
  frame(context,state,layout);click(context,state,layout,"Select Color");assert(state->editing_color);
  frame(context,state,layout);assert(!strcmp(state->original_title,k?snapshot->swimlanes[0].title:snapshot->lists[0].title));
  click(context,state,layout,"Cancel");
  /* A failed read cannot open a color editor or discard the title draft. */
  assert(wena_hierarchy_title_open(state,layout,kind,id));frame(context,state,layout);
  strcpy(adapter->actor_id,"missing");click(context,state,layout,"Select Color");
  assert(state->visible&&!state->editing_color&&state->error);strcpy(adapter->actor_id,"actor");
  click(context,state,layout,"Cancel");
 }
}

static void open_wip(struct nk_context *context,WenaHierarchyTitleState *state,WenaBoardLayout *layout)
{
 assert(wena_hierarchy_title_open(state,layout,WENA_HIERARCHY_LIST,"list"));frame(context,state,layout);
 click(context,state,layout,"Edit WIP Limit");assert(state->visible&&state->editing_wip);frame(context,state,layout);
}
static void wip_tests(sqlite3 *db,WenaHierarchyMutation *adapter,WenaSqliteBoardSnapshot *snapshot,
 struct nk_context *context,WenaHierarchyTitleState *state,WenaBoardLayout *layout)
{
 struct nk_vec2 point;int keys;
 execute(db,"INSERT INTO cards VALUES('wip1','board','lane','list','One',0,0,1),('wip2','board','lane','list','Two',1,0,1),('wip3','board','lane','list','Three',2,0,1)");
 assert(wena_sqlite_board_load(db,"board",snapshot));
 wena_hierarchy_title_set_wip_adapters(state,wena_hierarchy_mutation_wip_load,wena_hierarchy_mutation_wip_save);
 open_wip(context,state,layout);assert(state->wip_count==3&&state->wip_limit.value==1);
 click(context,state,layout,"Enable WIP Limit");assert(!state->visible&&snapshot->lists[0].wip_limit.enabled&&snapshot->lists[0].wip_limit.value==3);
 open_wip(context,state,layout);click(context,state,layout,"Soft WIP Limit");assert(!state->visible&&snapshot->lists[0].wip_limit.soft);
 open_wip(context,state,layout);point=label_center(context,"Soft WIP Limit");point.y+=32;click_at(context,state,layout,point);
 editor_text(context,state,layout,"1");click(context,state,layout,"Save");assert(!state->visible&&snapshot->lists[0].wip_limit.value==1);
 open_wip(context,state,layout);click(context,state,layout,"Soft WIP Limit");assert(!state->visible&&!snapshot->lists[0].wip_limit.soft&&snapshot->lists[0].wip_limit.value==3);
 open_wip(context,state,layout);point=label_center(context,"Soft WIP Limit");point.y+=32;click_at(context,state,layout,point);
 editor_text(context,state,layout,"100");assert(state->wip_length==3);keys=scalar(db,"SELECT count(*) FROM idempotency_keys");
 editor_key(context,state,layout,NK_KEY_ENTER,1);editor_key(context,state,layout,NK_KEY_ENTER,0);
 assert(state->visible&&scalar(db,"SELECT count(*) FROM idempotency_keys")==keys);
 click(context,state,layout,"Save");assert(state->visible&&state->error&&snapshot->lists[0].wip_limit.value==3);
 click_at(context,state,layout,point);editor_text(context,state,layout,"2");click(context,state,layout,"Save");assert(state->visible&&state->error);
 click_at(context,state,layout,point);editor_text(context,state,layout,"5");
 execute(db,"CREATE TRIGGER wip_late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
 click(context,state,layout,"Save");assert(state->visible&&state->error&&snapshot->lists[0].wip_limit.value==3);execute(db,"DROP TRIGGER wip_late");
 click(context,state,layout,"Save");assert(!state->visible&&snapshot->lists[0].wip_limit.value==5);
 open_wip(context,state,layout);point=label_center(context,"Soft WIP Limit");point.y+=32;click_at(context,state,layout,point);editor_text(context,state,layout,"9");
 execute(db,"UPDATE lists SET version=version+1 WHERE id='list'");click(context,state,layout,"Save");assert(state->visible&&state->error&&snapshot->lists[0].wip_limit.value==5);
 click(context,state,layout,"Cancel");assert(!state->visible);
 open_wip(context,state,layout);editor_key(context,state,layout,NK_KEY_TEXT_RESET_MODE,1);assert(!state->visible);editor_key(context,state,layout,NK_KEY_TEXT_RESET_MODE,0);
 assert(wena_hierarchy_title_open(state,layout,WENA_HIERARCHY_LIST,"list"));frame(context,state,layout);
 strcpy(adapter->actor_id,"missing");click(context,state,layout,"Edit WIP Limit");assert(state->visible&&state->error&&!state->editing_wip);strcpy(adapter->actor_id,"actor");
 click(context,state,layout,"Cancel");
}

static void archives_frame(struct nk_context *context,WenaCardArchivesState *state,WenaBoardLayout *layout)
{
 const struct nk_command *command;
 assert(wena_card_archives_render(context,state,layout,640,480));
 nk_foreach(command,context){(void)command;}
}
static void archives_click(struct nk_context *context,WenaCardArchivesState *state,WenaBoardLayout *layout,const char *label)
{
 struct nk_vec2 point;int down;point=label_center(context,label);
 for(down=1;down>=0;--down){nk_clear(context);nk_input_begin(context);
 nk_input_motion(context,(int)point.x,(int)point.y);nk_input_button(context,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);
 nk_input_end(context);archives_frame(context,state,layout);}
}

static void selection_frame(struct nk_context *ctx,WenaCardSelectionPanel *panel,WenaSqliteBoardSnapshot *snapshot)
{
 nk_clear(ctx);nk_input_begin(ctx);nk_input_end(ctx);
 assert(wena_card_selection_panel_render(ctx,panel,snapshot->cards,snapshot->card_count,"board",640,480));
}
static void selection_click(struct nk_context *ctx,WenaCardSelectionPanel *panel,WenaSqliteBoardSnapshot *snapshot,const char *label)
{
 struct nk_vec2 point;int down;point=label_center(ctx,label);
 for(down=1;down>=0;--down){
  nk_clear(ctx);nk_input_begin(ctx);nk_input_motion(ctx,(int)point.x,(int)point.y);
  nk_input_button(ctx,NK_BUTTON_LEFT,(int)point.x,(int)point.y,down);nk_input_end(ctx);
  if(panel->visible)assert(wena_card_selection_panel_render(ctx,panel,snapshot->cards,snapshot->card_count,"board",640,480));
 }
}
typedef struct LabelUiAdapter {WenaLabelMutation mutation;WenaLabelBoardSnapshot *badges;} LabelUiAdapter;
static int label_ui_load(void *data,const char *board,const WenaId *ids,size_t count,WenaLabelSelectionSnapshot **out)
{LabelUiAdapter *adapter;adapter=(LabelUiAdapter*)data;return wena_label_mutation_selected_load(&adapter->mutation,board,ids,count,out);}
static int label_ui_save(void *data,const char *board,const WenaLabelSelectionSnapshot *selection,const char *label,int assign)
{LabelUiAdapter *adapter;adapter=(LabelUiAdapter*)data;return wena_label_mutation_selected_save(&adapter->mutation,board,selection,label,assign,adapter->badges);}
static void selection_labels(struct nk_context *ctx,WenaCardSelectionPanel *panel,WenaSqliteBoardSnapshot *snapshot,sqlite3 *db)
{
    LabelUiAdapter adapter;WenaLabelBoardSnapshot *before,*fresh;int i;char query[256];
    assert(wena_label_mutation_init(&adapter.mutation,db,"actor","board"));
    adapter.badges=wena_label_board_snapshot_create();before=wena_label_board_snapshot_create();fresh=wena_label_board_snapshot_create();assert(adapter.badges&&before&&fresh);
    for(i=0;i<6;++i){sprintf(query,"INSERT INTO labels VALUES('board','label%d','Label %d','blue',%d,1,0,0)",i,i,i);execute(db,query);}
    execute(db,"INSERT INTO card_labels VALUES('board','outside','label5')");
    assert(wena_label_mutation_load_board(&adapter.mutation,"board",adapter.badges));*before=*adapter.badges;
    wena_card_selection_panel_set_labels(panel,label_ui_load,label_ui_save,&adapter);
    assert(wena_card_selection_panel_open(panel,snapshot->cards,snapshot->card_count,"board","list",NULL));
    selection_frame(ctx,panel,snapshot);
    strcpy(adapter.mutation.actor_id,"missing");selection_click(ctx,panel,snapshot,"Labels");
    assert(panel->archive_error&&!panel->labels&&panel->selection->count==4);strcpy(adapter.mutation.actor_id,"actor");
    selection_click(ctx,panel,snapshot,"Labels");assert(panel->labels&&panel->labels->assigned_counts[5]==1);
    selection_click(ctx,panel,snapshot,"Next Page");assert(panel->table.page==1);
    selection_click(ctx,panel,snapshot,"Label 5 [label5]");assert(panel->selected_label==6);
    nk_clear(ctx);nk_input_begin(ctx);nk_input_key(ctx,NK_KEY_ENTER,1);nk_input_end(ctx);
    assert(wena_card_selection_panel_render(ctx,panel,snapshot->cards,snapshot->card_count,"board",640,480));
    assert(!memcmp(adapter.badges,before,sizeof(*before))&&panel->labels);
    nk_clear(ctx);nk_input_begin(ctx);nk_input_key(ctx,NK_KEY_ENTER,0);nk_input_end(ctx);
    assert(wena_card_selection_panel_render(ctx,panel,snapshot->cards,snapshot->card_count,"board",640,480));
    selection_click(ctx,panel,snapshot,"Cancel");assert(!panel->labels&&panel->selection->count==4);
    selection_click(ctx,panel,snapshot,"Labels");selection_click(ctx,panel,snapshot,"Next Page");selection_click(ctx,panel,snapshot,"Label 5 [label5]");
    execute(db,"CREATE TRIGGER fail_label_selection BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    selection_click(ctx,panel,snapshot,"Add label");assert(panel->archive_error&&panel->labels&&!memcmp(adapter.badges,before,sizeof(*before)));
    execute(db,"DROP TRIGGER fail_label_selection");
    selection_click(ctx,panel,snapshot,"Add label");assert(!panel->labels&&!panel->archive_error&&panel->visible&&panel->selection->count==4);
    assert(adapter.badges->catalogue.assigned_card_counts[5]==4);
    assert(wena_label_mutation_load_board(&adapter.mutation,"board",fresh)&&!memcmp(fresh,adapter.badges,sizeof(*fresh)));
    selection_click(ctx,panel,snapshot,"Labels");selection_click(ctx,panel,snapshot,"Next Page");selection_click(ctx,panel,snapshot,"Label 5 [label5]");
    execute(db,"UPDATE cards SET version=version+1 WHERE id='outside'");
    selection_click(ctx,panel,snapshot,"Remove Label");assert(panel->archive_error&&panel->labels&&adapter.badges->catalogue.assigned_card_counts[5]==4);
    selection_click(ctx,panel,snapshot,"Cancel");selection_click(ctx,panel,snapshot,"Labels");selection_click(ctx,panel,snapshot,"Next Page");selection_click(ctx,panel,snapshot,"Label 5 [label5]");
    selection_click(ctx,panel,snapshot,"Remove Label");assert(!panel->labels&&panel->selection->count==4&&adapter.badges->catalogue.assigned_card_counts[5]==0);
    selection_click(ctx,panel,snapshot,"Labels");
    nk_clear(ctx);nk_input_begin(ctx);nk_input_key(ctx,NK_KEY_TEXT_RESET_MODE,1);nk_input_end(ctx);
    assert(wena_card_selection_panel_render(ctx,panel,snapshot->cards,snapshot->card_count,"board",640,480));
    assert(!panel->labels&&panel->visible&&panel->selection->count==4);
    nk_clear(ctx);nk_input_begin(ctx);nk_input_key(ctx,NK_KEY_TEXT_RESET_MODE,0);nk_input_end(ctx);
    wena_card_selection_panel_set_labels(panel,NULL,NULL,NULL);
    free(adapter.badges);free(before);free(fresh);
}

int main(int argc, char **argv)
{
    unsigned char migration[65536];
    size_t length;
    char hash[65];
    char path[4096];
    FILE *file;
    sqlite3 *database;
    WenaSqliteBoardSnapshot *snapshot;
    WenaHierarchyMutation adapter;
    WenaHierarchyTitleState state;
    WenaBoardLayout layout;
    WenaHierarchyKind kinds[3];
    WenaCardArchivesState archives;
    const char *ids[3];
    const char *bad_titles[6];
    char title[129];
    char before[129];
    char long_title[130];
    unsigned long version;
    size_t index;
    size_t bad;
    struct nk_context context;
    struct nk_user_font font;
    int initial_length;
    int count;

    assert(argc == 3 && strlen(argv[2]) < 4000);
    file = fopen(argv[1], "rb"); assert(file);
    length = fread(migration, 1, sizeof(migration), file);
    assert(length > 0 && length < sizeof(migration));
    assert(fclose(file) == 0);
    wena_sha256_hex(migration, length, hash);
    sprintf(path, "%s/hierarchy.sqlite", argv[2]);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    execute(database, "INSERT INTO actors VALUES('actor','User',1);"
        "INSERT INTO boards VALUES('board','Board',1);"
        "INSERT INTO boards VALUES('other','Other',1);"
        "INSERT INTO swimlanes VALUES('lane','board','Lane',0,1);"
        "INSERT INTO swimlanes VALUES('foreign-lane','other','Foreign lane',0,1);"
        "INSERT INTO lists VALUES('list','board','List',0,1);"
        "INSERT INTO lists VALUES('foreign-list','other','Foreign list',0,1);");
    snapshot = (WenaSqliteBoardSnapshot *)malloc(sizeof(*snapshot));
    assert(snapshot && wena_sqlite_board_load(database, "board", snapshot));
    assert(!wena_hierarchy_mutation_init(&adapter, database, "actor", "other", snapshot));
    assert(!wena_hierarchy_mutation_init(&adapter, database, "bad/actor", "board", snapshot));
    assert(wena_hierarchy_mutation_init(&adapter, database, "actor", "board", snapshot));
    kinds[0] = WENA_HIERARCHY_BOARD; kinds[1] = WENA_HIERARCHY_LIST;
    kinds[2] = WENA_HIERARCHY_SWIMLANE;
    ids[0] = "board"; ids[1] = "list"; ids[2] = "lane";
    bad_titles[0] = ""; bad_titles[1] = "Bad\n"; bad_titles[2] = "\177";
    bad_titles[3] = "\300\257"; bad_titles[4] = "\302\205";
    bad_titles[5] = "\355\240\200";
    memset(long_title, 'x', sizeof(long_title)); long_title[129] = '\0';
    for (index = 0; index < 3; ++index) {
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index],
            title, sizeof(title), &version) && version == 1);
        for (bad = 0; bad < 6; ++bad)
            assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 1, bad_titles[bad]));
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 1, long_title));
        assert(!wena_hierarchy_mutation_save(&adapter, "other", kinds[index], ids[index], 1, "Wrong"));
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], "missing", 1, "Wrong"));
        assert(!wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 0, 1, "Wrong"));
        assert(!wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 1, ULONG_MAX, "Wrong"));
        assert(!wena_hierarchy_mutation_load(&adapter, "other", kinds[index], ids[index], title, sizeof(title), &version));
        assert(!wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, 2, &version));
        assert(wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 1, 40, "A&B + 100% = \303\244"));
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, sizeof(title), &version));
        assert(version == 2 && strcmp(title, "A&B + 100% = \303\244") == 0);
        assert(!wena_hierarchy_mutation_save_request(&adapter, "board", kinds[index], ids[index], 2, 40, "Replay"));
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 1, "Stale"));
        execute(database, "CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'injected'); END;");
        assert(!wena_hierarchy_mutation_save(&adapter, "board", kinds[index], ids[index], 2, "Rollback"));
        execute(database, "DROP TRIGGER reject_metadata;");
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, sizeof(title), &version));
        assert(version == 2 && strcmp(title, "A&B + 100% = \303\244") == 0);
    }
    assert(strcmp(snapshot->board.title, title) == 0);
    assert(strcmp(snapshot->lists[0].title, title) == 0);
    assert(strcmp(snapshot->swimlanes[0].title, title) == 0);
    assert(!wena_hierarchy_mutation_save(&adapter, "board", WENA_HIERARCHY_LIST, "foreign-list", 1, "Wrong"));
    assert(!wena_hierarchy_mutation_save(&adapter, "board", WENA_HIERARCHY_SWIMLANE, "foreign-lane", 1, "Wrong"));
    assert(!wena_hierarchy_mutation_save(&adapter, "board", (WenaHierarchyKind)99, "list", 2, "Wrong"));
    assert(wena_hierarchy_mutation_init(&adapter, database, "unknown", "board", snapshot));
    assert(!wena_hierarchy_mutation_load(&adapter, "board", WENA_HIERARCHY_LIST, "list", title, sizeof(title), &version));
    assert(!wena_hierarchy_mutation_save(&adapter, "board", WENA_HIERARCHY_LIST, "list", 2, "Forbidden"));
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    assert(wena_sqlite_integrity(database));
    assert(wena_sqlite_board_load(database, "board", snapshot));
    assert(wena_hierarchy_mutation_init(&adapter, database, "actor", "board", snapshot));
    for (index = 0; index < 3; ++index) {
        assert(wena_hierarchy_mutation_load(&adapter, "board", kinds[index], ids[index], title, sizeof(title), &version));
        assert(version == 2 && strcmp(title, "A&B + 100% = \303\244") == 0);
    }

    memset(&layout, 0, sizeof(layout));
    layout.board = &snapshot->board;
    layout.lists = snapshot->lists; layout.list_count = snapshot->list_count;
    layout.swimlanes = snapshot->swimlanes; layout.swimlane_count = snapshot->swimlane_count;
    memset(&font, 0, sizeof(font)); font.height = 13.0f; font.width = text_width;
    assert(nk_init_default(&context, &font));
    wena_hierarchy_title_init(&state);
    assert(!wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    wena_hierarchy_title_set_adapter(&state, wena_hierarchy_mutation_load,
        wena_hierarchy_mutation_save, &adapter);
    assert(!wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_BOARD, "other"));
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    strcpy(before, snapshot->lists[0].title);
    frame(&context, &state, &layout);
    click(&context, &state, &layout, "Cancel");
    assert(!state.visible && strcmp(before, snapshot->lists[0].title) == 0);
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    frame(&context, &state, &layout);
    click_at(&context, &state, &layout, nk_vec2(200.0f, 145.0f));
    initial_length = state.title_length;
    nk_clear(&context); nk_input_begin(&context);
    nk_input_unicode(&context, (nk_rune)'X'); nk_input_end(&context);
    render(&context, &state, &layout);
    assert(state.title_length == initial_length + 1);
    click(&context, &state, &layout, "Save");
    assert(!state.visible && strchr(snapshot->lists[0].title, 'X') != NULL);
    assert(wena_hierarchy_mutation_load(&adapter, "board", WENA_HIERARCHY_LIST, "list", title, sizeof(title), &version) && version == 3);
    strcpy(before, title);
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    frame(&context, &state, &layout);
    click_at(&context, &state, &layout, nk_vec2(200.0f, 145.0f));
    for (count = 0; count < 160; ++count) {
        nk_clear(&context); nk_input_begin(&context);
        nk_input_unicode(&context, (nk_rune)'a'); nk_input_end(&context);
        render(&context, &state, &layout);
    }
    assert(state.title_length ==
        WENA_NATIVE_EDIT_CAPACITY(WENA_CARD_DETAILS_TITLE_CAPACITY) - 1);
    click(&context, &state, &layout, "Save");
    assert(state.visible && state.error && strcmp(before, snapshot->lists[0].title) == 0);
    click(&context, &state, &layout, "Cancel");
    assert(!state.visible);
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    state.title_input[0] = '\0'; state.title_length = 0;
    frame(&context, &state, &layout);
    click(&context, &state, &layout, "Save");
    assert(state.visible && state.error);
    click(&context, &state, &layout, "Cancel");
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    execute(database, "UPDATE lists SET version=version+1 WHERE id='list'");
    frame(&context, &state, &layout);
    click(&context, &state, &layout, "Save");
    assert(state.visible && state.error && strcmp(before, snapshot->lists[0].title) == 0);
    click(&context, &state, &layout, "Cancel");
    assert(wena_hierarchy_title_open(&state, &layout, WENA_HIERARCHY_LIST, "list"));
    snapshot->lists[0].archived = 1;
    assert(!wena_hierarchy_title_render(&context, &state, &layout, 640.0f, 480.0f));
    assert(!state.visible);
    snapshot->lists[0].archived = 0;
    creation_tests(database, &adapter, snapshot, &context, &state, &layout);
    color_tests(database,&adapter,snapshot,&context,&state,&layout);
    wip_tests(database,&adapter,snapshot,&context,&state,&layout);
    wena_hierarchy_title_set_archive_adapter(&state,wena_hierarchy_mutation_archive);
    assert(wena_hierarchy_title_open(&state,&layout,WENA_HIERARCHY_LIST,"list"));
    frame(&context,&state,&layout);
    execute(database,"UPDATE lists SET version=version+1 WHERE id='list'");
    click(&context,&state,&layout,"Move List to Archive");
    assert(state.visible&&state.error&&!snapshot->lists[0].archived);
    click(&context,&state,&layout,"Cancel");
    assert(wena_hierarchy_title_open(&state,&layout,WENA_HIERARCHY_LIST,"list"));
    frame(&context,&state,&layout);
    execute(database,"CREATE TRIGGER fail_archive BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    click(&context,&state,&layout,"Move List to Archive");
    assert(state.visible&&state.error&&!snapshot->lists[0].archived);
    execute(database,"DROP TRIGGER fail_archive");
    click(&context,&state,&layout,"Move List to Archive");
    assert(!state.visible&&snapshot->lists[0].archived&&!strcmp(before,snapshot->lists[0].title));
    assert(!wena_hierarchy_title_open(&state,&layout,WENA_HIERARCHY_LIST,"list"));
    wena_card_archives_init(&archives,NULL,NULL,NULL);
    wena_card_archives_set_lists(&archives,wena_hierarchy_mutation_archive_load,wena_hierarchy_mutation_restore,&adapter);
    assert(wena_card_archives_open(&archives,&layout));
    nk_clear(&context);nk_input_begin(&context);nk_input_end(&context);archives_frame(&context,&archives,&layout);
    archives_click(&context,&archives,&layout,"Lists");
    assert(archives.kind&&!strcmp(archives.card_id,"list")&&archives.version);
    execute(database,"CREATE TRIGGER fail_restore BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    archives_click(&context,&archives,&layout,"Restore");assert(archives.error&&snapshot->lists[0].archived);
    execute(database,"DROP TRIGGER fail_restore");
    archives_click(&context,&archives,&layout,"Restore");assert(!snapshot->lists[0].archived&&!archives.card_id[0]);
    wena_card_archives_close(&archives);
    wena_hierarchy_title_set_archive_provider(&state,WENA_HIERARCHY_SWIMLANE,wena_hierarchy_mutation_swimlane_archive);
    assert(wena_hierarchy_title_open(&state,&layout,WENA_HIERARCHY_SWIMLANE,"lane"));
    frame(&context,&state,&layout);
    execute(database,"UPDATE swimlanes SET version=version+1 WHERE id='lane'");
    click(&context,&state,&layout,"Move Swimlane to Archive");
    assert(state.visible&&state.error&&!snapshot->swimlanes[0].archived);
    click(&context,&state,&layout,"Cancel");
    assert(wena_hierarchy_title_open(&state,&layout,WENA_HIERARCHY_SWIMLANE,"lane"));
    frame(&context,&state,&layout);
    execute(database,"CREATE TRIGGER fail_lane BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    click(&context,&state,&layout,"Move Swimlane to Archive");
    assert(state.visible&&state.error&&!snapshot->swimlanes[0].archived);
    for(index=0;index<snapshot->card_count;++index)assert(!snapshot->cards[index].archived);
    execute(database,"DROP TRIGGER fail_lane");
    click(&context,&state,&layout,"Move Swimlane to Archive");
    assert(!state.visible&&snapshot->swimlanes[0].archived&&snapshot->card_count==3);
    for(index=0;index<snapshot->card_count;++index)assert(snapshot->cards[index].archived);
    assert(!wena_hierarchy_title_open(&state,&layout,WENA_HIERARCHY_SWIMLANE,"lane"));
    wena_card_archives_set_provider(&archives,WENA_ARCHIVE_SWIMLANES,
        wena_hierarchy_mutation_swimlane_archive_load,wena_hierarchy_mutation_swimlane_restore,&adapter);
    assert(wena_card_archives_open(&archives,&layout));
    nk_clear(&context);nk_input_begin(&context);nk_input_end(&context);archives_frame(&context,&archives,&layout);
    archives_click(&context,&archives,&layout,"Swimlanes");
    assert(archives.kind==WENA_ARCHIVE_SWIMLANES&&!strcmp(archives.card_id,"lane")&&archives.version);
    execute(database,"UPDATE list_wip_limits SET value=1 WHERE list_id='list'");
    archives_click(&context,&archives,&layout,"Restore");
    assert(archives.error&&snapshot->swimlanes[0].archived);
    for(index=0;index<snapshot->card_count;++index)assert(snapshot->cards[index].archived);
    execute(database,"UPDATE list_wip_limits SET value=5 WHERE list_id='list'");
    execute(database,"CREATE TRIGGER fail_lane_restore BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    archives_click(&context,&archives,&layout,"Restore");
    assert(archives.error&&snapshot->swimlanes[0].archived);
    execute(database,"DROP TRIGGER fail_lane_restore");
    archives_click(&context,&archives,&layout,"Restore");
    assert(!snapshot->swimlanes[0].archived&&!archives.card_id[0]);
    for(index=0;index<snapshot->card_count;++index)assert(!snapshot->cards[index].archived);
    wena_card_archives_close(&archives);
    execute(database,"INSERT INTO cards SELECT 'outside','board',id,'list','Outside',0,0,1 FROM swimlanes WHERE board_id='board' AND id<>'lane' LIMIT 1");
    assert(wena_sqlite_board_load(database,"board",snapshot));layout.card_count=snapshot->card_count;
    wena_hierarchy_title_set_list_cards_adapters(&state,wena_hierarchy_mutation_list_cards_load,wena_hierarchy_mutation_list_cards_archive);
    assert(!wena_hierarchy_title_open_list(&state,&layout,"list","foreign-lane")&&!state.visible);
    assert(wena_hierarchy_title_open_list(&state,&layout,"list","lane"));frame(&context,&state,&layout);
    strcpy(adapter.actor_id,"missing");
    click(&context,&state,&layout,"Archive all cards in this list");
    assert(state.error&&!state.confirming_cards&&state.visible);
    strcpy(adapter.actor_id,"actor");click(&context,&state,&layout,"Cancel");

    assert(wena_hierarchy_title_open_list(&state,&layout,"list","lane"));frame(&context,&state,&layout);
    click(&context,&state,&layout,"Archive all cards in this list");frame(&context,&state,&layout);
    assert(state.confirming_cards&&state.visible&&!strcmp(state.scope_lane,"lane"));
    count=scalar(database,"SELECT count(*) FROM idempotency_keys");
    editor_key(&context,&state,&layout,NK_KEY_ENTER,1);editor_key(&context,&state,&layout,NK_KEY_ENTER,0);
    assert(state.visible&&scalar(database,"SELECT count(*) FROM idempotency_keys")==count);
    click(&context,&state,&layout,"Cancel");assert(!state.visible);
    assert(wena_hierarchy_title_open_list(&state,&layout,"list","lane"));frame(&context,&state,&layout);
    click(&context,&state,&layout,"Archive all cards in this list");frame(&context,&state,&layout);
    editor_key(&context,&state,&layout,NK_KEY_TEXT_RESET_MODE,1);assert(!state.visible);
    editor_key(&context,&state,&layout,NK_KEY_TEXT_RESET_MODE,0);
    assert(wena_hierarchy_title_open_list(&state,&layout,"list","lane"));frame(&context,&state,&layout);
    click(&context,&state,&layout,"Archive all cards in this list");frame(&context,&state,&layout);
    execute(database,"UPDATE swimlanes SET version=version+1 WHERE id='lane'");
    click(&context,&state,&layout,"Move to Archive");
    assert(state.visible&&state.error&&scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==0);
    click(&context,&state,&layout,"Cancel");
    assert(wena_hierarchy_title_open_list(&state,&layout,"list","lane"));frame(&context,&state,&layout);
    click(&context,&state,&layout,"Archive all cards in this list");frame(&context,&state,&layout);
    execute(database,"CREATE TRIGGER fail_batch BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
    click(&context,&state,&layout,"Move to Archive");
    assert(state.visible&&state.error&&scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==0);
    for(index=0;index<snapshot->card_count;++index)assert(!snapshot->cards[index].archived);
    execute(database,"DROP TRIGGER fail_batch");
    click(&context,&state,&layout,"Move to Archive");assert(!state.visible);
    assert(scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==3);
    assert(scalar(database,"SELECT archived FROM cards WHERE id='outside'")==0);
    assert(!snapshot->lists[0].archived&&!snapshot->swimlanes[0].archived);
    assert(wena_hierarchy_title_open_list(&state,&layout,"list",NULL));frame(&context,&state,&layout);
    click(&context,&state,&layout,"Archive all cards in this list");frame(&context,&state,&layout);
    assert(!state.scope_lane[0]);click(&context,&state,&layout,"Move to Archive");assert(!state.visible);
    assert(scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==4);
    {
        WenaCardSelection *selection;WenaCardSelectionPanel panel;
        selection=(WenaCardSelection*)malloc(sizeof(*selection));assert(selection);
        assert(wena_card_selection_init(selection,"board"));wena_card_selection_panel_init(&panel,selection);
        execute(database,"UPDATE cards SET archived=0");
        assert(wena_sqlite_board_load(database,"board",snapshot));layout.card_count=snapshot->card_count;
        state.selection_enabled=1;
        assert(wena_hierarchy_title_open_list(&state,&layout,"list","lane"));frame(&context,&state,&layout);
        assert(click_action(&context,&state,&layout,"Select all cards in this list")==WENA_HIERARCHY_TITLE_SELECT_CARDS);
        assert(!strcmp(state.scope_lane,"lane"));
        assert(wena_card_selection_panel_open(&panel,snapshot->cards,snapshot->card_count,
            state.board_id,state.target_id,state.scope_lane));
        wena_hierarchy_title_close(&state);
        assert(selection->count==3&&!wena_card_selection_contains(selection,"outside"));
        nk_clear(&context);nk_input_begin(&context);nk_input_end(&context);
        assert(wena_card_selection_panel_render(&context,&panel,snapshot->cards,snapshot->card_count,"board",640,480));
        /* Opening another menu hides the panel but preserves additive selection. */
        panel.visible=0;
        assert(wena_hierarchy_title_open_list(&state,&layout,"list",NULL));frame(&context,&state,&layout);
        assert(click_action(&context,&state,&layout,"Select all cards in this list")==WENA_HIERARCHY_TITLE_SELECT_CARDS);
        assert(!state.scope_lane[0]);
        assert(wena_card_selection_panel_open(&panel,snapshot->cards,snapshot->card_count,
            state.board_id,state.target_id,state.scope_lane));
        assert(selection->count==4&&wena_card_selection_contains(selection,"outside"));
        frame(&context,&state,&layout);assert(!state.requested_action);
        wena_hierarchy_title_close(&state);
        selection_labels(&context,&panel,snapshot,database);
        wena_card_selection_panel_set_archive(&panel,wena_hierarchy_mutation_selected_cards_load,
            wena_hierarchy_mutation_selected_cards_archive,&adapter);
        assert(wena_card_selection_panel_open(&panel,snapshot->cards,snapshot->card_count,"board","list",NULL));
        selection_frame(&context,&panel,snapshot);
        strcpy(adapter.actor_id,"missing");selection_click(&context,&panel,snapshot,"Move selection to Archive");
        assert(panel.archive_error&&!panel.captured&&selection->count==4);strcpy(adapter.actor_id,"actor");
        selection_click(&context,&panel,snapshot,"Move selection to Archive");
        assert(panel.captured&&panel.captured_count==4&&selection->count==4);
        nk_clear(&context);nk_input_begin(&context);nk_input_key(&context,NK_KEY_ENTER,1);nk_input_end(&context);
        assert(wena_card_selection_panel_render(&context,&panel,snapshot->cards,snapshot->card_count,"board",640,480));
        nk_clear(&context);nk_input_begin(&context);nk_input_key(&context,NK_KEY_ENTER,0);nk_input_end(&context);
        assert(wena_card_selection_panel_render(&context,&panel,snapshot->cards,snapshot->card_count,"board",640,480));
        assert(panel.captured&&selection->count==4&&scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==0);

        selection_click(&context,&panel,snapshot,"Cancel");
        assert(!panel.captured&&selection->count==4&&panel.visible);
        selection_click(&context,&panel,snapshot,"Move selection to Archive");
        assert(wena_card_selection_toggle(selection,snapshot->cards,snapshot->card_count,"outside"));
        selection_frame(&context,&panel,snapshot);assert(panel.error&&panel.captured_count==4&&selection->count==3);
        selection_click(&context,&panel,snapshot,"Move to Archive");
        assert(scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==0&&panel.captured);
        selection_click(&context,&panel,snapshot,"Cancel");
        assert(wena_card_selection_panel_open(&panel,snapshot->cards,snapshot->card_count,"board","list",NULL));
        selection_frame(&context,&panel,snapshot);

        selection_click(&context,&panel,snapshot,"Move selection to Archive");
        nk_clear(&context);nk_input_begin(&context);nk_input_key(&context,NK_KEY_TEXT_RESET_MODE,1);nk_input_end(&context);
        assert(wena_card_selection_panel_render(&context,&panel,snapshot->cards,snapshot->card_count,"board",640,480));
        assert(!panel.captured&&selection->count==4&&panel.visible);
        nk_clear(&context);nk_input_begin(&context);nk_input_key(&context,NK_KEY_TEXT_RESET_MODE,0);nk_input_end(&context);
        assert(wena_card_selection_panel_render(&context,&panel,snapshot->cards,snapshot->card_count,"board",640,480));

        selection_click(&context,&panel,snapshot,"Move selection to Archive");
        assert(panel.captured_count==4);
        execute(database,"UPDATE cards SET version=version+1 WHERE id='outside'");
        selection_click(&context,&panel,snapshot,"Move to Archive");
        assert(panel.archive_error&&panel.captured_count==4&&selection->count==4);
        assert(scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==0);
        selection_click(&context,&panel,snapshot,"Cancel");
        selection_click(&context,&panel,snapshot,"Move selection to Archive");
        execute(database,"CREATE TRIGGER fail_selection BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late');END");
        selection_click(&context,&panel,snapshot,"Move to Archive");
        assert(panel.archive_error&&panel.captured&&selection->count==4);
        for(index=0;index<snapshot->card_count;++index)assert(!snapshot->cards[index].archived);
        execute(database,"DROP TRIGGER fail_selection");
        selection_click(&context,&panel,snapshot,"Move to Archive");
        assert(!panel.visible&&!panel.captured&&!selection->count);
        assert(scalar(database,"SELECT count(*) FROM cards WHERE archived=1")==4);
        for(index=0;index<snapshot->card_count;++index)assert(snapshot->cards[index].archived);
        free(selection);
    }
    nk_free(&context);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(wena_sqlite_open(path, migration, length, hash, &database));
    assert(wena_sqlite_board_load(database, "board", snapshot));
    assert(strcmp(before, snapshot->lists[0].title) == 0);
    assert(snapshot->list_count == 4 && snapshot->swimlane_count == 3);
    assert(snapshot->lists[0].wip_limit.value==5&&snapshot->lists[0].wip_limit.enabled&&!snapshot->lists[0].wip_limit.soft);
    assert(!strcmp(snapshot->lists[0].color,"#123AbC")&&!strcmp(snapshot->swimlanes[0].color,"#123AbC"));
    assert(sqlite3_close(database) == SQLITE_OK);
    free(snapshot);
    puts("hierarchy SQLite title and real Nuklear editor tests passed");
    return 0;
}
