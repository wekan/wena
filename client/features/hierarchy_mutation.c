#include "../../server/mutations/list_wip.h"
#include "../../server/mutations/swimlane_archive.h"
#include "../../server/mutations/hierarchy_colors.h"
#include "../../server/list_state.h"
#include "hierarchy_mutation.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *selected(WenaHierarchyMutation *adapter, const char *board,
                        WenaHierarchyKind kind, const char *target)
{
    WenaSqliteBoardSnapshot *snapshot;
    size_t index;
    if (!adapter || !adapter->persistence.database || !wena_model_identifier_valid(board) ||
        !wena_model_identifier_valid(target) || strcmp(adapter->board_id, board) != 0 ||
        !adapter->snapshot) return NULL;
    snapshot = adapter->snapshot;
    if (snapshot->board.archived || strcmp(snapshot->board.id, board) != 0 ||
        snapshot->list_count > WENA_SQLITE_BOARD_MAX_LISTS ||
        snapshot->swimlane_count > WENA_SQLITE_BOARD_MAX_SWIMLANES) return NULL;
    if (kind == WENA_HIERARCHY_BOARD)
        return strcmp(target, board) == 0 ? snapshot->board.title : NULL;
    if (kind == WENA_HIERARCHY_LIST) {
        for (index = 0; index < snapshot->list_count; ++index) {
            if (!snapshot->lists[index].archived &&
                strcmp(snapshot->lists[index].board_id, board) == 0 &&
                strcmp(snapshot->lists[index].id, target) == 0)
                return snapshot->lists[index].title;
        }
    } else if (kind == WENA_HIERARCHY_SWIMLANE) {
        for (index = 0; index < snapshot->swimlane_count; ++index) {
            if (!snapshot->swimlanes[index].archived &&
                strcmp(snapshot->swimlanes[index].board_id, board) == 0 &&
                strcmp(snapshot->swimlanes[index].id, target) == 0)
                return snapshot->swimlanes[index].title;
        }
    }
    return NULL;
}

int wena_hierarchy_mutation_init(WenaHierarchyMutation *adapter, sqlite3 *database,
    const char *actor, const char *board, WenaSqliteBoardSnapshot *snapshot)
{
    if (!adapter) return 0;
    memset(adapter, 0, sizeof(*adapter));
    if (!database || !wena_model_identifier_valid(actor) ||
        !wena_model_identifier_valid(board) || !snapshot ||
        strcmp(snapshot->board.id, board) != 0 || snapshot->board.archived ||
        snapshot->list_count > WENA_SQLITE_BOARD_MAX_LISTS ||
        snapshot->swimlane_count > WENA_SQLITE_BOARD_MAX_SWIMLANES) return 0;
    wena_sqlite_persistence_init(&adapter->persistence, database);
    strcpy(adapter->actor_id, actor);
    strcpy(adapter->board_id, board);
    sprintf(adapter->route, "/b/%s/native", board);
    adapter->snapshot = snapshot;
    return 1;
}

int wena_hierarchy_mutation_load(void *context, const char *board,
    WenaHierarchyKind kind, const char *target, char *title,
    size_t capacity, unsigned long *version)
{
    WenaHierarchyMutation *adapter;
    sqlite3_stmt *statement;
    sqlite3_int64 value;
    const unsigned char *stored;
    const char *sql;
    int bytes;
    int ok;
    adapter = (WenaHierarchyMutation *)context;
    if (!selected(adapter, board, kind, target) || !title || !capacity || !version)
        return 0;
    if (kind == WENA_HIERARCHY_BOARD)
        sql = "SELECT title,version FROM boards WHERE id=?1 AND id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)";
    else if (kind == WENA_HIERARCHY_LIST)
        sql = "SELECT title,version FROM lists WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)";
    else sql = "SELECT title,version FROM swimlanes WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)";
    if (sqlite3_prepare_v2(adapter->persistence.database, sql, -1,
                           &statement, NULL) != SQLITE_OK) return 0;
    ok = sqlite3_bind_text(statement, 1, target, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 2, board, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 3, adapter->actor_id, -1, SQLITE_TRANSIENT) == SQLITE_OK;
    if (ok) ok = sqlite3_step(statement) == SQLITE_ROW;
    if (ok) {
        value = sqlite3_column_int64(statement, 1);
        stored = sqlite3_column_text(statement, 0);
        bytes = sqlite3_column_bytes(statement, 0);
        ok = sqlite3_column_type(statement, 0) == SQLITE_TEXT &&
            sqlite3_column_type(statement, 1) == SQLITE_INTEGER && stored &&
            bytes > 0 && (size_t)bytes < capacity &&
            wena_model_title_valid((const char *)stored, (size_t)bytes,
                                    WENA_CARD_DETAILS_TITLE_CAPACITY) &&
            value > 0 && value <= (sqlite3_int64)WENA_VERSION_READ_MAX;
        if (ok) {
            memcpy(title, stored, (size_t)bytes);
            title[bytes] = '\0';
            *version = (unsigned long)value;
        }
    }
    sqlite3_finalize(statement);
    return ok;
}

static WenaDomainOperation operation(WenaHierarchyKind kind)
{
    if (kind == WENA_HIERARCHY_BOARD) return WENA_DOMAIN_EDIT_BOARD_TITLE;
    if (kind == WENA_HIERARCHY_LIST) return WENA_DOMAIN_EDIT_LIST_TITLE;
    return WENA_DOMAIN_EDIT_SWIMLANE_TITLE;
}

static const char *operation_name(WenaHierarchyKind kind)
{
    if (kind == WENA_HIERARCHY_BOARD) return "edit-board-title";
    if (kind == WENA_HIERARCHY_LIST) return "edit-list-title";
    return "edit-swimlane-title";
}

