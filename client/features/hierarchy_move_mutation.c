#include "hierarchy_move_mutation.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int selection(WenaHierarchyMoveMutation *a, const char *board,
    WenaHierarchyKind kind, const char *id, size_t *selected, char order[65])
{
    WenaSqliteBoardSnapshot *s;
    WenaSha256 hash;
    const char *item, *parent, *previous;
    double position;
    size_t count, i, j;
    int archived, found;
    if (!a || !a->persistence.database || !a->snapshot ||
        !wena_model_identifier_valid(board) || !wena_model_identifier_valid(id) ||
        strcmp(board,a->board_id) ||
        (kind!=WENA_HIERARCHY_LIST && kind!=WENA_HIERARCHY_SWIMLANE)) return 0;
    s=a->snapshot;
    if (!wena_model_identifier_valid(s->board.id) || strcmp(s->board.id,board) ||
        s->board.archived || s->list_count>WENA_SQLITE_BOARD_MAX_LISTS ||
        s->swimlane_count>WENA_SQLITE_BOARD_MAX_SWIMLANES) return 0;
    count=kind==WENA_HIERARCHY_LIST?s->list_count:s->swimlane_count;
    wena_sha256_init(&hash); found=0;
    for(i=0;i<count;++i){
        if(kind==WENA_HIERARCHY_LIST){item=s->lists[i].id;parent=s->lists[i].board_id;
            position=s->lists[i].sort;archived=s->lists[i].archived;}
        else{item=s->swimlanes[i].id;parent=s->swimlanes[i].board_id;
            position=s->swimlanes[i].sort;archived=s->swimlanes[i].archived;}
        if(!wena_model_identifier_valid(item)||!wena_model_identifier_valid(parent)||
            strcmp(parent,board)||archived||position!=(double)i)return 0;
        for(j=0;j<i;++j){previous=kind==WENA_HIERARCHY_LIST?s->lists[j].id:s->swimlanes[j].id;
            if(!strcmp(previous,item))return 0;}
        if(!wena_sqlite_hierarchy_order_add(&hash,item,strlen(item)))return 0;
        if(!strcmp(item,id)){*selected=i;found=1;}
    }
    if(!found)return 0;
    wena_sha256_final_hex(&hash,order);
    return 1;
}

int wena_hierarchy_move_mutation_init(WenaHierarchyMoveMutation *a,
    sqlite3 *db,const char *actor,const char *board,WenaSqliteBoardSnapshot *s)
{
    if(!a)return 0;
    memset(a,0,sizeof(*a));
    if(!db||!s||!wena_model_identifier_valid(actor)||!wena_model_identifier_valid(board)||
        !wena_model_identifier_valid(s->board.id)||strcmp(s->board.id,board)||s->board.archived||
        s->list_count>WENA_SQLITE_BOARD_MAX_LISTS||s->swimlane_count>WENA_SQLITE_BOARD_MAX_SWIMLANES)return 0;
    wena_sqlite_persistence_init(&a->persistence,db);
    strcpy(a->actor_id,actor);strcpy(a->board_id,board);sprintf(a->route,"/b/%s/native",board);
    a->snapshot=s;return 1;
}

