#include "../client/features/labels/panel.h"
#include "../client/features/labels/mutation.h"
#include "../server/mutations/labels.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3 *database, const char *query)
{
    char *error;
    int result;
    error = NULL;
    result = sqlite3_exec(database, query, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "%s: %s\n", query, error);
    sqlite3_free(error);
    assert(result == SQLITE_OK);
}
static sqlite3_int64 number(sqlite3 *database, const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(database, query, -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    result = sqlite3_column_int64(statement, 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    return result;
}
static void schema(sqlite3 *database, const char *path)
{
    FILE *file;
    long length;
    char *data;
    file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    length = ftell(file);
    assert(length > 0);
    rewind(file);
    data = (char *)malloc((size_t)length + 1);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    data[length] = 0;
    assert(!fclose(file));
    sql(database, data);
    free(data);
}
static void frame(WenaLabelsState *state, WenaCard *card, const char *button,
    const char *name)
{
    struct nk_context context;
    memset(&context, 0, sizeof(context));
    context.button_to_press = button;
    context.edit_text = name;
    assert(wena_labels_render(&context, state, "b", card, card ? 1 : 0, 800, 600));
}
static void reopen(WenaLabelsState *state, WenaCard *card)
{
    wena_labels_close(state);
    assert(wena_labels_open(state, "b", card));
}
static void batch_command(WenaDomainCommand *command,WenaDomainCardRevision *rows,size_t count,
    int assign,unsigned long board_version,unsigned long request)
{
    memset(command,0,sizeof(*command));command->operation=assign?WENA_DOMAIN_ASSIGN_SELECTED_LABEL:WENA_DOMAIN_UNASSIGN_SELECTED_LABEL;
    command->request_version=request;strcpy(command->user_id,"u");strcpy(command->route,"/b/batch/native");
    sprintf(command->form_body,"labelId=label&expectedLabelVersion=1&expectedBoardVersion=%lu",board_version);
    command->form_body_length=strlen(command->form_body);command->selected_cards=rows;command->selected_card_count=count;
}
static char *batch_state(sqlite3 *db)
{
    sqlite3_stmt *statement;char *copy;
    assert(sqlite3_prepare_v2(db,"SELECT group_concat(row,'|') FROM ("
        "SELECT 'c:'||id||':'||version||':'||archived||':'||list_id||':'||swimlane_id AS row FROM cards WHERE board_id='batch' "
        "UNION ALL SELECT 'a:'||card_id||':'||label_id FROM card_labels WHERE board_id='batch' "
        "UNION ALL SELECT 'l:'||id||':'||version FROM labels WHERE board_id='batch' ORDER BY row)",-1,&statement,NULL)==SQLITE_OK);
    assert(sqlite3_step(statement)==SQLITE_ROW);
    copy=sqlite3_mprintf("%s",sqlite3_column_text(statement,0));assert(copy);
    assert(sqlite3_finalize(statement)==SQLITE_OK);return copy;
}
static void batch_rejected(sqlite3 *db,WenaSqlitePersistence *store,WenaDomainCommand *command)
{
    WenaRegionResponse response;sqlite3_int64 version,keys,assignments;char *before,*after;
    version=number(db,"SELECT version FROM boards WHERE id='batch'");
    before=batch_state(db);
    keys=number(db,"SELECT count(*) FROM idempotency_keys");
    assignments=number(db,"SELECT count(*) FROM card_labels WHERE board_id='batch'");
    assert(!wena_sqlite_persistence_apply(store,command,&response));
    assert(version==number(db,"SELECT version FROM boards WHERE id='batch'"));
    after=batch_state(db);assert(!strcmp(before,after));sqlite3_free(before);sqlite3_free(after);
    assert(keys==number(db,"SELECT count(*) FROM idempotency_keys"));
    assert(assignments==number(db,"SELECT count(*) FROM card_labels WHERE board_id='batch'"));
    assert(sqlite3_get_autocommit(db));
}
static int reject_batch_commit(void *data){(void)data;return 1;}
static void selected_labels(sqlite3 *db)
{
    WenaSqlitePersistence store;WenaDomainCommand command;WenaRegionResponse response;
    WenaDomainCardRevision *rows;sqlite3_int64 keys;size_t i;char query[256];
    sql(db,"INSERT INTO boards VALUES('batch','Batch',1);"
        "INSERT INTO lists VALUES('bl','batch','List',0,1);INSERT INTO swimlanes VALUES('bs','batch','Lane',0,1);"
        "INSERT INTO cards VALUES('ba','batch','bs','bl','Same title',0,0,1),('bz','batch','bs','bl','Same title',1,0,1),('bu','batch','bs','bl','Unselected',2,0,1);"
        "INSERT INTO labels VALUES('batch','label','Batch label','blue',0,1,0,0)");
    rows=(WenaDomainCardRevision*)calloc(WENA_DOMAIN_CARD_BATCH_CAPACITY,sizeof(*rows));assert(rows);
    strcpy(rows[0].id,"ba");rows[0].version=1;strcpy(rows[1].id,"bz");rows[1].version=1;
    wena_sqlite_persistence_init(&store,db);batch_command(&command,rows,2,1,1,4000);
    command.selected_card_count=0;batch_rejected(db,&store,&command);
    command.selected_card_count=WENA_DOMAIN_CARD_BATCH_CAPACITY+1;batch_rejected(db,&store,&command);
    command.selected_card_count=2;command.selected_cards=NULL;batch_rejected(db,&store,&command);command.selected_cards=rows;
    strcpy(rows[1].id,"ba");batch_rejected(db,&store,&command);
    strcpy(rows[1].id,"foreign");batch_rejected(db,&store,&command);
    strcpy(rows[1].id,"missing");batch_rejected(db,&store,&command);
    strcpy(rows[1].id,"bad id");batch_rejected(db,&store,&command);
    strcpy(rows[1].id,"bz");rows[1].version=2;batch_rejected(db,&store,&command);rows[1].version=1;
    sql(db,"UPDATE cards SET archived=1 WHERE id='bz'");batch_rejected(db,&store,&command);
    sql(db,"UPDATE cards SET archived=0 WHERE id='bz'");
    sql(db,"INSERT INTO list_archive_state VALUES('bl','batch',1,1)");batch_rejected(db,&store,&command);
    sql(db,"UPDATE list_archive_state SET archived=0 WHERE list_id='bl'");
    sql(db,"INSERT INTO swimlane_archive_state VALUES('bs','batch',1,1)");batch_rejected(db,&store,&command);
    sql(db,"UPDATE swimlane_archive_state SET archived=0 WHERE swimlane_id='bs'");
    sql(db,"PRAGMA query_only=ON");batch_rejected(db,&store,&command);sql(db,"PRAGMA query_only=OFF");
    strcpy(command.user_id,"missing");batch_rejected(db,&store,&command);strcpy(command.user_id,"u");
    batch_command(&command,rows,2,1,2,4000);batch_rejected(db,&store,&command);
    batch_command(&command,rows,2,1,1,4000);
    sql(db,"UPDATE labels SET version=2 WHERE board_id='batch'");batch_rejected(db,&store,&command);
    sql(db,"UPDATE labels SET version=1 WHERE board_id='batch'");
    sql(db,"CREATE TRIGGER batch_late BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'late'); END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER batch_late");
    sql(db,"CREATE TRIGGER batch_second BEFORE INSERT ON card_labels WHEN NEW.card_id='bz' BEGIN SELECT RAISE(ABORT,'second'); END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER batch_second");
    sql(db,"CREATE TRIGGER batch_earlier AFTER INSERT ON card_labels WHEN NEW.card_id='bz' BEGIN DELETE FROM card_labels WHERE card_id='ba'; END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER batch_earlier");
    sql(db,"CREATE TRIGGER batch_parent AFTER INSERT ON card_labels WHEN NEW.card_id='bz' BEGIN UPDATE list_archive_state SET archived=1 WHERE list_id='bl'; END");
    batch_rejected(db,&store,&command);sql(db,"DROP TRIGGER batch_parent");
    assert(number(db,"SELECT archived FROM list_archive_state WHERE list_id='bl'")==0);
    sqlite3_commit_hook(db,reject_batch_commit,NULL);batch_rejected(db,&store,&command);sqlite3_commit_hook(db,NULL,NULL);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));batch_rejected(db,&store,&command);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='batch'")==2);
    assert(number(db,"SELECT version FROM cards WHERE id='bu'")==1);
    assert(number(db,"SELECT version FROM boards WHERE id='batch'")==2);
    assert(rows[0].version==1&&rows[1].version==1);
    rows[0].version=rows[1].version=2;
    keys=number(db,"SELECT count(*) FROM idempotency_keys");
    batch_command(&command,rows,2,1,2,4001);assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(keys==number(db,"SELECT count(*) FROM idempotency_keys"));
    /* Mixed add and remove preserve no-op card revisions. */
    strcpy(rows[1].id,"bu");rows[1].version=1;
    batch_command(&command,rows,2,1,2,4002);assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT version FROM cards WHERE id='ba'")==2&&number(db,"SELECT version FROM cards WHERE id='bu'")==2);
    sql(db,"DELETE FROM card_labels WHERE card_id='bu'");rows[1].version=2;
    batch_command(&command,rows,2,0,3,4003);assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT version FROM cards WHERE id='ba'")==3&&number(db,"SELECT version FROM cards WHERE id='bu'")==2);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE card_id='bz'")==1);
    /* Full native capacity is a typed span, independent of HTTP body size. */
    sql(db,"BEGIN");
    for(i=0;i<WENA_DOMAIN_CARD_BATCH_CAPACITY;++i){
        sprintf(rows[i].id,"bulk%lu",(unsigned long)i);rows[i].version=1;
        sprintf(query,"INSERT INTO cards VALUES('%s','batch','bs','bl','Bulk',%lu,0,1)",rows[i].id,(unsigned long)i+10);sql(db,query);
    }
    sql(db,"COMMIT");batch_command(&command,rows,WENA_DOMAIN_CARD_BATCH_CAPACITY,1,4,4004);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='batch'")==2049);
    for(i=0;i<WENA_DOMAIN_CARD_BATCH_CAPACITY;++i)rows[i].version=2;
    batch_command(&command,rows,WENA_DOMAIN_CARD_BATCH_CAPACITY,0,5,4005);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='batch'")==1);
    strcpy(rows[0].id,"ba");rows[0].version=WENA_VERSION_MUTATE_MAX;
    sprintf(query,"UPDATE boards SET version=%lu WHERE id='batch'",WENA_VERSION_MUTATE_MAX);sql(db,query);
    sprintf(query,"UPDATE cards SET version=%lu WHERE id='ba'",WENA_VERSION_MUTATE_MAX);sql(db,query);
    batch_command(&command,rows,1,1,WENA_VERSION_MUTATE_MAX,4010);
    assert(wena_sqlite_persistence_apply(&store,&command,&response));
    assert(number(db,"SELECT version FROM cards WHERE id='ba'")== (sqlite3_int64)WENA_VERSION_READ_MAX);
    rows[0].version=WENA_VERSION_READ_MAX;
    batch_command(&command,rows,1,0,WENA_VERSION_READ_MAX,4011);batch_rejected(db,&store,&command);
    free(rows);
}