int wena_hierarchy_mutation_save_request(WenaHierarchyMutation *adapter,
    const char *board, WenaHierarchyKind kind, const char *target,
    unsigned long expected, unsigned long request, const char *title)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    char encoded[3 * (WENA_CARD_DETAILS_TITLE_CAPACITY - 1) + 1];
    const char hex[] = "0123456789ABCDEF";
    char *model_title;
    size_t length;
    size_t index;
    unsigned char c;
    model_title = selected(adapter, board, kind, target);
    if (!model_title || !title || !expected || expected > WENA_VERSION_MUTATE_MAX ||
        !request || request >= (unsigned long)LONG_MAX) return 0;
    for (length = 0; length < WENA_CARD_DETAILS_TITLE_CAPACITY && title[length];
         ++length) {}
    if (!wena_model_title_valid(title, length, WENA_CARD_DETAILS_TITLE_CAPACITY)) return 0;
    for (index = 0; index < length; ++index) {
        c = (unsigned char)title[index];
        encoded[index * 3] = '%';
        encoded[index * 3 + 1] = hex[c >> 4];
        encoded[index * 3 + 2] = hex[c & 15];
    }
    encoded[length * 3] = '\0';
    memset(&command, 0, sizeof(command));
    command.operation = operation(kind);
    command.request_version = request;
    strcpy(command.user_id, adapter->actor_id);
    strcpy(command.route, adapter->route);
    if (kind == WENA_HIERARCHY_BOARD)
        sprintf(command.form_body, "expectedVersion=%lu&title=%s", expected, encoded);
    else sprintf(command.form_body, "%s=%s&expectedVersion=%lu&title=%s",
        kind == WENA_HIERARCHY_LIST ? "listId" : "swimlaneId", target, expected, encoded);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&adapter->persistence, &command, &response))
        return 0;
    /* The existing loader snapshot is published only after the adapter commits. */
    memmove(model_title, title, length + 1);
    return 1;
}

static unsigned long next_request(WenaHierarchyMutation *adapter,
                                    const char *name)
{
    sqlite3_stmt *statement;
    sqlite3_int64 value;
    int ok;
    if (sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE actor_id=?1 AND route=?2 AND operation=?3",
        -1, &statement, NULL) != SQLITE_OK) return 0;
    ok = sqlite3_bind_text(statement, 1, adapter->actor_id, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 2, adapter->route, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
        sqlite3_bind_text(statement, 3, name, -1, SQLITE_STATIC) == SQLITE_OK;
    value = -1;
    if (ok && sqlite3_step(statement) == SQLITE_ROW)
        value = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    if (value < 0 || value >= LONG_MAX - 1) return 0;
    return (unsigned long)value + 1;
}

int wena_hierarchy_mutation_save(void *context, const char *board,
    WenaHierarchyKind kind, const char *target, unsigned long expected,
    const char *title)
{
    WenaHierarchyMutation *adapter;
    adapter = (WenaHierarchyMutation *)context;
    if (!selected(adapter, board, kind, target)) return 0;
    return wena_hierarchy_mutation_save_request(adapter, board, kind, target,
        expected, next_request(adapter, operation_name(kind)), title);
}

int wena_hierarchy_mutation_create_request(WenaHierarchyMutation *adapter,
    const char *board, WenaHierarchyKind kind, unsigned long request,
    const char *title)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaList list;
    WenaSwimlane lane;
    char encoded[3 * (WENA_CARD_DETAILS_TITLE_CAPACITY - 1) + 1];
    const char hex[] = "0123456789ABCDEF";
    size_t length;
    size_t index;
    unsigned char c;
    if (!selected(adapter, board, WENA_HIERARCHY_BOARD, board) || !title ||
        (kind != WENA_HIERARCHY_LIST && kind != WENA_HIERARCHY_SWIMLANE) ||
        !request || request >= (unsigned long)LONG_MAX) return 0;
    for (length = 0; length < WENA_CARD_DETAILS_TITLE_CAPACITY && title[length];
         ++length) {}
    if (!wena_model_title_valid(title, length, WENA_CARD_DETAILS_TITLE_CAPACITY)) return 0;
    if (kind == WENA_HIERARCHY_LIST) {
        if (adapter->snapshot->list_count >= WENA_SQLITE_BOARD_MAX_LISTS ||
            !wena_list_init(&list, "pending", board, "", title, 0.0, 0)) return 0;
    } else if (adapter->snapshot->swimlane_count >= WENA_SQLITE_BOARD_MAX_SWIMLANES ||
        !wena_swimlane_init(&lane, "pending", board, title, 0.0, 0)) return 0;
    for (index = 0; index < length; ++index) {
        c = (unsigned char)title[index];
        encoded[index * 3] = '%';
        encoded[index * 3 + 1] = hex[c >> 4];
        encoded[index * 3 + 2] = hex[c & 15];
    }
    encoded[length * 3] = '\0';
    memset(&command, 0, sizeof(command));
    command.operation = kind == WENA_HIERARCHY_LIST ? WENA_DOMAIN_CREATE_LIST :
        WENA_DOMAIN_CREATE_SWIMLANE;
    command.request_version = request;
    strcpy(command.user_id, adapter->actor_id);
    strcpy(command.route, adapter->route);
    sprintf(command.form_body, "title=%s", encoded);
    command.form_body_length = strlen(command.form_body);
    if (!wena_sqlite_persistence_apply(&adapter->persistence, &command, &response))
        return 0;
    if (kind == WENA_HIERARCHY_LIST) {
        strcpy(list.id, adapter->persistence.created_hierarchy_id);
        list.sort = adapter->persistence.created_hierarchy_position;
        adapter->snapshot->lists[adapter->snapshot->list_count++] = list;
    } else {
        strcpy(lane.id, adapter->persistence.created_hierarchy_id);
        lane.sort = adapter->persistence.created_hierarchy_position;
        adapter->snapshot->swimlanes[adapter->snapshot->swimlane_count++] = lane;
    }
    return 1;
}

