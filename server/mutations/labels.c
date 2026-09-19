#include "labels.h"
#include "../../models/label.h"
#include <limits.h>
#include <string.h>

typedef struct LabelCommand {
    const char *board;
    char card[65],label[65],name[WENA_LABEL_NAME_CAPACITY],color[WENA_COLOR_CAPACITY];
    unsigned long board_version,label_version,card_version;
} LabelCommand;

/* Bind one consistent numbered parameter contract for this feature. */
static int statement(sqlite3 *db,const char *sql,const LabelCommand *edit,
    int mode,sqlite3_int64 *count)
{
    sqlite3_stmt *query;
    int ok,parameters;
    if (sqlite3_prepare_v2(db,sql,-1,&query,NULL)!=SQLITE_OK) return 0;
    parameters=sqlite3_bind_parameter_count(query);ok=1;
#define TEXT(n,v) if (parameters>=n && sqlite3_bind_text(query,n,v,-1,SQLITE_TRANSIENT)!=SQLITE_OK) ok=0
#define NUMBER(n,v) if (parameters>=n && sqlite3_bind_int64(query,n,(sqlite3_int64)(v))!=SQLITE_OK) ok=0
    TEXT(1,edit->board);TEXT(2,edit->card);TEXT(3,edit->label);
    TEXT(4,edit->name);TEXT(5,edit->color);
    NUMBER(6,edit->board_version);NUMBER(7,edit->label_version);NUMBER(8,edit->card_version);
    NUMBER(9,WENA_VERSION_MUTATE_MAX);
#undef TEXT
#undef NUMBER
    if (ok && mode==1) {
        ok=sqlite3_step(query)==SQLITE_ROW && sqlite3_column_type(query,0)==SQLITE_INTEGER;
        if (ok) *count=sqlite3_column_int64(query,0);
    } else if (ok) {
        ok=sqlite3_step(query)==SQLITE_DONE;
        if (ok && mode==0) ok=sqlite3_changes(db)==1;
        if (ok && count) *count=sqlite3_changes(db);
    }
    if (sqlite3_finalize(query)!=SQLITE_OK) ok=0;
    return ok;
}
static int one(sqlite3 *db,const char *sql,const LabelCommand *edit)
{
    sqlite3_int64 count;
    return statement(db,sql,edit,1,&count) && count==1;
}
static const char *text_column(sqlite3_stmt *query,int column,size_t capacity)
{
    const char *text;
    int bytes;
    if (sqlite3_column_type(query,column)!=SQLITE_TEXT) return NULL;
    text=(const char *)sqlite3_column_text(query,column);
    bytes=sqlite3_column_bytes(query,column);
    if (!text || bytes<0 || (size_t)bytes>=capacity || memchr(text,0,(size_t)bytes)) return NULL;
    return text;
}
/* Validate the complete bounded catalogue before changing any of its rows. */
static int catalogue_valid(sqlite3 *db,const char *board)
{
    sqlite3_stmt *query;
    WenaLabel label;
    sqlite3_int64 position,version,created,updated;
    unsigned long previous;
    size_t count;
    int status,ok;
    const char *id,*name,*color;
    if (sqlite3_prepare_v2(db,"SELECT id,name,color,position,version,created_at,updated_at FROM labels WHERE board_id=?1 ORDER BY position,id",-1,&query,NULL)!=SQLITE_OK) return 0;
    ok=sqlite3_bind_text(query,1,board,-1,SQLITE_TRANSIENT)==SQLITE_OK;
    count=0;previous=0;status=SQLITE_DONE;
    while (ok && (status=sqlite3_step(query))==SQLITE_ROW) {
        if (++count>WENA_BOARD_LABEL_CAPACITY) {ok=0;break;}
        id=text_column(query,0,65);name=text_column(query,1,WENA_LABEL_NAME_CAPACITY);
        color=text_column(query,2,WENA_COLOR_CAPACITY);
        if (sqlite3_column_type(query,3)!=SQLITE_INTEGER ||
            sqlite3_column_type(query,4)!=SQLITE_INTEGER ||
            sqlite3_column_type(query,5)!=SQLITE_INTEGER ||
            sqlite3_column_type(query,6)!=SQLITE_INTEGER) {ok=0;break;}
        position=sqlite3_column_int64(query,3);version=sqlite3_column_int64(query,4);
        created=sqlite3_column_int64(query,5);updated=sqlite3_column_int64(query,6);
        if (position<0 || position>(sqlite3_int64)WENA_LABEL_POSITION_MAX ||
            version<=0 || version>(sqlite3_int64)WENA_VERSION_READ_MAX || created<0 || updated<created ||
            (count>1 && previous>=(unsigned long)position) ||
            !wena_label_init(&label,id,board,name,color,(unsigned long)position)) {ok=0;break;}
        previous=(unsigned long)position;
    }
    if (status!=SQLITE_DONE) ok=0;
    if (sqlite3_finalize(query)!=SQLITE_OK) ok=0;
    return ok;
}
static int version_field(const WenaDomainCommand *command,const char *name,
    int allow_zero,unsigned long *version)
{
    char text[32];
    return wena_mutation_text(command,name,text,sizeof(text),0) &&
        wena_mutation_decimal(text,allow_zero,WENA_VERSION_MUTATE_MAX,version);
}