static void reboard_labels(char **paths)
{
    sqlite3 *db;size_t i;char query[512],path[1024];
    const char *triggers[]={
        "CREATE TRIGGER corrupt BEFORE DELETE ON card_labels WHEN OLD.label_id='s0' BEGIN SELECT RAISE(IGNORE);END",
        "CREATE TRIGGER corrupt BEFORE INSERT ON card_labels WHEN NEW.label_id='d1' BEGIN SELECT RAISE(ABORT,'second');END",
        "CREATE TRIGGER corrupt AFTER INSERT ON card_labels WHEN NEW.label_id='d1' BEGIN DELETE FROM card_labels WHERE card_id=NEW.card_id AND label_id='d0';END",
        "CREATE TRIGGER corrupt AFTER INSERT ON card_labels WHEN NEW.label_id='d1' BEGIN UPDATE labels SET name='Changed' WHERE board_id='b' AND id='d0';END",
        "CREATE TRIGGER corrupt AFTER INSERT ON card_labels WHEN NEW.label_id='d1' BEGIN UPDATE labels SET version=2 WHERE board_id='a' AND id='s0';END",
        "CREATE TRIGGER corrupt AFTER INSERT ON card_labels WHEN NEW.label_id='d1' BEGIN INSERT INTO card_labels VALUES('b',NEW.card_id,'d3');END"
    };
    assert(strlen(paths[4])+16<sizeof(path));sprintf(path,"%s.transfer",paths[4]);
    assert(sqlite3_open(path,&db)==SQLITE_OK);sql(db,"PRAGMA foreign_keys=ON");
    schema(db,paths[1]);schema(db,paths[2]);schema(db,paths[3]);
    sql(db,"INSERT INTO boards VALUES('a','Source',1),('b','Destination',1);"
        "INSERT INTO lists VALUES('al','a','List',0,1),('bl','b','List',0,1);"
        "INSERT INTO swimlanes VALUES('as','a','Lane',0,1),('bs','b','Lane',0,1);"
        "INSERT INTO cards VALUES('card','a','as','al','Card',0,0,1),('other','a','as','al','Other',1,0,1)");
    sql(db,"INSERT INTO labels VALUES('a','s0','Match','red',0,1,0,0),('a','s1','','blue',1,1,0,0),('a','s2','Missing','green',2,1,0,0)");
    sql(db,"INSERT INTO labels VALUES('b','d0','Match','blue',0,1,0,0),('b','d1','Match','green',1,1,0,0),"
        "('b','d2','','blue',2,1,0,0),('b','d3','match','red',3,1,0,0),('b','s0','Unrelated','pink',4,1,0,0)");
    sql(db,"INSERT INTO card_labels VALUES('a','card','s0'),('a','card','s1'),('a','card','s2'),('a','other','s0')");
    assert(!wena_sqlite_card_labels_reboard(db,"a","card","b"));
    sql(db,"BEGIN");assert(!wena_sqlite_card_labels_reboard(db,"a","card","a"));
    assert(!wena_sqlite_card_labels_reboard(db,"a","bad/id","b"));
    assert(!wena_sqlite_card_labels_reboard(db,"wrong","card","b"));sql(db,"ROLLBACK");
    for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){
        sql(db,triggers[i]);sql(db,"BEGIN");assert(!wena_sqlite_card_labels_reboard(db,"a","card","b"));sql(db,"ROLLBACK;DROP TRIGGER corrupt");
        assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='a'")==4&&number(db,"SELECT count(*) FROM card_labels WHERE board_id='b'")==0);
        assert(number(db,"SELECT count(*) FROM labels WHERE board_id='b' AND name='Match'")==2&&number(db,"SELECT version FROM labels WHERE board_id='a' AND id='s0'")==1);
        assert(number(db,"PRAGMA foreign_keys")==1&&number(db,"PRAGMA defer_foreign_keys")==0);
    }
    sql(db,"PRAGMA foreign_keys=OFF;BEGIN;UPDATE card_labels SET label_id='missing' WHERE card_id='card' AND label_id='s2'");
    assert(!wena_sqlite_card_labels_reboard(db,"a","card","b"));sql(db,"ROLLBACK;BEGIN;UPDATE card_labels SET board_id='b' WHERE card_id='card' AND label_id='s2'");
    assert(!wena_sqlite_card_labels_reboard(db,"a","card","b"));sql(db,"ROLLBACK;PRAGMA foreign_keys=ON");
    sql(db,"PRAGMA ignore_check_constraints=ON;BEGIN;UPDATE labels SET color='invalid' WHERE board_id='b' AND id='d0'");
    assert(!wena_sqlite_card_labels_reboard(db,"a","card","b"));sql(db,"ROLLBACK;PRAGMA ignore_check_constraints=OFF");
    sql(db,"BEGIN");assert(wena_sqlite_card_labels_reboard(db,"a","card","b"));
    assert(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_CONSTRAINT);sql(db,"ROLLBACK");
    sql(db,"BEGIN");assert(wena_sqlite_card_labels_reboard(db,"a","card","b"));
    sql(db,"UPDATE cards SET board_id='b',list_id='bl',swimlane_id='bs',version=version+1 WHERE id='card'");
    sqlite3_commit_hook(db,reject_batch_commit,NULL);assert(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_CONSTRAINT);
    sqlite3_commit_hook(db,NULL,NULL);assert(sqlite3_get_autocommit(db));
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='a'")==4);
    sql(db,"BEGIN");assert(wena_sqlite_card_labels_reboard(db,"a","card","b"));
    assert(number(db,"SELECT version FROM cards WHERE id='card'")==1&&number(db,"SELECT sum(version) FROM boards")==2);
    sql(db,"UPDATE cards SET board_id='b',list_id='bl',swimlane_id='bs',version=version+1 WHERE id='card';UPDATE boards SET version=version+1;COMMIT");
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b' AND card_id='card' AND label_id IN('d0','d1')")==2);
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='a' AND card_id='other'")==1);
    assert(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0);
    assert(sqlite3_close(db)==SQLITE_OK);assert(sqlite3_open(path,&db)==SQLITE_OK);sql(db,"PRAGMA foreign_keys=ON");
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='b' AND card_id='card'")==2);
    /* Exercise the complete bounded assignment set using distinct target IDs. */
    sql(db,"DELETE FROM card_labels;DELETE FROM labels;BEGIN");
    for(i=0;i<WENA_BOARD_LABEL_CAPACITY;++i){
        sprintf(query,"INSERT INTO labels VALUES('b','s%lu','Label %lu','red',%lu,1,0,0)",(unsigned long)i,(unsigned long)i,(unsigned long)i);sql(db,query);
        sprintf(query,"INSERT INTO labels VALUES('a','d%lu','Label %lu','blue',%lu,1,0,0)",(unsigned long)i,(unsigned long)i,(unsigned long)i);sql(db,query);
        sprintf(query,"INSERT INTO card_labels VALUES('b','card','s%lu')",(unsigned long)i);sql(db,query);
    }
    sql(db,"COMMIT;BEGIN;INSERT INTO labels VALUES('a','overflow','Extra','red',128,1,0,0)");
    assert(!wena_sqlite_card_labels_reboard(db,"b","card","a"));sql(db,"ROLLBACK;BEGIN");
    assert(wena_sqlite_card_labels_reboard(db,"b","card","a"));
    sql(db,"UPDATE cards SET board_id='a',list_id='al',swimlane_id='as',version=version+1 WHERE id='card';COMMIT");
    assert(number(db,"SELECT count(*) FROM card_labels WHERE board_id='a' AND card_id='card'")==WENA_BOARD_LABEL_CAPACITY);
    sql(db,"DELETE FROM labels WHERE board_id='b';BEGIN");assert(wena_sqlite_card_labels_reboard(db,"a","card","b"));
    sql(db,"UPDATE cards SET board_id='b',list_id='bl',swimlane_id='bs',version=version+1 WHERE id='card';COMMIT");
    assert(number(db,"SELECT count(*) FROM card_labels WHERE card_id='card'")==0);
    sql(db,"BEGIN");assert(wena_sqlite_card_labels_reboard(db,"b","card","a"));
    sql(db,"UPDATE cards SET board_id='a',list_id='al',swimlane_id='as',version=version+1 WHERE id='card';COMMIT");
    assert(number(db,"SELECT count(*) FROM pragma_foreign_key_check")==0&&number(db,"PRAGMA foreign_keys")==1);
    assert(sqlite3_close(db)==SQLITE_OK);
}