int wena_hierarchy_mutation_create(void *context, const char *board,
    WenaHierarchyKind kind, const char *title)
{
    WenaHierarchyMutation *adapter;
    adapter = (WenaHierarchyMutation *)context;
    if (!selected(adapter, board, WENA_HIERARCHY_BOARD, board) ||
        (kind != WENA_HIERARCHY_LIST && kind != WENA_HIERARCHY_SWIMLANE)) return 0;
    return wena_hierarchy_mutation_create_request(adapter, board, kind,
        next_request(adapter, kind == WENA_HIERARCHY_LIST ? "create-list" :
            "create-swimlane"), title);
}

/* List and lane archive controls share bounded selection and version loading.
 * Publication happens only after commit; no fallible work follows the write. */
static int archive_index(WenaHierarchyMutation *adapter,const char *board,const char *id,int lanes,size_t *index)
{
    const char *item,*scope,*previous;size_t i,j,count;int archived,found;
    if(!selected(adapter,board,WENA_HIERARCHY_BOARD,board)||!wena_model_identifier_valid(id))return 0;
    count=lanes?adapter->snapshot->swimlane_count:adapter->snapshot->list_count;found=0;
    for(i=0;i<count;++i){
        item=lanes?adapter->snapshot->swimlanes[i].id:adapter->snapshot->lists[i].id;
        scope=lanes?adapter->snapshot->swimlanes[i].board_id:adapter->snapshot->lists[i].board_id;
        archived=lanes?adapter->snapshot->swimlanes[i].archived:adapter->snapshot->lists[i].archived;
        if(!wena_model_identifier_valid(item)||strcmp(scope,board)||(archived!=0&&archived!=1))return 0;
        for(j=0;j<i;++j){previous=lanes?adapter->snapshot->swimlanes[j].id:adapter->snapshot->lists[j].id;if(!strcmp(item,previous))return 0;}
        if(!strcmp(item,id)){*index=i;found=1;}
    }
    return found;
}
static WenaList *archive_selection(WenaHierarchyMutation *adapter,const char *board,const char *id)
{size_t index;return archive_index(adapter,board,id,0,&index)?&adapter->snapshot->lists[index]:NULL;}
static int archive_load(void *context,const char *board,const char *id,unsigned long *version,int lanes)
{
    WenaHierarchyMutation *adapter;sqlite3_stmt *statement;sqlite3_int64 stored,at;
    int ok,archived,model_archived;size_t index;const char *query;
    adapter=(WenaHierarchyMutation*)context;
    if(!version||!archive_index(adapter,board,id,lanes,&index))return 0;
    model_archived=lanes?adapter->snapshot->swimlanes[index].archived:adapter->snapshot->lists[index].archived;
    query=lanes?"SELECT version FROM swimlanes WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)":
        "SELECT version FROM lists WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)";
    if(sqlite3_prepare_v2(adapter->persistence.database,query,-1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(statement,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,3,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(statement)==SQLITE_ROW&&sqlite3_column_type(statement,0)==SQLITE_INTEGER;
    stored=0;
    if(ok){stored=sqlite3_column_int64(statement,0);ok=stored>0&&stored<=(sqlite3_int64)WENA_VERSION_READ_MAX&&sqlite3_step(statement)==SQLITE_DONE;}
    if(sqlite3_finalize(statement)!=SQLITE_OK)ok=0;
    if(!ok||!(lanes?wena_sqlite_swimlane_state_read(adapter->persistence.database,board,id,(unsigned long)stored,&archived,&at):
        wena_sqlite_list_state_read(adapter->persistence.database,board,id,(unsigned long)stored,&archived,&at))||archived!=model_archived)return 0;
    *version=(unsigned long)stored;return 1;
}
int wena_hierarchy_mutation_archive_load(void *context,const char *board,const char *id,unsigned long *version)
{return archive_load(context,board,id,version,0);}
int wena_hierarchy_mutation_swimlane_archive_load(void *context,const char *board,const char *id,unsigned long *version)
{return archive_load(context,board,id,version,1);}

int wena_hierarchy_mutation_archive_request(WenaHierarchyMutation *adapter,
    const char *board,const char *id,unsigned long expected,unsigned long request,int archived)
{
    WenaList *list;WenaDomainCommand command;WenaRegionResponse response;
    list=archive_selection(adapter,board,id);
    if(!list||!expected||expected>WENA_VERSION_MUTATE_MAX||!request||
        request>=(unsigned long)LONG_MAX||(archived!=0&&archived!=1))return 0;
    memset(&command,0,sizeof(command));
    command.operation=archived?WENA_DOMAIN_ARCHIVE_LIST:WENA_DOMAIN_RESTORE_LIST;
    command.request_version=request;strcpy(command.user_id,adapter->actor_id);
    strcpy(command.route,adapter->route);
    sprintf(command.form_body,"listId=%s&expectedVersion=%lu",id,expected);
    command.form_body_length=strlen(command.form_body);
    if(!wena_sqlite_persistence_apply(&adapter->persistence,&command,&response))return 0;
    list->archived=archived;return 1;
}

int wena_hierarchy_mutation_archive(void *context,const char *board,
    const char *id,unsigned long expected)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!archive_selection(adapter,board,id))return 0;
    return wena_hierarchy_mutation_archive_request(adapter,board,id,expected,
        next_request(adapter,"archive-list"),1);
}

int wena_hierarchy_mutation_restore(void *context,const char *board,
    const char *id,unsigned long expected)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!archive_selection(adapter,board,id))return 0;
    return wena_hierarchy_mutation_archive_request(adapter,board,id,expected,
        next_request(adapter,"restore-list"),0);
}

