#include "../client/features/boards/presentation.h"
#include "../client/features/checklist_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Trace {
    unsigned long statements;
    unsigned long count_queries;
    int flip;
    int flipped;
    WenaBoardSettingsMutation *writer;
    unsigned long version;
} Trace;
typedef struct Access {
    int deny_labels;
    int deny_summary;
    unsigned long denials;
} Access;
static int trace(unsigned int type,void *context,void *statement,void *unused)
{
    Trace *work;
    const char *text;
    (void)unused;
    if (type!=SQLITE_TRACE_STMT) return 0;
    work=(Trace *)context;text=sqlite3_sql((sqlite3_stmt *)statement);
    ++work->statements;
    if (text && (strstr(text,"checklist_items") || strstr(text,"FROM checklists") ||
        strstr(text,"JOIN checklists"))) ++work->count_queries;
    if (work->flip && text && !strcmp(text,"BEGIN")) {
        work->flip=0;
        work->flipped=wena_board_settings_mutation_save(work->writer,"b",work->version,0);
    }
    return 0;
}
static int authorize(void *context,int action,const char *first,const char *second,
    const char *database,const char *trigger)
{
    Access *access;
    (void)second;(void)database;(void)trigger;
    access=(Access *)context;
    if (action==SQLITE_READ && first &&
        ((access->deny_labels && !strcmp(first,"labels")) ||
         (access->deny_summary && (!strcmp(first,"checklists") || !strcmp(first,"checklist_items"))))) {
        ++access->denials;return SQLITE_DENY;
    }
    return SQLITE_OK;
}
static void sql(sqlite3 *database,const char *query)
{
    int status;
    status=sqlite3_exec(database,query,NULL,NULL,NULL);
    if (status!=SQLITE_OK) fprintf(stderr,"SQL %s: %s\n",query,sqlite3_errmsg(database));
    assert(status==SQLITE_OK);
}
static sqlite3_int64 number(sqlite3 *database,const char *query)
{
    sqlite3_stmt *statement;
    sqlite3_int64 result;
    assert(sqlite3_prepare_v2(database,query,-1,&statement,NULL)==SQLITE_OK);
    assert(sqlite3_step(statement)==SQLITE_ROW);result=sqlite3_column_int64(statement,0);
    assert(sqlite3_finalize(statement)==SQLITE_OK);return result;
}
static void schema(sqlite3 *database,const char *path)
{
    FILE *file;
    long size;
    char *data;
    file=fopen(path,"rb");assert(file);assert(!fseek(file,0,SEEK_END));
    size=ftell(file);assert(size>0);rewind(file);
    data=(char *)malloc((size_t)size+1);assert(data);
    assert(fread(data,1,(size_t)size,file)==(size_t)size);data[size]=0;
    assert(!fclose(file));sql(database,data);free(data);
}
static void edit(WenaLabelEdit *change,const WenaLabelSnapshot *snapshot,
    WenaLabelAction action,const char *name)
{
    memset(change,0,sizeof(*change));change->action=action;
    change->expected_board_version=snapshot->board_version;
    change->expected_card_version=snapshot->card_version;
    change->name=name;change->color="green";
    if (snapshot->label_count) {
        change->label_id=snapshot->labels[0].id;
        change->expected_label_version=snapshot->label_versions[0];
    }
}
static void quiet(sqlite3 *database,WenaBoardPresentation *view,Trace *work,int valid)
{
    int index;
    memset(work,0,sizeof(*work));
    for (index=0;index<500;++index) assert(wena_board_presentation_poll(view)==valid);
    assert(work->statements==0 && work->count_queries==0);
    assert(sqlite3_get_autocommit(database));
}
int main(int argc,char **argv)
{
    sqlite3 *database,*second;
    WenaBoardPresentation view;
    WenaLabelSnapshot *labels;
    WenaLabelEdit label_edit;
    WenaBoardSettingsSnapshot settings;
    WenaBoardSettingsMutation external;
    WenaChecklistMutation checklist_adapter;
    WenaChecklistSnapshot *checklists;
    WenaChecklistEdit checklist_edit;
    const WenaChecklistCardSummary *card;
    const unsigned char *bits;
    Trace work;
    Access access;
    unsigned long denied;
    sqlite3_int64 keys,changes;
    assert(argc==3);assert(sqlite3_open(argv[2],&database)==SQLITE_OK);
    sql(database,"PRAGMA journal_mode=WAL;PRAGMA foreign_keys=ON");schema(database,argv[1]);
    sql(database,"INSERT INTO actors VALUES('u','User',1);INSERT INTO actors VALUES('v','Other',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1)");
    sql(database,"INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);INSERT INTO cards VALUES('archived','b','s','l','Archived',1,1,1)");
    sql(database,"INSERT INTO checklists(id,board_id,card_id,title,position) VALUES('check','b','c','Tasks',0)");
    sql(database,"INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES('one','b','c','check','First',0);INSERT INTO checklist_items(id,board_id,card_id,checklist_id,title,position) VALUES('two','b','c','check','Second',1)");
    labels=wena_label_snapshot_create();checklists=wena_checklist_snapshot_create();assert(labels && checklists);
    assert(!wena_board_presentation_init(NULL,database,"u","b"));
    assert(!wena_board_presentation_init(&view,database,"bad/id","b"));
    assert(!view.badges && !view.summary);wena_board_presentation_close(&view);
    memset(&work,0,sizeof(work));assert(sqlite3_trace_v2(database,SQLITE_TRACE_STMT,trace,&work)==SQLITE_OK);
    assert(wena_board_presentation_init(&view,database,"u","b"));
    assert(view.valid && view.summary_valid && !view.error && !view.summary_error);
    assert(!view.settings.show_checklist_count && !view.summary->enabled && !view.summary->card_count);
    assert(work.statements>0 && work.count_queries==0);
    quiet(database,&view,&work,1);
    /* Default-false no-op reloads just settings once, with zero count queries. */
    assert(wena_board_presentation_settings_save(&view,"b",view.settings.board_version,0));
    assert(!view.summary_valid && view.summary_pending);
    memset(&work,0,sizeof(work));assert(wena_board_presentation_poll(&view));
    assert(work.statements==1 && !work.count_queries);
    quiet(database,&view,&work,1);
    /* Label edits publish the exact catalogue and assignment bitsets. */
    assert(wena_board_presentation_labels_load(&view,"b","c",labels));
    edit(&label_edit,labels,WENA_LABEL_CREATE,"Priority");
    assert(wena_board_presentation_labels_save(&view,"b","c",&label_edit));
    assert(!view.valid && view.refresh_pending);assert(wena_board_presentation_poll(&view));
    assert(view.badges->catalogue.label_count==1);
    assert(wena_board_presentation_labels_load(&view,"b","c",labels));
    edit(&label_edit,labels,WENA_LABEL_ASSIGN,NULL);
    assert(wena_board_presentation_labels_save(&view,"b","c",&label_edit));assert(wena_board_presentation_poll(&view));
    bits=wena_label_board_assignments(view.badges,"b","c");assert(bits && (bits[0]&1));
    quiet(database,&view,&work,1);
    /* Enabling loads counts; subsequent checklist writes are detected cheaply. */
    assert(wena_board_presentation_settings_load(&view,"b",&settings));
    assert(wena_board_presentation_settings_save(&view,"b",settings.board_version,1));
    memset(&work,0,sizeof(work));assert(wena_board_presentation_poll(&view));
    assert(work.count_queries>0 && view.summary->enabled);
    card=wena_checklist_summary_find(view.summary,"c");assert(card && card->progress.total==2 && !card->progress.finished);
    assert(wena_checklist_mutation_init(&checklist_adapter,database,"u","b"));
    assert(wena_checklist_mutation_load(&checklist_adapter,"b","c",checklists));
    memset(&checklist_edit,0,sizeof(checklist_edit));checklist_edit.action=WENA_CHECKLIST_SET_FINISHED;
    checklist_edit.checklist_id="check";checklist_edit.item_id="one";checklist_edit.is_finished=1;
    checklist_edit.expected_card_version=checklists->card_version;
    checklist_edit.expected_checklist_version=checklists->checklist_versions[0];
    checklist_edit.expected_item_version=checklists->item_versions[0];
    assert(wena_checklist_mutation_save(&checklist_adapter,"b","c",&checklist_edit));
    assert(wena_board_presentation_poll(&view));card=wena_checklist_summary_find(view.summary,"c");
    assert(card && card->progress.total==2 && card->progress.finished==1 && card->progress.percent==50);
    quiet(database,&view,&work,1);
    /* Failed read after a committed save cannot turn that save into failure. */
    assert(wena_board_presentation_labels_load(&view,"b","c",labels));
    edit(&label_edit,labels,WENA_LABEL_EDIT,"Committed");
    assert(wena_board_presentation_labels_save(&view,"b","c",&label_edit));
    keys=number(database,"SELECT count(*) FROM idempotency_keys");
    memset(&access,0,sizeof(access));access.deny_labels=1;
    assert(sqlite3_set_authorizer(database,authorize,&access)==SQLITE_OK);
    assert(!wena_board_presentation_poll(&view));
    assert(!view.valid && view.error && !view.refresh_pending && view.summary_valid);
    denied=access.denials;assert(denied>0);
    quiet(database,&view,&work,0);assert(access.denials==denied);
    assert(sqlite3_set_authorizer(database,NULL,NULL)==SQLITE_OK);
    view.refresh_pending=1;assert(wena_board_presentation_poll(&view));
    assert(!strcmp(view.badges->catalogue.labels[0].name,"Committed"));
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    quiet(database,&view,&work,1);
    /* Summary failures similarly hide counts without retrying every frame. */
    memset(&access,0,sizeof(access));access.deny_summary=1;
    assert(sqlite3_set_authorizer(database,authorize,&access)==SQLITE_OK);
    view.summary_pending=1;assert(!wena_board_presentation_poll(&view));
    assert(view.valid && !view.summary_valid && view.summary_error && !view.summary_pending);
    denied=access.denials;assert(denied>0);quiet(database,&view,&work,0);assert(access.denials==denied);
    assert(sqlite3_set_authorizer(database,NULL,NULL)==SQLITE_OK);
    view.summary_pending=1;assert(wena_board_presentation_poll(&view));
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);
    /* Rolled-back writes may advance SQLite's counter, causing one safe reload. */
    assert(wena_board_presentation_labels_load(&view,"b","c",labels));
    edit(&label_edit,labels,WENA_LABEL_CREATE,"Rolled back");
    changes=view.observed_changes;
    sql(database,"CREATE TRIGGER reject_key BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'key'); END");
    assert(!wena_board_presentation_labels_save(&view,"b","c",&label_edit));
    sql(database,"DROP TRIGGER reject_key");
    assert(sqlite3_total_changes64(database)!=changes);
    assert(wena_board_presentation_poll(&view));assert(view.badges->catalogue.label_count==1);
    assert(number(database,"SELECT count(*) FROM idempotency_keys")==keys);quiet(database,&view,&work,1);
    /* External changes are observed on explicit panel reads, not by per-frame SQL. */
    assert(sqlite3_open(argv[2],&second)==SQLITE_OK);
    assert(wena_board_settings_mutation_init(&external,second,"v","b"));
    assert(wena_board_settings_mutation_load(&external,"b",&settings));
    assert(wena_board_settings_mutation_save(&external,"b",settings.board_version,0));
    quiet(database,&view,&work,1);
    assert(wena_board_presentation_settings_load(&view,"b",&settings));
    assert(!settings.show_checklist_count && !view.summary_valid && view.summary_pending);
    memset(&work,0,sizeof(work));assert(wena_board_presentation_poll(&view));
    assert(!view.summary->enabled && !work.count_queries);
    assert(wena_board_presentation_settings_save(&view,"b",settings.board_version,1));
    assert(wena_board_presentation_poll(&view));
    /* Force a setting commit precisely between the two atomic summary reads. */
    memset(&work,0,sizeof(work));work.flip=1;work.writer=&external;
    work.version=view.settings.board_version;view.summary_pending=1;
    assert(!wena_board_presentation_poll(&view));assert(work.flipped && !work.flip);
    assert(!view.summary_valid && view.summary_error && !view.summary_pending);
    quiet(database,&view,&work,0);
    assert(wena_board_presentation_settings_load(&view,"b",&settings));assert(!settings.show_checklist_count);
    assert(wena_board_presentation_poll(&view));assert(!view.summary->enabled);
    assert(sqlite3_close(second)==SQLITE_OK);
    /* Deletion removes rendered badges and preserves checklist counts on reopen. */
    assert(wena_board_presentation_labels_load(&view,"b","c",labels));
    edit(&label_edit,labels,WENA_LABEL_DELETE,NULL);
    assert(wena_board_presentation_labels_save(&view,"b","c",&label_edit));assert(wena_board_presentation_poll(&view));
    assert(!view.badges->catalogue.label_count);
    assert(wena_board_presentation_settings_load(&view,"b",&settings));
    assert(wena_board_presentation_settings_save(&view,"b",settings.board_version,1));assert(wena_board_presentation_poll(&view));
    wena_board_presentation_close(&view);wena_board_presentation_close(&view);
    assert(!view.badges && !view.summary && !wena_board_presentation_poll(&view));
    assert(sqlite3_close(database)==SQLITE_OK);assert(sqlite3_open(argv[2],&database)==SQLITE_OK);
    assert(wena_board_presentation_init(&view,database,"u","b"));
    assert(view.valid && view.summary_valid && !view.badges->catalogue.label_count);
    card=wena_checklist_summary_find(view.summary,"c");assert(card && card->progress.finished==1 && card->progress.total==2);
    wena_board_presentation_close(&view);
    /* Invalid existing actor yields visible errors, resource ownership still safe. */
    assert(wena_board_presentation_init(&view,database,"missing","b"));assert(view.error && view.summary_error);
    wena_board_presentation_close(&view);assert(sqlite3_close(database)==SQLITE_OK);
    wena_label_snapshot_free(labels);wena_checklist_snapshot_free(checklists);
    puts("board presentation refresh, zero-SQL frames, commit/read failure, retry, torn setting and reopen passed");
    return 0;
}