int wena_sqlite_labels_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    LabelCommand edit;
    sqlite3_int64 count,changed;
    int create,assign,unassign,remove,editing;
    if (!db || !command || !board || !result_version || sqlite3_get_autocommit(db)) return 0;
    memset(&edit,0,sizeof(edit));edit.board=board;
    create=command->operation==WENA_DOMAIN_CREATE_LABEL;
    editing=command->operation==WENA_DOMAIN_EDIT_LABEL;
    remove=command->operation==WENA_DOMAIN_DELETE_LABEL;
    assign=command->operation==WENA_DOMAIN_ASSIGN_LABEL;
    unassign=command->operation==WENA_DOMAIN_UNASSIGN_LABEL;
    if ((!create && !editing && !remove && !assign && !unassign) ||
        !wena_model_identifier_valid(board) ||
        !version_field(command,"expectedBoardVersion",0,&edit.board_version) ||
        !wena_mutation_text(command,"cardId",edit.card,sizeof(edit.card),2) ||
        (edit.card[0] && !wena_model_identifier_valid(edit.card)) ||
        !version_field(command,"expectedCardVersion",!edit.card[0],&edit.card_version)) return 0;
    if (!create && (!wena_mutation_text(command,"labelId",edit.label,sizeof(edit.label),0) ||
        !wena_model_identifier_valid(edit.label) ||
        !version_field(command,"expectedLabelVersion",0,&edit.label_version))) return 0;
    if ((assign || unassign) && !edit.card[0]) return 0;
    if (create || editing) {
        if (!wena_mutation_text(command,"name",edit.name,sizeof(edit.name),2) ||
            !wena_mutation_text(command,"color",edit.color,sizeof(edit.color),2) ||
            !wena_label_name_string_valid(edit.name) || !wena_color_valid(edit.color)) return 0;
    }
    if (!one(db,"SELECT count(*) FROM boards WHERE id=?1 AND typeof(version)='integer' AND version=?6",&edit) ||
        !catalogue_valid(db,board)) return 0;
    if (edit.card[0] && !one(db,"SELECT count(*) FROM cards WHERE board_id=?1 AND id=?2 AND archived=0 AND typeof(version)='integer' AND version=?8",&edit)) return 0;
    if (!create && !one(db,"SELECT count(*) FROM labels WHERE board_id=?1 AND id=?3 AND typeof(version)='integer' AND version=?7",&edit)) return 0;
    if (editing && one(db,"SELECT count(*) FROM labels WHERE board_id=?1 AND id=?3 AND name=?4 AND color=?5",&edit)) {
        *result_version=edit.board_version;return 2;
    }
    if (create || editing) {
        if (!statement(db,"SELECT count(*) FROM labels WHERE board_id=?1 AND name=?4 AND color=?5",&edit,1,&count) || count) return 0;
    }
    if (assign || unassign) {
        /* Do not omit corrupt foreign-board or dangling selected-card rows. */
        if (!one(db,"SELECT CASE WHEN count(*)<=128 AND count(*)=count(CASE WHEN cl.board_id=?1 AND l.id IS NOT NULL THEN 1 END) THEN 1 ELSE 0 END FROM card_labels cl LEFT JOIN labels l ON l.board_id=cl.board_id AND l.id=cl.label_id WHERE cl.card_id=?2",&edit) ||
            !statement(db,"SELECT count(*) FROM card_labels WHERE board_id=?1 AND card_id=?2 AND label_id=?3",&edit,1,&count)) return 0;
        if ((assign && count==1) || (unassign && count==0)) {
            *result_version=edit.board_version;return 2;
        }
        if (assign && !one(db,"SELECT CASE WHEN count(*)<128 THEN 1 ELSE 0 END FROM card_labels WHERE card_id=?2",&edit)) return 0;
        if (!statement(db,assign?
            "INSERT INTO card_labels(board_id,card_id,label_id) VALUES(?1,?2,?3)":
            "DELETE FROM card_labels WHERE board_id=?1 AND card_id=?2 AND label_id=?3",&edit,0,NULL) ||
            !statement(db,"UPDATE cards SET version=version+1 WHERE board_id=?1 AND id=?2 AND version=?8 AND archived=0",&edit,0,NULL)) return 0;
    } else if (remove) {
        if (!one(db,"SELECT CASE WHEN count(*)=count(CASE WHEN c.id IS NOT NULL AND typeof(c.version)='integer' AND c.version>=1 AND c.version<=?9 THEN 1 END) THEN 1 ELSE 0 END FROM card_labels cl LEFT JOIN cards c ON c.board_id=cl.board_id AND c.id=cl.card_id WHERE cl.board_id=?1 AND cl.label_id=?3",&edit) ||
            !statement(db,"SELECT count(*) FROM card_labels WHERE board_id=?1 AND label_id=?3",&edit,1,&count) ||
            !statement(db,"UPDATE cards SET version=version+1 WHERE board_id=?1 AND id IN(SELECT card_id FROM card_labels WHERE board_id=?1 AND label_id=?3)",&edit,2,&changed) || changed!=count ||
            !statement(db,"DELETE FROM card_labels WHERE board_id=?1 AND label_id=?3",&edit,2,&changed) || changed!=count ||
            !statement(db,"DELETE FROM labels WHERE board_id=?1 AND id=?3 AND version=?7",&edit,0,NULL)) return 0;
    } else if (create) {
        if (!one(db,"SELECT CASE WHEN count(*)<128 AND COALESCE(max(position),-1)<2147483647 THEN 1 ELSE 0 END FROM labels WHERE board_id=?1",&edit)) return 0;
        wena_mutation_identity(command,"create-label",edit.label);
        if (!statement(db,"INSERT INTO labels(board_id,id,name,color,position,version,created_at,updated_at) VALUES(?1,?3,?4,?5,COALESCE((SELECT max(position)+1 FROM labels WHERE board_id=?1),0),1,CAST(strftime('%s','now') AS INTEGER)*1000,CAST(strftime('%s','now') AS INTEGER)*1000)",&edit,0,NULL)) return 0;
    } else {
        if (!statement(db,"UPDATE labels SET name=?4,color=?5,version=version+1,updated_at=max(updated_at,CAST(strftime('%s','now') AS INTEGER)*1000) WHERE board_id=?1 AND id=?3 AND version=?7",&edit,0,NULL)) return 0;
    }
    if (!statement(db,"UPDATE boards SET version=version+1 WHERE id=?1 AND version=?6",&edit,0,NULL)) return 0;
    *result_version=edit.board_version+1;
    return 1;
}
