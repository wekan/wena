#include "../client/features/hierarchy_move_mutation.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3*d,const char*q){assert(sqlite3_exec(d,q,NULL,NULL,NULL)==SQLITE_OK);}
static int number(sqlite3*d,const char*q){sqlite3_stmt*s;int n;assert(sqlite3_prepare_v2(d,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
static void unchanged(sqlite3*d,WenaSqliteBoardSnapshot*s,WenaSqliteBoardSnapshot*before,int keys){assert(!memcmp(s,before,sizeof(*s)));assert(number(d,"SELECT count(*) FROM idempotency_keys")==keys);}
int main(int argc,char**argv)
{
    const char*hash="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";
    FILE*f;unsigned char*migration;long length;sqlite3*d,*writer;
    WenaSqliteBoardSnapshot *snapshot,*before,*reopened;
    WenaHierarchyMoveMutation adapter,other;
    WenaSqlitePersistence store;WenaDomainCommand command;WenaRegionResponse response;
    WenaHierarchyKind kind;const char*table,*id,*last;char path[512],query[512],body[256];
    unsigned long version,position;size_t i;int keys;
    assert(argc==3);f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));length=ftell(f);assert(length>0);rewind(f);
    migration=(unsigned char*)malloc((size_t)length);assert(migration);assert(fread(migration,1,(size_t)length,f)==(size_t)length);fclose(f);
    snapshot=(WenaSqliteBoardSnapshot*)malloc(sizeof(*snapshot));before=(WenaSqliteBoardSnapshot*)malloc(sizeof(*before));reopened=(WenaSqliteBoardSnapshot*)malloc(sizeof(*reopened));assert(snapshot&&before&&reopened);
    sprintf(path,"%s/hierarchy-move.sqlite",argv[2]);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&d));
    sql(d,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('other','Other',1);");
    sql(d,"INSERT INTO lists VALUES('l0','b','Zero',0,1);INSERT INTO lists VALUES('l1','b','One',1,1);INSERT INTO lists VALUES('l2','b','Two',2,1);");
    sql(d,"INSERT INTO swimlanes VALUES('s0','b','Zero',0,1);INSERT INTO swimlanes VALUES('s1','b','One',1,1);INSERT INTO swimlanes VALUES('s2','b','Two',2,1);");
    for(kind=WENA_HIERARCHY_LIST;kind<=WENA_HIERARCHY_SWIMLANE;kind=(WenaHierarchyKind)((int)kind+1)){
        table=kind==WENA_HIERARCHY_LIST?"lists":"swimlanes";id=kind==WENA_HIERARCHY_LIST?"l0":"s0";last=kind==WENA_HIERARCHY_LIST?"l2":"s2";
        assert(wena_sqlite_board_load(d,"b",snapshot));assert(wena_hierarchy_move_mutation_init(&adapter,d,"u","b",snapshot));
        assert(wena_hierarchy_move_mutation_load(&adapter,"b",kind,id,&version,&position));assert(version==1&&position==0);
        memcpy(before,snapshot,sizeof(*snapshot));keys=number(d,"SELECT count(*) FROM idempotency_keys");
        assert(!wena_hierarchy_move_mutation_move(&adapter,"other",kind,id,1,2));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",WENA_HIERARCHY_BOARD,id,1,2));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,"missing",1,2));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,0,2));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,ULONG_MAX,2));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,1,ULONG_MAX));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,1,3));
        assert(!wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,1,0,2));
        assert(wena_hierarchy_move_mutation_init(&other,d,"unknown","b",snapshot));
        assert(!wena_hierarchy_move_mutation_load(&other,"b",kind,id,&version,&position));
        assert(!wena_hierarchy_move_mutation_move(&other,"b",kind,id,1,2));unchanged(d,snapshot,before,keys);
        assert(wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,1,40,2));
        assert(!strcmp(kind==WENA_HIERARCHY_LIST?snapshot->lists[2].id:snapshot->swimlanes[2].id,id));
        memcpy(before,snapshot,sizeof(*snapshot));++keys;
        assert(wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,2,41,2));unchanged(d,snapshot,before,keys);
        sprintf(query,"SELECT version FROM %s WHERE id='%s'",table,id);assert(number(d,query)==2);
        assert(!wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,1,41,2));
        assert(!wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,2,40,2));unchanged(d,snapshot,before,keys);
        assert(wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,2,41,0));++keys;
        memcpy(before,snapshot,sizeof(*snapshot));sql(d,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'injected'); END;");
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,3,2));unchanged(d,snapshot,before,keys);sql(d,"DROP TRIGGER reject_metadata");
        /* A sibling move changes this row's position but not its version.
         * The order guard must reject a stale cache even though version matches. */
        assert(wena_sqlite_open(path,migration,(size_t)length,hash,&writer));wena_sqlite_persistence_init(&store,writer);
        memset(&command,0,sizeof(command));command.operation=kind==WENA_HIERARCHY_LIST?WENA_DOMAIN_MOVE_LIST:WENA_DOMAIN_MOVE_SWIMLANE;
        command.request_version=88;strcpy(command.user_id,"u");strcpy(command.route,"/b/b/other-client");
        sprintf(body,"%s=%s&expectedVersion=1&targetPosition=0",kind==WENA_HIERARCHY_LIST?"listId":"swimlaneId",last);
        strcpy(command.form_body,body);command.form_body_length=strlen(body);
        /* First reorder two siblings without changing the selected position. */
        sprintf(body,"%s=%s&expectedVersion=1&targetPosition=1",kind==WENA_HIERARCHY_LIST?"listId":"swimlaneId",last);
        strcpy(command.form_body,body);command.form_body_length=strlen(body);
        assert(wena_sqlite_persistence_apply(&store,&command,&response));++keys;
        assert(wena_hierarchy_move_mutation_load(&adapter,"b",kind,id,&version,&position));assert(version==3&&position==0);
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,3,2));
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,3,0));unchanged(d,snapshot,before,keys);
        command.request_version=89;
        sprintf(body,"%s=%s&expectedVersion=2&targetPosition=0",kind==WENA_HIERARCHY_LIST?"listId":"swimlaneId",last);
        strcpy(command.form_body,body);command.form_body_length=strlen(body);
        assert(wena_sqlite_persistence_apply(&store,&command,&response));assert(sqlite3_close(writer)==SQLITE_OK);++keys;
        assert(number(d,query)==3);
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,3,2));
        assert(!wena_hierarchy_move_mutation_load(&adapter,"b",kind,id,&version,&position));unchanged(d,snapshot,before,keys);
        assert(wena_sqlite_board_load(d,"b",snapshot));assert(wena_hierarchy_move_mutation_load(&adapter,"b",kind,id,&version,&position));assert(version==3&&position==1);
        assert(wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,3,2));++keys;
        memcpy(before,snapshot,sizeof(*snapshot));
        sprintf(query,"UPDATE %s SET position=100000 WHERE id='%s'",table,id);sql(d,query);
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,4,0));unchanged(d,snapshot,before,keys);
        sprintf(query,"UPDATE %s SET position=2147483648 WHERE id='%s'",table,id);sql(d,query);
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,4,0));unchanged(d,snapshot,before,keys);
        sprintf(query,"UPDATE %s SET position=0.5 WHERE id='%s'",table,id);sql(d,query);
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,4,0));unchanged(d,snapshot,before,keys);
        sprintf(query,"UPDATE %s SET position=2 WHERE id='%s'",table,id);sql(d,query);
        if(kind==WENA_HIERARCHY_LIST)snapshot->lists[0].sort=99;else snapshot->swimlanes[0].sort=99;
        assert(!wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,4,0));memcpy(snapshot,before,sizeof(*snapshot));
        assert(sqlite3_close(d)==SQLITE_OK);assert(wena_sqlite_open(path,migration,(size_t)length,hash,&d));
        assert(wena_sqlite_board_load(d,"b",reopened));assert(!memcmp(snapshot,reopened,sizeof(*snapshot)));
        assert(wena_hierarchy_move_mutation_init(&adapter,d,"u","b",snapshot));
        assert(!wena_hierarchy_move_mutation_move_request(&adapter,"b",kind,id,4,42,0));
        assert(wena_hierarchy_move_mutation_move(&adapter,"b",kind,id,4,0));
        for(i=0;i<3;++i)assert((kind==WENA_HIERARCHY_LIST?snapshot->lists[i].sort:snapshot->swimlanes[i].sort)==(double)i);
    }
    assert(sqlite3_close(d)==SQLITE_OK);free(migration);free(snapshot);free(before);free(reopened);
    puts("native hierarchy movement tests passed");return 0;
}