int wena_hierarchy_move_mutation_load(void *context,const char *board,
    WenaHierarchyKind kind,const char *id,unsigned long *version,unsigned long *position)
{
    WenaHierarchyMoveMutation *a;
    sqlite3_stmt *statement;
    sqlite3_int64 value, stored;
    char order[65];
    size_t selected;
    int ok;
    a=(WenaHierarchyMoveMutation*)context;
    if(!version||!position||!selection(a,board,kind,id,&selected,order))return 0;
    if(sqlite3_prepare_v2(a->persistence.database,kind==WENA_HIERARCHY_LIST?
        "SELECT version,position FROM lists WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)":
        "SELECT version,position FROM swimlanes WHERE id=?1 AND board_id=?2 AND EXISTS(SELECT 1 FROM actors WHERE id=?3)",
        -1,&statement,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_text(statement,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(statement,2,board,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(statement,3,a->actor_id,-1,SQLITE_TRANSIENT);
    ok=sqlite3_step(statement)==SQLITE_ROW&&sqlite3_column_type(statement,0)==SQLITE_INTEGER&&
        sqlite3_column_type(statement,1)==SQLITE_INTEGER;
    if(ok){value=sqlite3_column_int64(statement,0);stored=sqlite3_column_int64(statement,1);
        ok=value>0&&value<LONG_MAX&&stored==(sqlite3_int64)selected;
        if(ok){*version=(unsigned long)value;*position=(unsigned long)stored;}}
    sqlite3_finalize(statement);return ok;
}

int wena_hierarchy_move_mutation_move_request(WenaHierarchyMoveMutation *a,
    const char *board,WenaHierarchyKind kind,const char *id,unsigned long expected,
    unsigned long request,unsigned long target)
{
    WenaDomainCommand command;
    WenaRegionResponse response;
    WenaSqliteBoardSnapshot *s;
    WenaList list;
    WenaSwimlane lane;
    char order[65];
    size_t selected,count,index;
    if(!selection(a,board,kind,id,&selected,order)||!expected||expected>=(unsigned long)LONG_MAX||
        !request||request>=(unsigned long)LONG_MAX)return 0;
    s=a->snapshot;count=kind==WENA_HIERARCHY_LIST?s->list_count:s->swimlane_count;
    if(target>=(unsigned long)count)return 0;
    memset(&command,0,sizeof(command));
    command.operation=kind==WENA_HIERARCHY_LIST?WENA_DOMAIN_MOVE_LIST:WENA_DOMAIN_MOVE_SWIMLANE;
    command.request_version=request;strcpy(command.user_id,a->actor_id);strcpy(command.route,a->route);
    sprintf(command.form_body,"%s=%s&expectedVersion=%lu&targetPosition=%lu&expectedOrder=%s",
        kind==WENA_HIERARCHY_LIST?"listId":"swimlaneId",id,expected,target,order);
    command.form_body_length=strlen(command.form_body);
    if(!wena_sqlite_persistence_apply(&a->persistence,&command,&response))return 0;
    if(selected==(size_t)target)return 1;
    if(kind==WENA_HIERARCHY_LIST){
        list=s->lists[selected];
        if(selected<(size_t)target)memmove(&s->lists[selected],&s->lists[selected+1],((size_t)target-selected)*sizeof(list));
        else memmove(&s->lists[target+1],&s->lists[target],(selected-(size_t)target)*sizeof(list));
        s->lists[target]=list;
        for(index=0;index<count;++index)s->lists[index].sort=(double)index;
    }else{
        lane=s->swimlanes[selected];
        if(selected<(size_t)target)memmove(&s->swimlanes[selected],&s->swimlanes[selected+1],((size_t)target-selected)*sizeof(lane));
        else memmove(&s->swimlanes[target+1],&s->swimlanes[target],(selected-(size_t)target)*sizeof(lane));
        s->swimlanes[target]=lane;
        for(index=0;index<count;++index)s->swimlanes[index].sort=(double)index;
    }
    return 1;
}

int wena_hierarchy_move_mutation_move(void *context,const char *board,
    WenaHierarchyKind kind,const char *id,unsigned long expected,unsigned long target)
{
    WenaHierarchyMoveMutation *a;
    sqlite3_stmt *statement;
    sqlite3_int64 request;
    char order[65];
    size_t selected;
    a=(WenaHierarchyMoveMutation*)context;
    if(!selection(a,board,kind,id,&selected,order))return 0;
    if(sqlite3_prepare_v2(a->persistence.database,
        "SELECT COALESCE(max(request_version),0) FROM idempotency_keys WHERE actor_id=?1 AND route=?2 AND operation=?3",
        -1,&statement,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_text(statement,1,a->actor_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(statement,2,a->route,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(statement,3,kind==WENA_HIERARCHY_LIST?"move-list":"move-swimlane",-1,SQLITE_STATIC);
    request=-1;if(sqlite3_step(statement)==SQLITE_ROW)request=sqlite3_column_int64(statement,0);
    sqlite3_finalize(statement);
    if(request<0||request>=LONG_MAX-1)return 0;
    return wena_hierarchy_move_mutation_move_request(a,board,kind,id,expected,(unsigned long)request+1,target);
}