static char *color_selection(WenaHierarchyMutation *adapter,const char *board,
    WenaHierarchyKind kind,const char *id)
{
    const char *item,*scope,*previous;char *color,*found;size_t count,i,j;int archived;
    if((kind!=WENA_HIERARCHY_LIST&&kind!=WENA_HIERARCHY_SWIMLANE)||
        !selected(adapter,board,WENA_HIERARCHY_BOARD,board)||!wena_model_identifier_valid(id))return NULL;
    count=kind==WENA_HIERARCHY_LIST?adapter->snapshot->list_count:adapter->snapshot->swimlane_count;found=NULL;
    for(i=0;i<count;++i){
        if(kind==WENA_HIERARCHY_LIST){item=adapter->snapshot->lists[i].id;scope=adapter->snapshot->lists[i].board_id;
            archived=adapter->snapshot->lists[i].archived;color=adapter->snapshot->lists[i].color;}
        else{item=adapter->snapshot->swimlanes[i].id;scope=adapter->snapshot->swimlanes[i].board_id;
            archived=adapter->snapshot->swimlanes[i].archived;color=adapter->snapshot->swimlanes[i].color;}
        if(!wena_model_identifier_valid(item)||strcmp(scope,board)||(archived!=0&&archived!=1))return NULL;
        for(j=0;j<i;++j){previous=kind==WENA_HIERARCHY_LIST?adapter->snapshot->lists[j].id:adapter->snapshot->swimlanes[j].id;
            if(!strcmp(item,previous))return NULL;}
        if(!strcmp(item,id)&&!archived)found=color;
    }
    return found;
}

int wena_hierarchy_mutation_color_load(void *context,const char *board,
    WenaHierarchyKind kind,const char *id,char *color,size_t capacity,unsigned long *version)
{
    WenaHierarchyMutation *adapter;char title[WENA_TITLE_CAPACITY],stored[WENA_COLOR_CAPACITY];unsigned long current;
    adapter=(WenaHierarchyMutation*)context;
    if(!color||!version||!capacity||!color_selection(adapter,board,kind,id)||
        !wena_hierarchy_mutation_load(adapter,board,kind,id,title,sizeof(title),&current)||
        !wena_sqlite_hierarchy_color_read(adapter->persistence.database,board,id,kind==WENA_HIERARCHY_LIST,current,stored)||
        !(kind==WENA_HIERARCHY_LIST?wena_sqlite_list_active(adapter->persistence.database,board,id):wena_sqlite_swimlane_active(adapter->persistence.database,board,id))||
        strlen(stored)>=capacity)return 0;
    strcpy(color,stored);*version=current;return 1;
}

int wena_hierarchy_mutation_color_save_request(WenaHierarchyMutation *adapter,
    const char *board,WenaHierarchyKind kind,const char *id,unsigned long expected,
    unsigned long request,const char *color)
{
    char *model,encoded[3*(WENA_COLOR_CAPACITY-1)+1],published[WENA_COLOR_CAPACITY];size_t length,i;unsigned char c;
    const char hex[]="0123456789ABCDEF";WenaDomainCommand command;WenaRegionResponse response;
    model=color_selection(adapter,board,kind,id);
    if(!model||!color||!expected||expected>WENA_VERSION_MUTATE_MAX||!request||request>=(unsigned long)LONG_MAX)return 0;
    for(length=0;length<WENA_COLOR_CAPACITY&&color[length];++length){}
    if(length==WENA_COLOR_CAPACITY||!wena_color_valid(color))return 0;
    for(i=0;i<length;++i){c=(unsigned char)color[i];encoded[i*3]='%';encoded[i*3+1]=hex[c>>4];encoded[i*3+2]=hex[c&15];}
    encoded[length*3]=0;memset(published,0,sizeof(published));memcpy(published,color,length);
    memset(&command,0,sizeof(command));
    command.operation=kind==WENA_HIERARCHY_LIST?WENA_DOMAIN_SET_LIST_COLOR:WENA_DOMAIN_SET_SWIMLANE_COLOR;
    command.request_version=request;strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"%s=%s&expectedVersion=%lu&color=%s",kind==WENA_HIERARCHY_LIST?"listId":"swimlaneId",id,expected,encoded);
    command.form_body_length=strlen(command.form_body);
    if(!wena_sqlite_persistence_apply(&adapter->persistence,&command,&response))return 0;
    memcpy(model,published,sizeof(published));return 1;
}

int wena_hierarchy_mutation_color_save(void *context,const char *board,
    WenaHierarchyKind kind,const char *id,unsigned long expected,const char *color)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!color_selection(adapter,board,kind,id))return 0;
    return wena_hierarchy_mutation_color_save_request(adapter,board,kind,id,expected,
        next_request(adapter,kind==WENA_HIERARCHY_LIST?"set-list-color":"set-swimlane-color"),color);
}

