#include "../../server/mutations/hierarchy_colors.h"
#include "../../server/list_state.h"
#include "hierarchy_mutation.h"

#include <limits.h>
#include <stdio.h>
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

/* Archive controls retain the complete loader snapshot. Only the selected flag
 * is published after commit; no allocation or reload can fail after the write. */
static WenaList *archive_selection(WenaHierarchyMutation *adapter,
    const char *board,const char *id)
{
    WenaList *found,*list;size_t i,j;
    if(!selected(adapter,board,WENA_HIERARCHY_BOARD,board)||
        !wena_model_identifier_valid(id))return NULL;
    found=NULL;
    for(i=0;i<adapter->snapshot->list_count;++i){
        list=&adapter->snapshot->lists[i];
        if(!wena_model_identifier_valid(list->id)||strcmp(list->board_id,board)||
            (list->archived!=0&&list->archived!=1))return NULL;
        for(j=0;j<i;++j)if(!strcmp(adapter->snapshot->lists[j].id,list->id))return NULL;
        if(!strcmp(list->id,id))found=list;
    }
    return found;
}

int wena_hierarchy_mutation_archive_load(void *context,const char *board,
    const char *id,unsigned long *version)
{
    WenaHierarchyMutation *adapter;WenaList *list;sqlite3_stmt *statement;
    sqlite3_int64 stored,at;int ok,archived;
    adapter=(WenaHierarchyMutation*)context;
    list=archive_selection(adapter,board,id);if(!list||!version)return 0;
    if(sqlite3_prepare_v2(adapter->persistence.database,
        "SELECT version FROM lists WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)",
        -1,&statement,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(statement,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(statement,3,adapter->actor_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_step(statement)==SQLITE_ROW&&sqlite3_column_type(statement,0)==SQLITE_INTEGER;
    stored=0;
    if(ok){stored=sqlite3_column_int64(statement,0);
        ok=stored>0&&stored<=(sqlite3_int64)WENA_VERSION_READ_MAX&&sqlite3_step(statement)==SQLITE_DONE;}
    if(sqlite3_finalize(statement)!=SQLITE_OK)ok=0;
    if(!ok||!wena_sqlite_list_state_read(adapter->persistence.database,board,id,
        (unsigned long)stored,&archived,&at)||archived!=list->archived)return 0;
    *version=(unsigned long)stored;return 1;
}

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
        (kind==WENA_HIERARCHY_LIST&&!wena_sqlite_list_active(adapter->persistence.database,board,id))||
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
