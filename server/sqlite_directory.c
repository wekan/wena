#include "sqlite_directory.h"
#include <string.h>

static const char *text_column(sqlite3_stmt *statement,int column,int id)
{
    const unsigned char *text;
    int length;
    if (sqlite3_column_type(statement,column)!=SQLITE_TEXT) return NULL;
    length=sqlite3_column_bytes(statement,column);
    if (length<=0 || length >= (id ? WENA_ID_CAPACITY : WENA_TITLE_CAPACITY)) return NULL;
    text=sqlite3_column_text(statement,column);
    if (!text || strlen((const char *)text)!=(size_t)length ||
        (id ? !wena_model_identifier_valid((const char *)text) :
         !wena_model_title_valid((const char *)text,(size_t)length,WENA_TITLE_CAPACITY))) return NULL;
    return (const char *)text;
}
int wena_sqlite_directory_load(sqlite3 *database,const char *actor,
    WenaDirectoryKind kind,size_t page,size_t page_size,WenaDirectoryPage *output)
{
    static const char *counts[]={
        "SELECT count(*),EXISTS(SELECT 1 FROM actors WHERE id=?1) FROM boards",
        "SELECT count(*),EXISTS(SELECT 1 FROM actors WHERE id=?1) FROM actors"
    };
    static const char *rows[]={
        "SELECT id,title,version FROM boards ORDER BY id LIMIT ?1 OFFSET ?2",
        "SELECT id,display_name,version FROM actors ORDER BY id LIMIT ?1 OFFSET ?2"
    };
    WenaDirectoryPage candidate;
    sqlite3_stmt *statement;
    sqlite3_int64 total,version;
    size_t expected,last;
    int valid,step,index;
    const char *id,*title;
    if (!database || !output || !wena_model_identifier_valid(actor) ||
        (kind!=WENA_DIRECTORY_BOARDS && kind!=WENA_DIRECTORY_ACTORS) ||
        !page_size || page_size>WENA_DIRECTORY_PAGE_CAPACITY ||
        !sqlite3_get_autocommit(database)) return 0;
    if (sqlite3_exec(database,"BEGIN",NULL,NULL,NULL)!=SQLITE_OK) return 0;
    memset(&candidate,0,sizeof(candidate));candidate.kind=kind;candidate.page_size=page_size;
    statement=NULL;valid=0;index=(int)kind-1;
    if (sqlite3_prepare_v2(database,counts[index],-1,&statement,NULL)!=SQLITE_OK) goto done;
    if (sqlite3_bind_text(statement,1,actor,-1,SQLITE_TRANSIENT)!=SQLITE_OK ||
        sqlite3_step(statement)!=SQLITE_ROW || sqlite3_column_type(statement,0)!=SQLITE_INTEGER ||
        sqlite3_column_int(statement,1)!=1) goto done;
    total=sqlite3_column_int64(statement,0);
    if (total<0 || (sqlite3_uint64)total>(sqlite3_uint64)(size_t)-1) goto done;
    candidate.total=(size_t)total;
    if (sqlite3_step(statement)!=SQLITE_DONE) goto done;
    if (sqlite3_finalize(statement)!=SQLITE_OK) {statement=NULL;goto done;}
    statement=NULL;
    last=candidate.total ? (candidate.total-1)/page_size : 0;
    candidate.page=page>last ? last : page;
    candidate.first=candidate.page*page_size;
    expected=candidate.total-candidate.first;
    if (expected>page_size) expected=page_size;
    if (sqlite3_prepare_v2(database,rows[index],-1,&statement,NULL)!=SQLITE_OK) goto done;
    if (sqlite3_bind_int(statement,1,(int)page_size)!=SQLITE_OK ||
        sqlite3_bind_int64(statement,2,(sqlite3_int64)candidate.first)!=SQLITE_OK) goto done;
    while ((step=sqlite3_step(statement))==SQLITE_ROW) {
        if (candidate.count>=expected || !(id=text_column(statement,0,1)) ||
            !(title=text_column(statement,1,0)) || sqlite3_column_type(statement,2)!=SQLITE_INTEGER)
            goto done;
        version=sqlite3_column_int64(statement,2);
        if (version<=0 || version>(sqlite3_int64)WENA_VERSION_READ_MAX ||
            (candidate.count && strcmp(candidate.rows[candidate.count-1].id,id)>=0)) goto done;
        strcpy(candidate.rows[candidate.count].id,id);
        strcpy(candidate.rows[candidate.count].title,title);
        candidate.rows[candidate.count].version=(unsigned long)version;
        ++candidate.count;
    }
    valid=step==SQLITE_DONE && candidate.count==expected && wena_directory_page_valid(&candidate);
done:
    if (statement && sqlite3_finalize(statement)!=SQLITE_OK) valid=0;
    if (valid && sqlite3_exec(database,"COMMIT",NULL,NULL,NULL)==SQLITE_OK) {
        *output=candidate;return 1;
    }
    (void)sqlite3_exec(database,"ROLLBACK",NULL,NULL,NULL);
    return 0;
}

int wena_sqlite_directory_reader_init(WenaSqliteDirectoryReader *reader,
    sqlite3 *database,const char *actor)
{
    if (!reader || !database || !wena_model_identifier_valid(actor)) return 0;
    reader->database=database;strcpy(reader->actor,actor);return 1;
}
int wena_sqlite_directory_read(void *context,WenaDirectoryKind kind,
    size_t page,size_t page_size,WenaDirectoryPage *output)
{
    WenaSqliteDirectoryReader *reader;
    reader=(WenaSqliteDirectoryReader *)context;
    return reader && wena_sqlite_directory_load(reader->database,reader->actor,
        kind,page,page_size,output);
}