int wena_hierarchy_mutation_wip_load(void *context,const char *board,const char *id,
    WenaWipLimit *limit,size_t *count,unsigned long *version)
{
    WenaHierarchyMutation *adapter;WenaList *list;WenaWipLimit stored;size_t total;
    unsigned long current;sqlite3 *db;int ok;
    adapter=(WenaHierarchyMutation*)context;list=archive_selection(adapter,board,id);
    if(!list||list->archived||!limit||!count||!version)return 0;
    db=adapter->persistence.database;
    if(!sqlite3_get_autocommit(db)||sqlite3_exec(db,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK)return 0;
    ok=wena_hierarchy_mutation_archive_load(adapter,board,id,&current)&&
        wena_sqlite_list_wip_read(db,board,id,current,&stored)&&wena_sqlite_list_wip_count(db,board,id,&total);
    if(ok)ok=sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if(!ok){(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);return 0;}
    *limit=stored;*count=total;*version=current;return 1;
}
int wena_hierarchy_mutation_wip_save_request(WenaHierarchyMutation *adapter,
    const char *board,const char *id,unsigned long expected,unsigned long request,
    WenaWipEdit edit,size_t value)
{
    WenaList *list;WenaDomainCommand command;WenaRegionResponse response;const char *action;
    list=archive_selection(adapter,board,id);
    if(!list||list->archived||!expected||expected>WENA_VERSION_MUTATE_MAX||!request||request>=(unsigned long)LONG_MAX)return 0;
    if(edit==WENA_WIP_APPLY_VALUE){if(value<1||value>99)return 0;action="value";}
    else if(edit==WENA_WIP_TOGGLE_ENABLED)action="enabled";
    else if(edit==WENA_WIP_TOGGLE_SOFT)action="soft";
    else return 0;
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_EDIT_LIST_WIP;
    command.request_version=request;strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"listId=%s&expectedVersion=%lu&action=%s",id,expected,action);
    if(edit==WENA_WIP_APPLY_VALUE)sprintf(command.form_body+strlen(command.form_body),"&value=%lu",(unsigned long)value);
    command.form_body_length=strlen(command.form_body);
    if(!wena_sqlite_persistence_apply(&adapter->persistence,&command,&response))return 0;
    /* Exact transaction result: toggles may adjust to a count newer than cache. */
    list->wip_limit.value=adapter->persistence.list_wip_result.value;
    list->wip_limit.enabled=adapter->persistence.list_wip_result.enabled;
    list->wip_limit.soft=adapter->persistence.list_wip_result.soft;return 1;
}
int wena_hierarchy_mutation_wip_save(void *context,const char *board,const char *id,
    unsigned long expected,WenaWipEdit edit,size_t value)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!archive_selection(adapter,board,id))return 0;
    return wena_hierarchy_mutation_wip_save_request(adapter,board,id,expected,
        next_request(adapter,"edit-list-wip"),edit,value);
}

typedef struct BoardPublish {
    const char *board;
    WenaSqliteBoardSnapshot *snapshot;
    const char *target;
    WenaSqliteBoardSnapshot *destination;
} BoardPublish;
static int prepare_board_publish(void *context,sqlite3 *db)
{
    BoardPublish *publish;publish=(BoardPublish*)context;
    return wena_sqlite_board_read_transaction(db,publish->board,publish->snapshot)&&
        (!publish->target||wena_sqlite_board_read_transaction(db,publish->target,publish->destination));
}
static int publish_boards(WenaHierarchyMutation *adapter,const char *board,
    const WenaDomainCommand *command,const char *target,WenaSqliteBoardSnapshot *destination,size_t *target_count)
{
    WenaRegionResponse response;BoardPublish publish;int ok;
    if(adapter->persistence.prepare_publish)return 0;
    publish.snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*publish.snapshot));if(!publish.snapshot)return 0;
    publish.board=board;publish.target=target;publish.destination=NULL;
    if(target){publish.destination=(WenaSqliteBoardSnapshot*)malloc(sizeof(*publish.destination));
        if(!publish.destination){free(publish.snapshot);return 0;}}
    adapter->persistence.prepare_publish=prepare_board_publish;adapter->persistence.publish_context=&publish;
    ok=wena_sqlite_persistence_apply(&adapter->persistence,command,&response);
    adapter->persistence.prepare_publish=NULL;adapter->persistence.publish_context=NULL;
    if(ok){
        memcpy(adapter->snapshot,publish.snapshot,sizeof(*publish.snapshot));
        if(adapter->published_card_count)*adapter->published_card_count=publish.snapshot->card_count;
        if(target){memcpy(destination,publish.destination,sizeof(*destination));
            if(target_count)*target_count=publish.destination->card_count;}
    }
    free(publish.snapshot);free(publish.destination);return ok;
}
static int publish_board(WenaHierarchyMutation *adapter,const char *board,const WenaDomainCommand *command)
{return publish_boards(adapter,board,command,NULL,NULL,NULL);}
int wena_hierarchy_mutation_swimlane_archive_request(WenaHierarchyMutation *adapter,
    const char *board,const char *id,unsigned long expected,unsigned long request,int archived)
{
    WenaDomainCommand command;size_t index;
    if(!archive_index(adapter,board,id,1,&index)||!expected||expected>WENA_VERSION_MUTATE_MAX||!request||
        request>=(unsigned long)LONG_MAX||(archived!=0&&archived!=1))return 0;
    memset(&command,0,sizeof(command));command.operation=archived?WENA_DOMAIN_ARCHIVE_SWIMLANE:WENA_DOMAIN_RESTORE_SWIMLANE;
    command.request_version=request;strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"swimlaneId=%s&expectedVersion=%lu",id,expected);command.form_body_length=strlen(command.form_body);
    return publish_board(adapter,board,&command);
}
int wena_hierarchy_mutation_swimlane_archive(void *context,const char *board,const char *id,unsigned long expected)
{
    WenaHierarchyMutation *adapter;size_t index;adapter=(WenaHierarchyMutation*)context;
    if(!archive_index(adapter,board,id,1,&index))return 0;
    return wena_hierarchy_mutation_swimlane_archive_request(adapter,board,id,expected,next_request(adapter,"archive-swimlane"),1);
}
int wena_hierarchy_mutation_swimlane_restore(void *context,const char *board,const char *id,unsigned long expected)
{
    WenaHierarchyMutation *adapter;size_t index;adapter=(WenaHierarchyMutation*)context;
    if(!archive_index(adapter,board,id,1,&index))return 0;
    return wena_hierarchy_mutation_swimlane_archive_request(adapter,board,id,expected,next_request(adapter,"restore-swimlane"),0);
}


