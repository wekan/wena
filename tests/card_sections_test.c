#include "../imports/preferences/sections.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int reject_commit;
static unsigned long statements;
static int authorize(void *context,int action,const char *first,const char *second,const char *database,const char *trigger)
{(void)context;(void)second;(void)database;(void)trigger;return reject_commit&&action==SQLITE_TRANSACTION&&first&&!strcmp(first,"COMMIT")?SQLITE_DENY:SQLITE_OK;}
static int trace(unsigned int kind,void *context,void *query,void *extra)
{(void)kind;(void)context;(void)query;(void)extra;++statements;return 0;}
static void sql(sqlite3 *db,const char *text){int result;result=sqlite3_exec(db,text,NULL,NULL,NULL);if(result!=SQLITE_OK)fprintf(stderr,"%s: %s\n",text,sqlite3_errmsg(db));assert(result==SQLITE_OK);}
static sqlite3_int64 number(sqlite3 *db,const char *text)
{sqlite3_stmt *s;sqlite3_int64 n;assert(sqlite3_prepare_v2(db,text,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int64(s,0);assert(sqlite3_finalize(s)==SQLITE_OK);return n;}
static unsigned char *read_file(const char *path,long *size)
{
 FILE *f;unsigned char *bytes;
 f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));*size=ftell(f);assert(*size>0);rewind(f);
 bytes=(unsigned char*)malloc((size_t)*size);assert(bytes);assert(fread(bytes,1,(size_t)*size,f)==(size_t)*size);fclose(f);return bytes;
}
int main(int argc,char **argv)
{
 sqlite3 *db,*second;unsigned char *bundle,*old;long size,old_size;char hash[65],key[129],query[320];
 WenaCardSectionsSnapshot *view,*other,*before;const WenaCardSectionPreference *entry;unsigned long version;int i;
 assert(argc==4);
 old=read_file(argv[3],&old_size);wena_sha256_hex(old,(size_t)old_size,hash);
 assert(wena_sqlite_open(argv[2],old,(size_t)old_size,hash,&db));free(old);
 assert(wena_sqlite_schema_version(db)==7);
 sql(db,"INSERT INTO boards VALUES('survivor','Preserved board',17)");
 bundle=read_file(argv[1],&size);wena_sha256_hex(bundle,(size_t)size,hash);
 reject_commit=1;assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);
 assert(!wena_sqlite_upgrade(db,hash));reject_commit=0;assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);
 assert(wena_sqlite_schema_version(db)==7&&!number(db,"SELECT count(*) FROM sqlite_master WHERE name='actor_card_sections'"));
 assert(sqlite3_close(db)==SQLITE_OK);
 assert(wena_sqlite_open(argv[2],bundle,(size_t)size,hash,&db));assert(wena_sqlite_schema_version(db)==13);
 assert(number(db,"SELECT version FROM boards WHERE id='survivor'")==17);

 sql(db,"INSERT INTO actors VALUES('u','User',1),('v','Other',1);INSERT INTO boards VALUES('b','Board',1),('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1),('archived','b','s','l','Archived',1,1,1)");
 view=NULL;other=NULL;
 assert(wena_card_sections_load(db,"u","b",&view)&&!view->count);
 assert(wena_card_section_checklist_key("abc",key,sizeof(key))&&!strcmp(key,"checklist-abc"));
 assert(!wena_card_section_checklist_key("../bad",key,sizeof(key)));
 assert(!wena_card_section_checklist_key("abc",key,4));
 assert(!wena_card_section_key_valid("bad.key")&&!wena_card_section_key_valid("")&&!wena_card_section_key_valid(NULL));
 memset(key,'x',128);key[128]=0;assert(wena_card_section_key_valid(key));
 assert(wena_card_section_save(db,"u","b","c","description",0,0));
 assert(!number(db,"SELECT count(*) FROM actor_card_sections"));
 assert(!wena_card_section_save(db,"missing","b","c","description",0,1));
 assert(!wena_card_section_save(db,"u","other","c","description",0,1));
 assert(!wena_card_section_save(db,"u","b","archived","description",0,1));
 assert(!wena_card_section_save(db,"u","b","c","description",0,2));
 sql(db,"BEGIN");assert(!wena_card_section_save(db,"u","b","c","description",0,1));assert(!wena_card_sections_load(db,"u","b",&view));assert(!sqlite3_get_autocommit(db));sql(db,"ROLLBACK");
 assert(wena_card_section_save(db,"u","b","c","description",0,1));
 assert(wena_card_section_save(db,"u","b","c","checklist-abc",0,1));
 assert(wena_card_sections_load(db,"u","b",&view)&&view->count==2);
 entry=wena_card_sections_find(view,"c","description");assert(entry&&entry->collapsed&&entry->version==1);
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,NULL)==SQLITE_OK);
 for(i=0;i<1000;++i)assert(wena_card_sections_find(view,"c","checklist-abc"));
 assert(!statements);assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);
 assert(wena_card_sections_load(db,"v","b",&other)&&!other->count);
 assert(wena_card_section_save(db,"v","b","c","description",0,1));
 assert(!wena_card_section_save(db,"u","b","c","description",0,0));
 assert(wena_card_section_save(db,"u","b","c","description",1,0));
 assert(wena_card_sections_load(db,"u","b",&view));entry=wena_card_sections_find(view,"c","description");assert(entry&&!entry->collapsed&&entry->version==2);
 assert(wena_card_sections_load(db,"v","b",&other));assert(wena_card_sections_find(other,"c","description")->collapsed);
 assert(wena_card_section_save(db,"u","b","c","description",2,0));
 assert(number(db,"SELECT version FROM actor_card_sections WHERE actor_id='u' AND section_key='description'")==2);
 assert(number(db,"SELECT version FROM cards WHERE id='c'")==1&&number(db,"SELECT version FROM boards WHERE id='b'")==1);
 sql(db,"CREATE TRIGGER reject_section AFTER UPDATE ON actor_card_sections BEGIN SELECT RAISE(ABORT,'late'); END");
 assert(!wena_card_section_save(db,"u","b","c","description",2,1));sql(db,"DROP TRIGGER reject_section");
 sql(db,"CREATE TRIGGER reject_section BEFORE UPDATE ON actor_card_sections BEGIN SELECT RAISE(IGNORE); END");
 assert(!wena_card_section_save(db,"u","b","c","description",2,1));sql(db,"DROP TRIGGER reject_section");
 reject_commit=1;assert(sqlite3_set_authorizer(db,authorize,NULL)==SQLITE_OK);
 before=view;assert(!wena_card_sections_load(db,"u","b",&view)&&view==before);
 assert(!wena_card_section_save(db,"u","b","c","description",2,1));
 reject_commit=0;assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);
 assert(number(db,"SELECT version FROM actor_card_sections WHERE actor_id='u' AND section_key='description'")==2);
 assert(sqlite3_open(argv[2],&second)==SQLITE_OK);
 assert(wena_card_section_save(second,"u","b","c","description",2,1));assert(sqlite3_close(second)==SQLITE_OK);
 assert(!wena_card_section_save(db,"u","b","c","description",2,0));
 before=view;assert(!wena_card_sections_load(db,"missing","b",&view)&&view==before);
 sql(db,"PRAGMA ignore_check_constraints=ON;UPDATE actor_card_sections SET collapsed=2 WHERE actor_id='u' AND section_key='description'");
 assert(!wena_card_sections_load(db,"u","b",&view)&&view==before);
 assert(!wena_card_section_save(db,"u","b","c","description",3,0));
 sql(db,"UPDATE actor_card_sections SET collapsed=1 WHERE actor_id='u' AND section_key='description';PRAGMA ignore_check_constraints=OFF");
 for(i=2;i<128;++i){sprintf(key,"section-%d",i);assert(wena_card_section_save(db,"u","b","c",key,0,1));}
 assert(!wena_card_section_save(db,"u","b","c","overflow",0,1));
 assert(wena_card_sections_load(db,"u","b",&view)&&view->count==128);
 sql(db,"INSERT INTO actor_card_sections VALUES('u','c','overflow',1,1)");before=view;
 assert(!wena_card_sections_load(db,"u","b",&view)&&view==before);
 sql(db,"DELETE FROM actor_card_sections WHERE section_key='overflow'");
 sprintf(query,"UPDATE actor_card_sections SET version=%lu WHERE actor_id='u' AND section_key='description'",WENA_VERSION_MUTATE_MAX);sql(db,query);
 assert(wena_card_section_save(db,"u","b","c","description",WENA_VERSION_MUTATE_MAX,0));
 assert(!wena_card_section_save(db,"u","b","c","description",WENA_VERSION_READ_MAX,1));
 assert(wena_card_section_save(db,"v","b","c","minicard",0,1));
 assert(sqlite3_close(db)==SQLITE_OK);assert(wena_sqlite_open(argv[2],bundle,(size_t)size,hash,&db));
 assert(wena_card_sections_load(db,"u","b",&view));entry=wena_card_sections_find(view,"c","description");version=entry->version;
 assert(!entry->collapsed&&version==WENA_VERSION_READ_MAX&&view->count==128);
 assert(wena_card_sections_load(db,"v","b",&other));
 entry=wena_card_sections_find(other,"c","minicard");assert(entry&&entry->collapsed&&entry->version==1);
 assert(!wena_card_sections_find(view,"c","minicard"));
 assert(number(db,"SELECT version FROM cards WHERE id='c'")==1);
 assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
 wena_card_sections_free(view);wena_card_sections_free(other);free(bundle);
 puts("Generic actor/card sections: scope, versions, actor isolation, no-op, rollback, capacity and reopen passed");return 0;
}
