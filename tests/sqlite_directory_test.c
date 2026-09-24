#include "../server/sqlite_directory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int reject_commit,append_on_read;
static sqlite3 *writer;
static void sql(sqlite3 *db,const char *query)
{assert(sqlite3_exec(db,query,NULL,NULL,NULL)==SQLITE_OK);}
static int authorize(void *data,int action,const char *first,const char *second,const char *db,const char *trigger)
{(void)data;(void)second;(void)db;(void)trigger;return reject_commit&&action==SQLITE_TRANSACTION&&first&&!strcmp(first,"COMMIT")?SQLITE_DENY:SQLITE_OK;}
static int trace(unsigned int kind,void *data,void *query,void *extra)
{
 const char *text;(void)kind;(void)data;(void)extra;text=sqlite3_sql((sqlite3_stmt *)query);
 if(append_on_read&&text&&!strncmp(text,"SELECT id,title",15)){append_on_read=0;sql(writer,"INSERT INTO boards VALUES('z','Concurrent',1)");}
 return 0;
}
int main(int argc,char **argv)
{
 sqlite3 *db;FILE *file;char *schema,query[256];long size;
 WenaDirectoryPage page,before;int i;
 assert(argc==3);file=fopen(argv[1],"rb");assert(file&&!fseek(file,0,SEEK_END));size=ftell(file);assert(size>0);rewind(file);
 schema=(char *)malloc((size_t)size+1);assert(schema&&fread(schema,1,(size_t)size,file)==(size_t)size);schema[size]=0;fclose(file);
 assert(sqlite3_open(argv[2],&db)==SQLITE_OK);sql(db,schema);free(schema);sql(db,"PRAGMA journal_mode=WAL;INSERT INTO actors VALUES('u','Local user',1)");
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,0,16,&page)&&!page.total&&!page.count);
 sql(db,"BEGIN");
 for(i=0;i<65;++i){sprintf(query,"INSERT INTO boards VALUES('b%03d','Same title',1)",i);sql(db,query);}
 sql(db,"COMMIT");
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,2,16,&page));
 assert(page.total==65&&page.first==32&&page.count==16&&!strcmp(page.rows[0].id,"b032"));
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,(size_t)-1,16,&page));
 assert(page.page==4&&page.first==64&&page.count==1&&!strcmp(page.rows[0].id,"b064"));
 before=page;
 assert(!wena_sqlite_directory_load(db,"missing",WENA_DIRECTORY_BOARDS,0,16,&page)&&!memcmp(&before,&page,sizeof(page)));
 assert(!wena_sqlite_directory_load(db,"u",(WenaDirectoryKind)3,0,16,&page));
 assert(!wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,0,33,&page));
 assert(!wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,0,0,&page));
 sql(db,"BEGIN");assert(!wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,0,16,&page)&&!sqlite3_get_autocommit(db));sql(db,"ROLLBACK");
 assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);reject_commit=1;
 assert(!wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,0,16,&page)&&!memcmp(&before,&page,sizeof(page)));
 reject_commit=0;assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);
 sql(db,"UPDATE boards SET version=1.5 WHERE id='b064'");
 assert(!wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,4,16,&page)&&!memcmp(&before,&page,sizeof(page)));
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,0,16,&page));before=page;
 sql(db,"UPDATE boards SET version=1,title=CAST(x'C0AF' AS TEXT) WHERE id='b064'");
 assert(!wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,4,16,&page)&&!memcmp(&before,&page,sizeof(page)));
 sql(db,"UPDATE boards SET title='Restored' WHERE id='b064'");
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_ACTORS,0,16,&page));
 assert(page.count==1&&!strcmp(page.rows[0].id,"u")&&!strcmp(page.rows[0].title,"Local user"));
 before=page;page.count=33;assert(!wena_directory_page_valid(&page));page=before;
 page.page=1;assert(!wena_directory_page_valid(&page));page=before;
 page.page_size=0;assert(!wena_directory_page_valid(&page));page=before;
 page.rows[0].version=0;assert(!wena_directory_page_valid(&page));page=before;
 assert(sqlite3_open(argv[2],&writer)==SQLITE_OK);
 append_on_read=1;assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,4,16,&page));
 assert(!append_on_read&&page.total==65&&page.count==1);
 assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_sqlite_directory_load(db,"u",WENA_DIRECTORY_BOARDS,4,16,&page)&&page.total==66&&page.count==2);
 assert(!strcmp(page.rows[1].id,"z"));assert(sqlite3_close(writer)==SQLITE_OK);
 sql(db,"INSERT INTO lists VALUES('l','b000','List',0,1),('l2','b001','Other',0,1);"
  "INSERT INTO swimlanes VALUES('s','b000','Lane',0,1),('s2','b001','Other',0,1);"
  "INSERT INTO cards VALUES('c0','b000','s','l','Shared',0,0,1),('c1','b000','s','l','Shared',1,0,2),"
  "('hidden','b000','s','l','Archived',2,1,1),('other','b001','s2','l2','Other',0,0,1)");
 assert(wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"b000",0,1,&page));
 assert(page.total==2&&page.count==1&&!strcmp(page.board_id,"b000")&&!strcmp(page.rows[0].id,"c0"));
 assert(wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"b000",(size_t)-1,1,&page));
 assert(page.page==1&&page.rows[0].version==2&&!strcmp(page.rows[0].id,"c1"));before=page;
 assert(!wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"missing",0,1,&page)&&!memcmp(&page,&before,sizeof(page)));
 assert(!wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"",0,1,&page));
 assert(!wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_BOARDS,"b000",0,1,&page));
 assert(wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"b002",0,1,&page)&&!page.total);
 assert(wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"b001",0,1,&page)&&page.total==1&&!strcmp(page.rows[0].id,"other"));
 before=page;page.board_id[0]=0;assert(!wena_directory_page_valid(&page));page=before;
 sql(db,"UPDATE cards SET title=CAST(x'C0AF' AS TEXT) WHERE id='c1'");
 assert(!wena_sqlite_directory_load_scoped(db,"u",WENA_DIRECTORY_CARDS,"b000",1,1,&page)&&!memcmp(&page,&before,sizeof(page)));
 assert(sqlite3_close(db)==SQLITE_OK);
 puts("Shared directory pages: boards/actors, bounds, snapshot consistency, strict rows and atomic failure passed");
 return 0;
}