int wena_hierarchy_mutation_list_cards_load(void *context,const char *board,
    const char *list,const char *lane,unsigned long *list_version,unsigned long *lane_version)
{
    WenaHierarchyMutation *adapter;unsigned long l,s;int ok;sqlite3 *db;
    adapter=(WenaHierarchyMutation*)context;
    if(!list_version||!lane_version||!selected(adapter,board,WENA_HIERARCHY_LIST,list)||
        (lane&&lane[0]&&!selected(adapter,board,WENA_HIERARCHY_SWIMLANE,lane)))return 0;
    db=adapter->persistence.database;l=s=0;
    if(sqlite3_exec(db,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK)return 0;
    ok=archive_load(adapter,board,list,&l,0);
    if(ok&&lane&&lane[0])ok=archive_load(adapter,board,lane,&s,1);
    if(ok)ok=sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if(!ok){(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);return 0;}
    *list_version=l;*lane_version=s;return 1;
}
int wena_hierarchy_mutation_list_cards_archive_request(WenaHierarchyMutation *adapter,
    const char *board,const char *list,const char *lane,unsigned long expected,
    unsigned long lane_version,unsigned long request)
{
    WenaDomainCommand command;WenaList *chosen;size_t index;int scoped;scoped=lane&&lane[0];
    chosen=archive_selection(adapter,board,list);
    if(!chosen||chosen->archived||!expected||expected>WENA_VERSION_MUTATE_MAX||
        !request||request>=(unsigned long)LONG_MAX||
        (scoped?(!lane_version||lane_version>WENA_VERSION_MUTATE_MAX||
            (!archive_index(adapter,board,lane,1,&index)||adapter->snapshot->swimlanes[index].archived)):lane_version!=0))return 0;
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_ARCHIVE_LIST_CARDS;
    command.request_version=request;strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    sprintf(command.form_body,"listId=%s&expectedVersion=%lu",list,expected);
    if(scoped)sprintf(command.form_body+strlen(command.form_body),"&swimlaneId=%s&expectedSwimlaneVersion=%lu",lane,lane_version);
    command.form_body_length=strlen(command.form_body);
    return publish_board(adapter,board,&command);
}
int wena_hierarchy_mutation_list_cards_archive(void *context,const char *board,
    const char *list,const char *lane,unsigned long expected,unsigned long lane_version)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!selected(adapter,board,WENA_HIERARCHY_LIST,list))return 0;
    return wena_hierarchy_mutation_list_cards_archive_request(adapter,board,list,lane,
        expected,lane_version,next_request(adapter,"archive-list-cards"));
}


static int selected_ids_valid(WenaHierarchyMutation *adapter,const char *board,const WenaId *ids,size_t count)
{
    size_t i,j;
    if(!ids||!count||count>WENA_DOMAIN_CARD_BATCH_CAPACITY||!selected(adapter,board,WENA_HIERARCHY_BOARD,board))return 0;
    for(i=0;i<count;++i){
        if(!wena_model_identifier_valid(ids[i]))return 0;
        for(j=0;j<i;++j)if(!strcmp(ids[i],ids[j]))return 0;
    }
    return 1;
}
static int selected_read_locked(WenaHierarchyMutation *adapter,const char *board,
    const WenaId *ids,size_t count,WenaCardRevision *rows)
{
    sqlite3_stmt *statement;sqlite3 *db;size_t i;int ok;db=adapter->persistence.database;
    ok=sqlite3_prepare_v2(db,"SELECT 1 FROM actors WHERE id=?1",-1,&statement,NULL)==SQLITE_OK;
    if(ok){
        ok=sqlite3_bind_text(statement,1,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
            sqlite3_step(statement)==SQLITE_ROW&&sqlite3_step(statement)==SQLITE_DONE;
        if(sqlite3_finalize(statement)!=SQLITE_OK)ok=0;
    }
    for(i=0;ok&&i<count;++i){
        strcpy(rows[i].id,ids[i]);ok=wena_sqlite_card_archive_version(db,board,ids[i],&rows[i].version);
    }
    return ok;
}
int wena_hierarchy_mutation_selected_cards_load(void *context,const char *board,
    const WenaId *ids,size_t count,WenaDomainCardRevision **output)
{
    WenaHierarchyMutation *adapter;WenaDomainCardRevision *rows;sqlite3 *db;int ok;
    adapter=(WenaHierarchyMutation*)context;
    if(!output||!selected_ids_valid(adapter,board,ids,count))return 0;
    rows=(WenaDomainCardRevision*)calloc(count,sizeof(*rows));if(!rows)return 0;
    db=adapter->persistence.database;
    if(sqlite3_exec(db,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK){free(rows);return 0;}
    ok=selected_read_locked(adapter,board,ids,count,rows)&&sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if(!ok){(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);free(rows);return 0;}
    free(*output);*output=rows;return 1;
}
int wena_hierarchy_mutation_selected_cards_archive_request(WenaHierarchyMutation *adapter,
    const char *board,const WenaDomainCardRevision *cards,size_t count,unsigned long request)
{
    WenaDomainCommand command;
    if(!selected(adapter,board,WENA_HIERARCHY_BOARD,board)||!cards||!count||
        count>WENA_DOMAIN_CARD_BATCH_CAPACITY||!request||request>=(unsigned long)LONG_MAX)return 0;
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_ARCHIVE_SELECTED_CARDS;
    command.request_version=request;strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    command.selected_cards=cards;command.selected_card_count=count;
    return publish_board(adapter,board,&command);
}
int wena_hierarchy_mutation_selected_cards_archive(void *context,const char *board,
    const WenaDomainCardRevision *cards,size_t count)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!selected(adapter,board,WENA_HIERARCHY_BOARD,board))return 0;
    return wena_hierarchy_mutation_selected_cards_archive_request(adapter,board,cards,count,
        next_request(adapter,"archive-selected-cards"));
}

