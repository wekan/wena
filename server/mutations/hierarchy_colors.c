#include "hierarchy_colors.h"
#include "../list_state.h"
#include "../../models/model.h"
#include "../../models/color.h"
#include <stdio.h>
#include <string.h>

static int read_color(sqlite3 *db,const char *parent,const char *table,const char *key,
    const char *board,const char *id,unsigned long expected,char color[WENA_COLOR_CAPACITY])
{
    sqlite3_stmt *s;char sql[512],candidate[WENA_COLOR_CAPACITY];
    const unsigned char *text,*scope;int ok,bytes;
    sprintf(sql,"SELECT p.version,c.color,c.board_id,c.%s FROM %s p LEFT JOIN %s c ON c.%s=p.id WHERE p.id=?1 AND p.board_id=?2",key,parent,table,key);
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_ROW&&
        sqlite3_column_type(s,0)==SQLITE_INTEGER&&sqlite3_column_int64(s,0)==(sqlite3_int64)expected;
    candidate[0]=0;
    if(ok&&sqlite3_column_type(s,3)!=SQLITE_NULL){
        ok=sqlite3_column_type(s,1)==SQLITE_TEXT&&sqlite3_column_type(s,2)==SQLITE_TEXT;
        text=sqlite3_column_text(s,1);scope=sqlite3_column_text(s,2);bytes=sqlite3_column_bytes(s,1);
        ok=ok&&text&&bytes>=0&&bytes<(int)sizeof(candidate)&&
            !memchr(text,0,(size_t)bytes)&&sqlite3_column_type(s,2)==SQLITE_TEXT&&scope&&
            sqlite3_column_bytes(s,2)==(int)strlen(board)&&!strcmp((const char*)scope,board);
        if(ok){memcpy(candidate,text,(size_t)bytes);candidate[bytes]=0;ok=wena_color_valid(candidate);}
    }
    if(ok)ok=sqlite3_step(s)==SQLITE_DONE;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(ok)strcpy(color,candidate);
    return ok;
}

int wena_sqlite_hierarchy_color_change(sqlite3 *db,const WenaDomainCommand *command,
    const char *board,unsigned long *result_version)
{
    const char *parent,*table,*key,*field;char id[WENA_ID_CAPACITY],text[32];
    char color[WENA_COLOR_CAPACITY],stored[WENA_COLOR_CAPACITY],sql[640];
    unsigned long expected;sqlite3_stmt *s;int lists,ok;
    if(!db||!command||!result_version||sqlite3_get_autocommit(db)||!wena_model_identifier_valid(board)||
        (command->operation!=WENA_DOMAIN_SET_LIST_COLOR&&command->operation!=WENA_DOMAIN_SET_SWIMLANE_COLOR))return 0;
    lists=command->operation==WENA_DOMAIN_SET_LIST_COLOR;
    parent=lists?"lists":"swimlanes";table=lists?"list_colors":"swimlane_colors";
    key=lists?"list_id":"swimlane_id";field=lists?"listId":"swimlaneId";
    if(!wena_mutation_text(command,field,id,sizeof(id),0)||!wena_model_identifier_valid(id)||
        !wena_mutation_text(command,"color",color,sizeof(color),2)||!wena_color_valid(color)||
        !wena_mutation_text(command,"expectedVersion",text,sizeof(text),0)||
        !wena_mutation_decimal(text,0,WENA_VERSION_MUTATE_MAX,&expected)||
        (lists&&!wena_sqlite_list_active(db,board,id))||
        !read_color(db,parent,table,key,board,id,expected,stored))return 0;
    if(!strcmp(color,stored)){*result_version=expected;return 2;}
    sprintf(sql,"INSERT INTO %s(%s,board_id,color) VALUES(?1,?2,?3) ON CONFLICT(%s) DO UPDATE SET color=excluded.color WHERE %s.board_id=excluded.board_id",table,key,key,table);
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,3,color,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(!ok)return 0;
    sprintf(sql,"UPDATE %s SET version=version+1 WHERE id=?1 AND board_id=?2 AND version=?3",parent);
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_text(s,2,board,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
        sqlite3_bind_int64(s,3,(sqlite3_int64)expected)==SQLITE_OK&&sqlite3_step(s)==SQLITE_DONE&&sqlite3_changes(db)==1;
    if(sqlite3_finalize(s)!=SQLITE_OK)ok=0;
    if(!ok||!read_color(db,parent,table,key,board,id,expected+1,stored)||strcmp(color,stored)||
        (lists&&!wena_sqlite_list_active(db,board,id)))return 0;
    *result_version=expected+1;return 1;
}
