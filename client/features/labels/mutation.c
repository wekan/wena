#include "mutation.h"
#include "../../../server/list_state.h"
#include "../../../server/mutations/card_archive.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int scope_valid(WenaLabelMutation *adapter,const char *board,const char *card)
{
    return adapter && adapter->persistence.database &&
        wena_model_identifier_valid(board) && !strcmp(board,adapter->board_id) &&
        (!card || !card[0] || wena_model_identifier_valid(card));
}
int wena_label_mutation_init(WenaLabelMutation *adapter,sqlite3 *database,
    const char *actor,const char *board)
{
    if (!adapter) return 0;
    memset(adapter,0,sizeof(*adapter));
    if (!database || !wena_model_identifier_valid(actor) ||
        !wena_model_identifier_valid(board)) return 0;
    wena_sqlite_persistence_init(&adapter->persistence,database);
    strcpy(adapter->actor_id,actor);strcpy(adapter->board_id,board);
    sprintf(adapter->route,"/b/%s/native",board);
    return 1;
}
static const char *read_text(sqlite3_stmt *statement,int column,size_t capacity)
{
    const char *value;
    int bytes;
    if (sqlite3_column_type(statement,column)!=SQLITE_TEXT) return NULL;
    value=(const char *)sqlite3_column_text(statement,column);
    bytes=sqlite3_column_bytes(statement,column);
    if (!value || bytes<0 || (size_t)bytes>=capacity || memchr(value,0,(size_t)bytes)) return NULL;
    return value;
}
static int read_integer(sqlite3_stmt *statement,int column,unsigned long maximum,
    unsigned long *output)
{
    sqlite3_int64 value;
    if (sqlite3_column_type(statement,column)!=SQLITE_INTEGER) return 0;
    value=sqlite3_column_int64(statement,column);
    if (value<0 || value>(sqlite3_int64)maximum) return 0;
    *output=(unsigned long)value;return 1;
}
static int prepare(WenaLabelMutation *adapter,const char *sql,const char *board,
    const char *card,sqlite3_stmt **statement)
{
    int parameters;
    if (sqlite3_prepare_v2(adapter->persistence.database,sql,-1,statement,NULL)!=SQLITE_OK) return 0;
    parameters=sqlite3_bind_parameter_count(*statement);
    if (sqlite3_bind_text(*statement,1,board,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
        (parameters>=2 && sqlite3_bind_text(*statement,2,card?card:"",-1,SQLITE_TRANSIENT)!=SQLITE_OK) ||
        (parameters>=3 && sqlite3_bind_text(*statement,3,adapter->actor_id,-1,SQLITE_TRANSIENT)!=SQLITE_OK)) {
        sqlite3_finalize(*statement);*statement=NULL;return 0;
    }
    return 1;
}
static int load_locked(WenaLabelMutation *adapter,const char *board,const char *card,
    WenaLabelSnapshot *candidate)
{
    sqlite3_stmt *statement;
    const char *id,*name,*color,*scope;
    unsigned long position,version;
    sqlite3_int64 created,updated;
    size_t index,assignment_count;
    int step,success;
    statement=NULL;success=0;
    if (!prepare(adapter,"SELECT version FROM boards WHERE id=?1 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)",board,card,&statement) ||
        sqlite3_step(statement)!=SQLITE_ROW ||
        !read_integer(statement,0,WENA_VERSION_READ_MAX,&candidate->board_version) ||
        !candidate->board_version) goto done;
    sqlite3_finalize(statement);statement=NULL;strcpy(candidate->board_id,board);
    if (card && card[0]) {
        if (!prepare(adapter,"SELECT version FROM cards WHERE board_id=?1 AND id=?2 AND archived=0",board,card,&statement) ||
            sqlite3_step(statement)!=SQLITE_ROW ||
            !read_integer(statement,0,WENA_VERSION_READ_MAX,&candidate->card_version) ||
            !candidate->card_version) goto done;
        sqlite3_finalize(statement);statement=NULL;strcpy(candidate->card_id,card);
    }
    if (!prepare(adapter,"SELECT id,name,color,position,version,created_at,updated_at,"
        "(SELECT count(*) FROM card_labels cl WHERE cl.board_id=labels.board_id AND cl.label_id=labels.id),"
        "(SELECT count(*) FROM card_labels cl JOIN cards c ON c.board_id=cl.board_id AND c.id=cl.card_id WHERE cl.board_id=labels.board_id AND cl.label_id=labels.id)"
        " FROM labels WHERE board_id=?1 ORDER BY position,id",board,card,&statement)) goto done;
    while ((step=sqlite3_step(statement))==SQLITE_ROW) {
        if (candidate->label_count>=WENA_BOARD_LABEL_CAPACITY) goto done;
        index=candidate->label_count;
        id=read_text(statement,0,65);name=read_text(statement,1,WENA_LABEL_NAME_CAPACITY);
        color=read_text(statement,2,WENA_COLOR_CAPACITY);
        if (!read_integer(statement,3,WENA_LABEL_POSITION_MAX,&position) ||
            !read_integer(statement,4,WENA_VERSION_READ_MAX,&version) || !version ||
            sqlite3_column_type(statement,5)!=SQLITE_INTEGER ||
            sqlite3_column_type(statement,6)!=SQLITE_INTEGER ||
            !read_integer(statement,7,(unsigned long)LONG_MAX,&candidate->assigned_card_counts[index]) ||
            sqlite3_column_type(statement,8)!=SQLITE_INTEGER ||
            sqlite3_column_int64(statement,8)!=sqlite3_column_int64(statement,7) ||
            !wena_label_init(&candidate->labels[index],id,board,name,color,position)) goto done;
        created=sqlite3_column_int64(statement,5);updated=sqlite3_column_int64(statement,6);
        if (created<0 || updated<created) goto done;
        candidate->label_versions[index]=version;++candidate->label_count;
    }
    if (step!=SQLITE_DONE) goto done;
    sqlite3_finalize(statement);statement=NULL;
    if (candidate->card_id[0]) {
        if (!prepare(adapter,"SELECT board_id,label_id FROM card_labels WHERE card_id=?2 ORDER BY label_id",board,card,&statement)) goto done;
        assignment_count=0;
        while ((step=sqlite3_step(statement))==SQLITE_ROW) {
            if (++assignment_count>WENA_BOARD_LABEL_CAPACITY) goto done;
            scope=read_text(statement,0,65);id=read_text(statement,1,65);
            if (!scope || strcmp(scope,board) || !wena_model_identifier_valid(id)) goto done;
            for (index=0;index<candidate->label_count;++index)
                if (!strcmp(candidate->labels[index].id,id)) break;
            if (index==candidate->label_count || candidate->assigned[index]) goto done;
            candidate->assigned[index]=1;
        }
        if (step!=SQLITE_DONE) goto done;
        sqlite3_finalize(statement);statement=NULL;
    }
    success=wena_label_snapshot_valid(candidate,board,card);
done:
    if (statement) sqlite3_finalize(statement);
    return success;
}
int wena_label_mutation_load(void *context,const char *board,const char *card,
    WenaLabelSnapshot *output)
{
    WenaLabelMutation *adapter;
    WenaLabelSnapshot *candidate;
    int success;
    adapter=(WenaLabelMutation *)context;
    if (!scope_valid(adapter,board,card) || !output) return 0;
    candidate=wena_label_snapshot_create();if (!candidate) return 0;
    if (sqlite3_exec(adapter->persistence.database,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK) {
        free(candidate);return 0;
    }
    success=load_locked(adapter,board,card,candidate) &&
        sqlite3_exec(adapter->persistence.database,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if (success) *output=*candidate;
    else sqlite3_exec(adapter->persistence.database,"ROLLBACK",NULL,NULL,NULL);
    free(candidate);return success;
}
static int load_board_locked(WenaLabelMutation *adapter,const char *board,WenaLabelBoardSnapshot *candidate)
{
    sqlite3_stmt *query;const char *card_id,*label_id;
    size_t card_index,label_index;unsigned char mask;unsigned long archived;
    int success,status;
    memset(candidate,0,sizeof(*candidate));
    query=NULL;success=0;
    if (!load_locked(adapter,board,NULL,&candidate->catalogue) ||
        !prepare(adapter,"SELECT id,version,archived FROM cards WHERE board_id=?1 ORDER BY id COLLATE BINARY",board,NULL,&query)) goto done;
    while ((status=sqlite3_step(query))==SQLITE_ROW) {
        if (candidate->card_count>=WENA_LABEL_BOARD_CARD_CAPACITY) goto done;
        card_index=candidate->card_count;
        card_id=read_text(query,0,65);
        if (!wena_model_identifier_valid(card_id) ||
            !read_integer(query,1,WENA_VERSION_READ_MAX,&candidate->card_versions[card_index]) ||
            !candidate->card_versions[card_index] || !read_integer(query,2,1,&archived)) goto done;
        strcpy(candidate->card_ids[card_index],card_id);
        ++candidate->card_count;
    }
    if (status!=SQLITE_DONE) goto done;
    sqlite3_finalize(query);query=NULL;
    if (!prepare(adapter,"SELECT card_id,label_id FROM card_labels WHERE board_id=?1 ORDER BY card_id,label_id",board,NULL,&query)) goto done;
    while ((status=sqlite3_step(query))==SQLITE_ROW) {
        card_id=read_text(query,0,65);label_id=read_text(query,1,65);
        if (!wena_model_identifier_valid(card_id) || !wena_model_identifier_valid(label_id)) goto done;
        card_index=wena_label_board_card_index(candidate,card_id);
        if (card_index==candidate->card_count) goto done;
        for (label_index=0;label_index<candidate->catalogue.label_count;++label_index)
            if (!strcmp(candidate->catalogue.labels[label_index].id,label_id)) break;
        if (label_index==candidate->catalogue.label_count) goto done;
        mask=(unsigned char)(1u<<(label_index%8u));
        if (candidate->assignments[card_index][label_index/8u]&mask) goto done;
        candidate->assignments[card_index][label_index/8u]|=mask;
    }
    if (status!=SQLITE_DONE) goto done;
    sqlite3_finalize(query);query=NULL;
    /* Joining through all board cards also catches foreign-board assignments
     * deliberately inserted with foreign-key enforcement disabled. */
    if (!prepare(adapter,"SELECT cl.board_id FROM cards c JOIN card_labels cl ON cl.card_id=c.id WHERE c.board_id=?1 AND cl.board_id<>?1",board,NULL,&query)) goto done;
    status=sqlite3_step(query);
    if (status==SQLITE_ROW) goto done;
    if (status!=SQLITE_DONE) goto done;
    sqlite3_finalize(query);query=NULL;
    success=wena_label_board_snapshot_valid(candidate,board);
done:
    if(query)sqlite3_finalize(query);
    return success;
}
int wena_label_mutation_load_board(void *context,const char *board,WenaLabelBoardSnapshot *output)
{
    WenaLabelMutation *adapter;WenaLabelBoardSnapshot *candidate;int success;
    adapter=(WenaLabelMutation*)context;
    if(!scope_valid(adapter,board,NULL)||!output)return 0;
    candidate=wena_label_board_snapshot_create();if(!candidate)return 0;
    if(sqlite3_exec(adapter->persistence.database,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK){free(candidate);return 0;}
    success=load_board_locked(adapter,board,candidate)&&
        sqlite3_exec(adapter->persistence.database,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if(success)*output=*candidate;
    else sqlite3_exec(adapter->persistence.database,"ROLLBACK",NULL,NULL,NULL);
    free(candidate);return success;
}
static const char *operation(WenaLabelAction action,WenaDomainOperation *domain)
{
    switch (action) {
    case WENA_LABEL_CREATE:*domain=WENA_DOMAIN_CREATE_LABEL;return "create-label";
    case WENA_LABEL_EDIT:*domain=WENA_DOMAIN_EDIT_LABEL;return "edit-label";
    case WENA_LABEL_DELETE:*domain=WENA_DOMAIN_DELETE_LABEL;return "delete-label";
    case WENA_LABEL_ASSIGN:*domain=WENA_DOMAIN_ASSIGN_LABEL;return "assign-label";
    case WENA_LABEL_UNASSIGN:*domain=WENA_DOMAIN_UNASSIGN_LABEL;return "unassign-label";
    default:return NULL;
    }
}
static void encode(const char *text,char *output)
{
    const char hex[]="0123456789ABCDEF";
    unsigned char byte;
    while (*text) {
        byte=(unsigned char)*text++;*output++='%';
        *output++=hex[byte>>4];*output++=hex[byte&15];
    }
    *output=0;
}
int wena_label_mutation_save_request(WenaLabelMutation *adapter,const char *board,
    const char *card,const WenaLabelEdit *edit,unsigned long request_version)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    char name[WENA_LABEL_NAME_CAPACITY*3],color[WENA_COLOR_CAPACITY*3];
    int create,definition,card_mode;
    if (!scope_valid(adapter,board,card) || !edit || !request_version ||
        request_version>=(unsigned long)LONG_MAX || !edit->expected_board_version ||
        edit->expected_board_version>WENA_VERSION_MUTATE_MAX) return 0;
    memset(&command,0,sizeof(command));
    if (!operation(edit->action,&command.operation)) return 0;
    create=edit->action==WENA_LABEL_CREATE;
    definition=create || edit->action==WENA_LABEL_EDIT;
    card_mode=card && card[0];
    if (!create && (!wena_model_identifier_valid(edit->label_id) ||
        !edit->expected_label_version || edit->expected_label_version>WENA_VERSION_MUTATE_MAX)) return 0;
    if (card_mode && (!edit->expected_card_version ||
        edit->expected_card_version>WENA_VERSION_MUTATE_MAX)) return 0;
    if (!card_mode && (edit->action==WENA_LABEL_ASSIGN || edit->action==WENA_LABEL_UNASSIGN)) return 0;
    name[0]=color[0]=0;
    if (definition) {
        if (!wena_label_name_string_valid(edit->name) || !wena_color_valid(edit->color)) return 0;
        encode(edit->name,name);encode(edit->color,color);
    }
    command.request_version=request_version;
    strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"cardId=%s&expectedBoardVersion=%lu&expectedCardVersion=%lu&labelId=%s&expectedLabelVersion=%lu&name=%s&color=%s",
        card_mode?card:"",edit->expected_board_version,card_mode?edit->expected_card_version:0UL,
        create?"":edit->label_id,create?0UL:edit->expected_label_version,name,color);
    command.form_body_length=strlen(command.form_body);
    return wena_sqlite_persistence_apply(&adapter->persistence,&command,&response);
}
static int next_request(WenaLabelMutation *adapter,const char *name,unsigned long *request)
{
    sqlite3_stmt *statement;sqlite3_int64 version;int ok;
    if(sqlite3_prepare_v2(adapter->persistence.database,"SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE actor_id=?1 AND route=?2 AND operation=?3",-1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(statement,1,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,2,adapter->route,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,3,name,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_step(statement)==SQLITE_ROW&&sqlite3_column_type(statement,0)==SQLITE_INTEGER;
    version=ok?sqlite3_column_int64(statement,0):-1;
    if(sqlite3_finalize(statement)!=SQLITE_OK)return 0;
    if(version<0||version>=(sqlite3_int64)LONG_MAX-1)return 0;
    *request=(unsigned long)version+1UL;return 1;
}
int wena_label_mutation_save(void *context,const char *board,const char *card,const WenaLabelEdit *edit)
{
    WenaLabelMutation *adapter;WenaDomainOperation domain;const char *name;unsigned long request;
    adapter=(WenaLabelMutation*)context;
    if(!scope_valid(adapter,board,card)||!edit||!(name=operation(edit->action,&domain))||!next_request(adapter,name,&request))return 0;
    return wena_label_mutation_save_request(adapter,board,card,edit,request);
}

static int selected_active(WenaLabelMutation *adapter,const char *board,const char *card)
{
    sqlite3_stmt *query;const char *list,*lane;int ok,archived;sqlite3_int64 at;
    if(!wena_sqlite_card_archive_read(adapter->persistence.database,board,card,0,&archived,&at)||archived)return 0;
    if(!prepare(adapter,"SELECT list_id,swimlane_id FROM cards WHERE board_id=?1 AND id=?2 AND archived=0",board,card,&query))return 0;
    ok=sqlite3_step(query)==SQLITE_ROW;
    if(ok){
        list=read_text(query,0,65);lane=read_text(query,1,65);
        ok=wena_model_identifier_valid(list)&&wena_model_identifier_valid(lane)&&
            wena_sqlite_list_active(adapter->persistence.database,board,list)&&
            wena_sqlite_swimlane_active(adapter->persistence.database,board,lane)&&sqlite3_step(query)==SQLITE_DONE;
    }
    if(sqlite3_finalize(query)!=SQLITE_OK)ok=0;return ok;
}
int wena_label_mutation_selected_load(void *context,const char *board,const WenaId *ids,
    size_t count,WenaLabelSelectionSnapshot **output)
{
    WenaLabelMutation *adapter;WenaLabelBoardSnapshot *all;WenaLabelSelectionSnapshot *candidate;
    size_t i,j,index;int ok;
    adapter=(WenaLabelMutation*)context;
    if(!scope_valid(adapter,board,NULL)||!output||!ids||!count||count>WENA_LABEL_BOARD_CARD_CAPACITY)return 0;
    for(i=0;i<count;++i){
        if(!wena_model_identifier_valid(ids[i]))return 0;
        for(j=0;j<i;++j)if(!strcmp(ids[i],ids[j]))return 0;
    }
    all=wena_label_board_snapshot_create();candidate=(WenaLabelSelectionSnapshot*)calloc(1,sizeof(*candidate));
    if(!all||!candidate){free(all);free(candidate);return 0;}
    if(sqlite3_exec(adapter->persistence.database,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK){free(all);free(candidate);return 0;}
    ok=0;
    if(!load_board_locked(adapter,board,all))goto done;
    candidate->catalogue=all->catalogue;candidate->card_count=count;
    for(i=0;i<count;++i){
        index=wena_label_board_card_index(all,ids[i]);
        if(index==all->card_count||!selected_active(adapter,board,ids[i]))goto done;
        strcpy(candidate->cards[i].id,ids[i]);candidate->cards[i].version=all->card_versions[index];
        for(j=0;j<all->catalogue.label_count;++j)
            if(all->assignments[index][j/8u]&(1u<<(j%8u)))++candidate->assigned_counts[j];
    }
    if(!wena_label_selection_snapshot_valid(candidate,board)||
        sqlite3_exec(adapter->persistence.database,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto done;
    free(*output);*output=candidate;candidate=NULL;ok=1;
done:
    if(!ok)sqlite3_exec(adapter->persistence.database,"ROLLBACK",NULL,NULL,NULL);
    free(all);free(candidate);return ok;
}

typedef struct LabelPublication {
    WenaLabelMutation *adapter;
    const char *board;
    WenaLabelBoardSnapshot *snapshot;
} LabelPublication;
static int prepare_labels(void *context,sqlite3 *database)
{
    LabelPublication *publication;publication=(LabelPublication*)context;
    return database==publication->adapter->persistence.database&&
        load_board_locked(publication->adapter,publication->board,publication->snapshot);
}
int wena_label_mutation_selected_save_request(WenaLabelMutation *adapter,const char *board,
    const WenaLabelSelectionSnapshot *selection,const char *label,int assign,
    WenaLabelBoardSnapshot *output,unsigned long request)
{
    WenaDomainCommand command;WenaRegionResponse response;LabelPublication publication;
    size_t index;int ok;
    if(!scope_valid(adapter,board,NULL)||!output||!wena_label_selection_snapshot_valid(selection,board)||
        !wena_model_identifier_valid(label)||(assign!=0&&assign!=1)||!request||request>=(unsigned long)LONG_MAX||
        adapter->persistence.prepare_publish||selection->catalogue.board_version>WENA_VERSION_MUTATE_MAX)return 0;
    for(index=0;index<selection->catalogue.label_count;++index)
        if(!strcmp(selection->catalogue.labels[index].id,label))break;
    if(index==selection->catalogue.label_count||selection->catalogue.label_versions[index]>WENA_VERSION_MUTATE_MAX)return 0;
    memset(&command,0,sizeof(command));command.request_version=request;
    command.operation=assign?WENA_DOMAIN_ASSIGN_SELECTED_LABEL:WENA_DOMAIN_UNASSIGN_SELECTED_LABEL;
    strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    command.selected_cards=selection->cards;command.selected_card_count=selection->card_count;
    sprintf(command.form_body,"labelId=%s&expectedBoardVersion=%lu&expectedLabelVersion=%lu",label,
        selection->catalogue.board_version,selection->catalogue.label_versions[index]);
    command.form_body_length=strlen(command.form_body);
    publication.adapter=adapter;publication.board=board;publication.snapshot=wena_label_board_snapshot_create();
    if(!publication.snapshot)return 0;
    adapter->persistence.prepare_publish=prepare_labels;adapter->persistence.publish_context=&publication;
    ok=wena_sqlite_persistence_apply(&adapter->persistence,&command,&response);
    adapter->persistence.prepare_publish=NULL;adapter->persistence.publish_context=NULL;
    if(ok)*output=*publication.snapshot;
    free(publication.snapshot);return ok;
}

int wena_label_mutation_selected_save(void *context,const char *board,
    const WenaLabelSelectionSnapshot *selection,const char *label,int assign,WenaLabelBoardSnapshot *output)
{
    WenaLabelMutation *adapter;unsigned long request;
    adapter=(WenaLabelMutation*)context;
    if(!scope_valid(adapter,board,NULL)||(assign!=0&&assign!=1)||
        !next_request(adapter,assign?"assign-selected-label":"unassign-selected-label",&request))return 0;
    return wena_label_mutation_selected_save_request(adapter,board,selection,label,assign,output,request);
}