int wena_hierarchy_mutation_selected_move_load(void *context,const char *board,
    const WenaId *ids,size_t count,WenaCardMoveSelection **output)
{
    WenaHierarchyMutation *adapter;WenaCardMoveSelection *candidate;WenaSqliteBoardSnapshot *snapshot;
    sqlite3 *db;size_t i;int ok;
    adapter=(WenaHierarchyMutation*)context;
    if(!output||!selected_ids_valid(adapter,board,ids,count))return 0;
    candidate=(WenaCardMoveSelection*)calloc(1,sizeof(*candidate));if(!candidate)return 0;
    snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));
    if(!snapshot){free(candidate);return 0;}
    db=adapter->persistence.database;
    if(sqlite3_exec(db,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK){free(snapshot);free(candidate);return 0;}
    ok=selected_read_locked(adapter,board,ids,count,candidate->cards);
    for(i=0;ok&&i<count;++i)ok=wena_sqlite_card_parents_active(db,board,ids[i]);
    if(ok)ok=wena_sqlite_card_board_order(db,board,candidate->fingerprint)&&
        wena_sqlite_board_read_transaction(db,board,snapshot)&&sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if(!ok){(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);free(snapshot);free(candidate);return 0;}
    strcpy(candidate->board_id,board);candidate->count=count;
    memcpy(adapter->snapshot,snapshot,sizeof(*snapshot));
    if(adapter->published_card_count)*adapter->published_card_count=snapshot->card_count;
    free(snapshot);free(*output);*output=candidate;return 1;
}
static int move_fingerprint_valid(const char value[65])
{
    size_t i;for(i=0;i<64;++i)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return 0;
    return value[64]==0;
}
int wena_hierarchy_mutation_selected_move_request(WenaHierarchyMutation *adapter,
    const WenaCardMoveSelection *selection,const char *list,const char *lane,size_t before,unsigned long request)
{
    WenaDomainCommand command;
    if(!selection||!selected(adapter,selection->board_id,WENA_HIERARCHY_BOARD,selection->board_id)||
        !selection->count||selection->count>WENA_CARD_ORDER_CAPACITY||before>WENA_CARD_ORDER_CAPACITY||
        !wena_model_identifier_valid(list)||!wena_model_identifier_valid(lane)||!request||request>=(unsigned long)LONG_MAX)return 0;
    if(!move_fingerprint_valid(selection->fingerprint))return 0;
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_MOVE_SELECTED_CARDS;command.request_version=request;
    strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    command.selected_cards=selection->cards;command.selected_card_count=selection->count;
    sprintf(command.form_body,"targetListId=%s&targetSwimlaneId=%s&insertPosition=%lu&expectedBoardOrder=%s",list,lane,(unsigned long)before,selection->fingerprint);
    command.form_body_length=strlen(command.form_body);
    return publish_board(adapter,selection->board_id,&command);
}
int wena_hierarchy_mutation_selected_move(void *context,const WenaCardMoveSelection *selection,
    const char *list,const char *lane,size_t before)
{
    WenaHierarchyMutation *adapter;adapter=(WenaHierarchyMutation*)context;
    if(!selection||!selected(adapter,selection->board_id,WENA_HIERARCHY_BOARD,selection->board_id))return 0;
    return wena_hierarchy_mutation_selected_move_request(adapter,selection,list,lane,before,next_request(adapter,"move-selected-cards"));
}