int main(int argc, char **argv)
{
    sqlite3 *database;
    WenaLabelMutation adapter;
    WenaLabelsState state;
    WenaLabelSnapshot *other;
    WenaLabelEdit edit;
    WenaCard card, wrong;
    WenaId label_id;
    char long_name[150], query[512];
    sqlite3_int64 keys, board_version, card_version, archived_version;
    unsigned long request;
    assert(argc == 7);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    sql(database, "PRAGMA foreign_keys=ON");
    schema(database, argv[1]);
    schema(database, argv[2]);
    schema(database, argv[3]);
    sql(database, "INSERT INTO actors VALUES('u','User',1);"
        "INSERT INTO boards VALUES('b','Board',1),('other','Other',1);"
        "INSERT INTO lists VALUES('l','b','List',0,1),('ol','other','Other',0,1);"
        "INSERT INTO swimlanes VALUES('s','b','Lane',0,1),('os','other','Other',0,1);"
        "INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1),"
        "('c2','b','s','l','Later archived',1,0,1),"
        "('foreign','other','os','ol','Foreign',0,0,1)");
    assert(wena_card_init(&card, "c", "b", "s", "l", "Card", 0, 0));
    assert(wena_label_mutation_init(&adapter, database, "u", "b"));
    wena_labels_init(&state, wena_label_mutation_load, wena_label_mutation_save, &adapter);
    assert(wena_labels_open(&state, "b", &card));
    frame(&state, &card, "Create Label", NULL);
    frame(&state, &card, "Cancel", "Cancelled");
    assert(number(database, "SELECT count(*) FROM labels") == 0);
    frame(&state, &card, "Create Label", NULL);
    frame(&state, &card, "Create", "");
    assert(!state.error && state.snapshot->label_count == 1 &&
        !state.snapshot->labels[0].name[0]);
    strcpy(label_id, state.snapshot->labels[0].id);
    frame(&state, &card, "Create Label", NULL);
    memset(long_name, 'x', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = 0;
    frame(&state, &card, "Create", long_name);
    assert(state.error && number(database, "SELECT count(*) FROM labels") == 1);
    frame(&state, &card, "Create", "Bad\001name");
    assert(state.error && number(database, "SELECT count(*) FROM labels") == 1);
    frame(&state, &card, "Create", "  Other label  ");
    assert(!state.error && state.snapshot->label_count == 2 &&
        !strcmp(state.snapshot->labels[1].name, "Other label"));
    frame(&state, &card, "Change Label", NULL);
    strcpy(state.color_input.custom_color, "#001122");
    state.color_input.color_length = 7;
    state.color_input.use_custom_color = 1;
    frame(&state, &card, "Save", "Alpha");
    assert(!state.error && !strcmp(state.snapshot->labels[0].name, "Alpha") &&
        !strcmp(state.snapshot->labels[0].color, "#001122"));
    frame(&state, &card, "Alpha", NULL);
    assert(!state.error && state.snapshot->assigned[0] &&
        state.snapshot->assigned_card_counts[0] == 1);
    frame(&state, &card, "Alpha", NULL);
    assert(!state.error && !state.snapshot->assigned[0]);
    frame(&state, &card, "Alpha", NULL);
    assert(state.snapshot->assigned[0]);
    frame(&state, &card, "Change Label", NULL);
    keys = number(database, "SELECT count(*) FROM idempotency_keys");
    board_version = number(database, "SELECT version FROM boards WHERE id='b'");
    sql(database, "CREATE TRIGGER rollback_label BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'rollback label'); END");
    frame(&state, &card, "Save", "After rollback");
    assert(state.error && state.action == WENA_LABEL_EDIT &&
        !strcmp(state.name, "After rollback") && !strcmp(state.snapshot->labels[0].name, "Alpha"));
    assert(number(database, "SELECT count(*) FROM labels WHERE name='After rollback'") == 0);
    assert(number(database, "SELECT version FROM boards WHERE id='b'") == board_version);
    assert(number(database, "SELECT count(*) FROM idempotency_keys") == keys);
    sql(database, "DROP TRIGGER rollback_label");
    frame(&state, &card, "Save", NULL);
    assert(!state.error && !strcmp(state.snapshot->labels[0].name, "After rollback"));
    /* Each captured revision remains authoritative until explicitly reopened. */
    frame(&state, &card, "Change Label", NULL);
    sql(database, "UPDATE boards SET version=version+1 WHERE id='b'");
    frame(&state, &card, "Save", "Stale board");
    assert(state.error && state.action && !strcmp(state.name, "Stale board"));
    reopen(&state, &card);
    frame(&state, &card, "Change Label", NULL);
    sprintf(query, "UPDATE labels SET version=version+1 WHERE board_id='b' AND id='%s'", label_id);
    sql(database, query);
    frame(&state, &card, "Save", "Stale label");
    assert(state.error && state.action);
    reopen(&state, &card);
    frame(&state, &card, "Change Label", NULL);
    sql(database, "UPDATE cards SET version=version+1 WHERE id='c'");
    frame(&state, &card, "Save", "Stale card");
    assert(state.error && state.action);
    reopen(&state, &card);
    /* A current payload cannot reuse an already committed request key. */
    memset(&edit, 0, sizeof(edit));
    edit.action = WENA_LABEL_CREATE;
    edit.expected_board_version = state.snapshot->board_version;
    edit.expected_card_version = state.snapshot->card_version;
    edit.name = "Replay";
    edit.color = "red";
    request = (unsigned long)number(database, "SELECT min(request_version) FROM idempotency_keys");
    assert(request && !wena_label_mutation_save_request(&adapter, "b", "c", &edit, request));
    assert(!wena_label_mutation_save(&adapter, "other", "c", &edit));
    assert(!wena_label_mutation_save(&adapter, "b", "foreign", &edit));
    assert(wena_card_init(&wrong, "foreign", "other", "os", "ol", "Foreign", 0, 0));
    assert(!wena_labels_open(&state, "b", &wrong));
    reopen(&state, &card);
    wena_labels_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    sql(database, "PRAGMA foreign_keys=ON");
    assert(wena_label_mutation_init(&adapter, database, "u", "b"));
    assert(wena_labels_open(&state, "b", &card));
    assert(state.snapshot->label_count == 2 && state.snapshot->assigned[0] &&
        !strcmp(state.snapshot->labels[0].name, "After rollback") &&
        !strcmp(state.snapshot->labels[0].color, "#001122"));
    other = wena_label_snapshot_create();
    assert(other && wena_label_mutation_load(&adapter, "b", "c2", other));
    memset(&edit, 0, sizeof(edit));
    edit.action = WENA_LABEL_ASSIGN;
    edit.label_id = label_id;
    edit.expected_board_version = other->board_version;
    edit.expected_card_version = other->card_version;
    edit.expected_label_version = other->label_versions[0];
    assert(wena_label_mutation_save(&adapter, "b", "c2", &edit));
    wena_label_snapshot_free(other);
    sql(database, "UPDATE cards SET archived=1,version=version+1 WHERE id='c2'");
    reopen(&state, &card);
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Delete", "Discarded destructive draft");
    assert(state.action == WENA_LABEL_DELETE && state.affected_cards == 2 &&
        !strcmp(state.name, "After rollback"));
    frame(&state, &card, "Cancel", NULL);
    assert(number(database, "SELECT count(*) FROM card_labels") == 2);
    frame(&state, &card, "Change Label", NULL);
    frame(&state, &card, "Delete", NULL);
    keys = number(database, "SELECT count(*) FROM idempotency_keys");
    card_version = number(database, "SELECT version FROM cards WHERE id='c'");
    archived_version = number(database, "SELECT version FROM cards WHERE id='c2'");
    sql(database, "CREATE TRIGGER rollback_delete BEFORE INSERT ON idempotency_keys "
        "BEGIN SELECT RAISE(ABORT,'rollback delete'); END");
    frame(&state, &card, "Delete", NULL);
    assert(state.error && state.action == WENA_LABEL_DELETE &&
        number(database, "SELECT count(*) FROM labels") == 2 &&
        number(database, "SELECT count(*) FROM card_labels") == 2 &&
        number(database, "SELECT version FROM cards WHERE id='c'") == card_version &&
        number(database, "SELECT version FROM cards WHERE id='c2'") == archived_version &&
        number(database, "SELECT count(*) FROM idempotency_keys") == keys);
    sql(database, "DROP TRIGGER rollback_delete");
    frame(&state, &card, "Delete", NULL);
    assert(!state.error && !state.action && state.snapshot->label_count == 1 &&
        !strcmp(state.snapshot->labels[0].name, "Other label"));
    assert(number(database, "SELECT count(*) FROM card_labels") == 0 &&
        number(database, "SELECT version FROM cards WHERE id='c'") == card_version + 1 &&
        number(database, "SELECT version FROM cards WHERE id='c2'") == archived_version + 1);
    wena_labels_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    assert(wena_label_mutation_init(&adapter, database, "u", "b"));
    assert(wena_labels_open(&state, "b", NULL));
    assert(state.snapshot->label_count == 1 && !state.snapshot->card_version &&
        !strcmp(state.snapshot->labels[0].name, "Other label"));
    wena_labels_close(&state);
    assert(sqlite3_close(database) == SQLITE_OK);
    assert(sqlite3_open(argv[4], &database) == SQLITE_OK);
    schema(database,argv[5]);schema(database,argv[6]);
    selected_labels(database);
    reboard_labels(argv);
    assert(sqlite3_close(database)==SQLITE_OK);
    assert(sqlite3_open(argv[4], &database)==SQLITE_OK);
    assert(number(database,"SELECT version FROM boards WHERE id='batch'")== (sqlite3_int64)WENA_VERSION_READ_MAX);
    assert(number(database,"SELECT count(*) FROM card_labels WHERE board_id='batch'")==2);
    assert(sqlite3_close(database)==SQLITE_OK);
    puts("Labels UI SQLite writes, cancel, revisions, scopes, replay, rollback and reopening passed");
    return 0;
}
