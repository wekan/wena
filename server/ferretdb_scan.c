#include "ferretdb_scan.h"
#include "ferretdb_compat.h"
#include "../imports/ferretdb/sjson.h"
#include <sqlite3.h>
#include <string.h>
static int table_valid(sqlite3 *db,const char *name)
{
    sqlite3_stmt *s;int ok;
    const char *sql="SELECT count(*) FROM pragma_table_list t JOIN pragma_table_xinfo(?1) c "
        "WHERE t.schema='main' AND t.name=?1 AND t.type='table' AND t.strict=1 AND t.ncol=1 "
        "AND c.name='_ferretdb_sjson' AND c.type='TEXT' AND c.[notnull]=1 AND c.hidden=0";
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    sqlite3_bind_text(s,1,name,-1,SQLITE_TRANSIENT);
    ok=sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)==1;
    sqlite3_finalize(s);return ok;
}
static int scan_table(sqlite3 *db,const char *table,size_t limit,size_t *count)
{
    char *sql;sqlite3_stmt *rows;int rc,ok;WenaSjsonDocument *document;
    const char *value;size_t length;
    if(!table_valid(db,table))return 0;
    sql=sqlite3_mprintf("SELECT _ferretdb_sjson FROM \"%w\"",table);if(!sql)return 0;
    rc=sqlite3_prepare_v2(db,sql,-1,&rows,NULL);sqlite3_free(sql);
    if(rc!=SQLITE_OK)return 0;
    document=NULL;ok=1;
    while((rc=sqlite3_step(rows))==SQLITE_ROW){
        if(*count==limit||sqlite3_column_type(rows,0)!=SQLITE_TEXT){ok=0;break;}
        value=(const char*)sqlite3_column_text(rows,0);
        length=(size_t)sqlite3_column_bytes(rows,0);
        if(!value||!wena_sjson_parse(value,length,&document)){ok=0;break;}
        ++*count;wena_sjson_free(document);document=NULL;
    }
    wena_sjson_free(document);sqlite3_finalize(rows);
    return ok&&rc==SQLITE_DONE;
}
int wena_ferretdb_scan_readonly(const char *path,size_t limit,size_t *documents)
{
    sqlite3 *db;sqlite3_stmt *collections;const char *table;size_t count;int rc,ok;
    if(!path||path[0]!='/'||!documents)return 0;
    db=NULL;collections=NULL;ok=0;count=0;
    if(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK)goto done;
    if(sqlite3_exec(db,"PRAGMA query_only=ON;BEGIN",NULL,NULL,NULL)!=SQLITE_OK)goto done;
    if(wena_ferretdb_probe_connection(db)!=WENA_FERRET_V1_SJSON_INDEX2)goto done;
    if(sqlite3_prepare_v2(db,"SELECT table_name, CASE WHEN json_valid(settings) THEN "
        "json_extract(settings,'$.indexFormat')=2 ELSE 0 END FROM _ferretdb_collections",-1,&collections,NULL)!=SQLITE_OK)goto done;
    while((rc=sqlite3_step(collections))==SQLITE_ROW){
        table=(const char*)sqlite3_column_text(collections,0);
        if(!table||strlen(table)!=(size_t)sqlite3_column_bytes(collections,0)||
            sqlite3_column_int(collections,1)!=1||!scan_table(db,table,limit,&count))goto done;
    }
    if(rc!=SQLITE_DONE)goto done;
    sqlite3_finalize(collections);collections=NULL;
    if(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)goto done;
    *documents=count;ok=1;
 done:
    if(collections)sqlite3_finalize(collections);
    if(db)sqlite3_close(db);
    return ok;
}