static int transfer_context_valid(const WenaHierarchyTransfer *transfer)
{
    return transfer&&transfer->source&&transfer->source->snapshot&&transfer->destination&&
        transfer->destination!=transfer->source->snapshot&&
        (!transfer->published_card_count||transfer->published_card_count!=transfer->source->published_card_count);
}
int wena_hierarchy_transfer_init(WenaHierarchyTransfer *transfer,WenaHierarchyMutation *source,
    WenaSqliteBoardSnapshot *destination)
{
    WenaHierarchyTransfer candidate;memset(&candidate,0,sizeof(candidate));candidate.source=source;candidate.destination=destination;
    if(!transfer||!transfer_context_valid(&candidate))return 0;
    *transfer=candidate;return 1;
}
int wena_hierarchy_transfer_view(void *context,WenaBoardLayout *layout)
{
    WenaHierarchyTransfer *transfer;WenaSqliteBoardSnapshot *snapshot;WenaBoardLayout candidate;
    transfer=(WenaHierarchyTransfer*)context;
    if(!layout||!transfer_context_valid(transfer))return 0;
    snapshot=transfer->destination;
    if(!wena_model_identifier_valid(snapshot->board.id)||snapshot->board.archived||
        snapshot->card_count>WENA_SQLITE_BOARD_MAX_CARDS||snapshot->list_count>WENA_SQLITE_BOARD_MAX_LISTS||
        snapshot->swimlane_count>WENA_SQLITE_BOARD_MAX_SWIMLANES)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.board=&snapshot->board;
    candidate.cards=snapshot->cards;candidate.card_count=snapshot->card_count;
    candidate.lists=snapshot->lists;candidate.list_count=snapshot->list_count;
    candidate.swimlanes=snapshot->swimlanes;candidate.swimlane_count=snapshot->swimlane_count;
    *layout=candidate;return 1;
}
static int transfer_version(sqlite3 *db,const char *board,unsigned long *output)
{
    sqlite3_stmt *s;sqlite3_int64 version;int ok;
    if(sqlite3_prepare_v2(db,"SELECT version FROM boards WHERE id=?1",-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_type(s,0)==SQLITE_INTEGER;
    version=ok?sqlite3_column_int64(s,0):0;ok=ok&&version>0&&version<=(sqlite3_int64)WENA_VERSION_MUTATE_MAX&&sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)*output=(unsigned long)version;return ok;
}
int wena_hierarchy_transfer_load(void *context,const char *board,const WenaId *ids,size_t count,
    const char *target,WenaCardTransferSelection **output)
{
    WenaHierarchyTransfer *transfer;WenaHierarchyMutation *adapter;WenaCardTransferSelection *candidate;
    BoardPublish views;sqlite3 *db;size_t i;int ok;transfer=(WenaHierarchyTransfer*)context;
    if(!output||!transfer_context_valid(transfer)||!wena_model_identifier_valid(target))return 0;
    adapter=transfer->source;
    if(!selected_ids_valid(adapter,board,ids,count)||!strcmp(board,target))return 0;
    candidate=(WenaCardTransferSelection*)calloc(1,sizeof(*candidate));
    views.snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*views.snapshot));views.destination=(WenaSqliteBoardSnapshot*)malloc(sizeof(*views.destination));
    ok=0;if(!candidate||!views.snapshot||!views.destination)goto done;
    views.board=board;views.target=target;db=adapter->persistence.database;
    if(sqlite3_exec(db,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK)goto done;
    ok=selected_read_locked(adapter,board,ids,count,candidate->source.cards);
    for(i=0;ok&&i<count;++i)ok=wena_sqlite_card_parents_active(db,board,ids[i]);
    if(ok)ok=transfer_version(db,board,&candidate->source_board_version)&&transfer_version(db,target,&candidate->target_board_version)&&
        wena_sqlite_card_board_order(db,board,candidate->source.fingerprint)&&wena_sqlite_card_board_order(db,target,candidate->target_fingerprint)&&
        prepare_board_publish(&views,db)&&sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;
    if(!ok){(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);goto done;}
    strcpy(candidate->source.board_id,board);candidate->source.count=count;strcpy(candidate->target_board_id,target);
    memcpy(adapter->snapshot,views.snapshot,sizeof(*views.snapshot));memcpy(transfer->destination,views.destination,sizeof(*views.destination));
    if(adapter->published_card_count)*adapter->published_card_count=views.snapshot->card_count;
    if(transfer->published_card_count)*transfer->published_card_count=views.destination->card_count;
    free(*output);*output=candidate;candidate=NULL;
 done:
    free(candidate);free(views.snapshot);free(views.destination);return ok;
}
int wena_hierarchy_transfer_request(WenaHierarchyTransfer *transfer,const WenaCardTransferSelection *selection,
    const char *list,const char *lane,size_t before,unsigned long request)
{
    WenaHierarchyMutation *adapter;WenaDomainCommand command;
    if(!selection||!transfer_context_valid(transfer)||!wena_model_identifier_valid(selection->target_board_id))return 0;
    adapter=transfer->source;
    if(!selected(adapter,selection->source.board_id,WENA_HIERARCHY_BOARD,selection->source.board_id)||
        !strcmp(selection->source.board_id,selection->target_board_id)||
        strcmp(transfer->destination->board.id,selection->target_board_id)||transfer->destination->board.archived||
        !selection->source.count||selection->source.count>WENA_CARD_ORDER_CAPACITY||
        !selection->source_board_version||selection->source_board_version>WENA_VERSION_MUTATE_MAX||
        !selection->target_board_version||selection->target_board_version>WENA_VERSION_MUTATE_MAX||
        !move_fingerprint_valid(selection->source.fingerprint)||!move_fingerprint_valid(selection->target_fingerprint)||
        !wena_model_identifier_valid(list)||!wena_model_identifier_valid(lane)||before>WENA_CARD_ORDER_CAPACITY||
        !request||request>=(unsigned long)LONG_MAX)return 0;
    memset(&command,0,sizeof(command));command.operation=WENA_DOMAIN_TRANSFER_SELECTED_CARDS;command.request_version=request;
    strcpy(command.user_id,adapter->actor_id);strcpy(command.route,adapter->route);
    command.selected_cards=selection->source.cards;command.selected_card_count=selection->source.count;
    sprintf(command.form_body,"targetBoardId=%s&targetListId=%s&targetSwimlaneId=%s&insertPosition=%lu&expectedBoardVersion=%lu&expectedTargetBoardVersion=%lu&expectedBoardOrder=%s&expectedTargetBoardOrder=%s",
        selection->target_board_id,list,lane,(unsigned long)before,selection->source_board_version,selection->target_board_version,
        selection->source.fingerprint,selection->target_fingerprint);command.form_body_length=strlen(command.form_body);
    return publish_boards(adapter,selection->source.board_id,&command,selection->target_board_id,transfer->destination,transfer->published_card_count);
}
int wena_hierarchy_transfer_save(void *context,const WenaCardTransferSelection *selection,
    const char *list,const char *lane,size_t before)
{
    WenaHierarchyTransfer *transfer;transfer=(WenaHierarchyTransfer*)context;
    if(!selection||!transfer_context_valid(transfer)||
        !selected(transfer->source,selection->source.board_id,WENA_HIERARCHY_BOARD,selection->source.board_id))return 0;
    return wena_hierarchy_transfer_request(transfer,selection,list,lane,before,next_request(transfer->source,"transfer-selected-cards"));
}
