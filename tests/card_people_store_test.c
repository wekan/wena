#include "../server/card_people_store.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *s)
{char *error=NULL;int result;result=sqlite3_exec(db,s,NULL,NULL,&error);if(result!=SQLITE_OK)fprintf(stderr,"%s: %s\n",s,error);sqlite3_free(error);assert(result==SQLITE_OK);}
static void bad(sqlite3 *db,const char *change,WenaMemberRoster *roster,WenaCardPeopleSnapshot *people,int r,int p)
{
 WenaMemberRoster *old_roster;WenaCardPeopleSnapshot *old_people;
 old_roster=(WenaMemberRoster*)malloc(sizeof(*old_roster));old_people=(WenaCardPeopleSnapshot*)malloc(sizeof(*old_people));assert(old_roster&&old_people);
 *old_roster=*roster;*old_people=*people;sql(db,"BEGIN");sql(db,change);
 if(r)assert(!wena_sqlite_member_roster_read(db,"b",roster)&&!memcmp(old_roster,roster,sizeof(*roster)));
 if(p)assert(!wena_sqlite_card_people_read(db,"b","c",people)&&!memcmp(old_people,people,sizeof(*people)));
 assert(!sqlite3_get_autocommit(db));sql(db,"ROLLBACK");free(old_roster);free(old_people);
}
static int deny(void *data,int action,const char *one,const char *two,const char *database,const char *trigger)
{(void)data;(void)two;(void)database;(void)trigger;return action==SQLITE_READ&&one&&!strcmp(one,"actors")?SQLITE_DENY:SQLITE_OK;}
typedef struct Concurrent {sqlite3 *writer;int fired;} Concurrent;
static int concurrent(unsigned int event,void *data,void *statement,void *unused)
{
 Concurrent *c;const char *query;(void)unused;c=(Concurrent*)data;
 query=event==SQLITE_TRACE_STMT?sqlite3_sql((sqlite3_stmt*)statement):NULL;
 if(!c->fired&&query&&strstr(query,"FROM board_members")){
  c->fired=1;sql(c->writer,"BEGIN;UPDATE boards SET version=version+1 WHERE id='b';"
   "UPDATE board_members SET active=0,version=version+1 WHERE actor_id='a';"
   "UPDATE actors SET display_name='Changed',version=version+1 WHERE id='a';"
   "UPDATE cards SET version=version+1 WHERE id='c';DELETE FROM card_people WHERE actor_id='a';COMMIT");
 }
 return 0;
}
int main(int argc,char **argv)
{
 FILE *file;long length;unsigned char *bundle;char hash[65],query[512];sqlite3 *db,*writer;
 WenaMemberRoster *roster,*saved_roster;WenaCardPeopleSnapshot *people,*saved_people;size_t i;Concurrent change;
 assert(argc==3);file=fopen(argv[1],"rb");assert(file&&!fseek(file,0,SEEK_END));length=ftell(file);assert(length>0);rewind(file);
 bundle=(unsigned char*)malloc((size_t)length);assert(bundle&&fread(bundle,1,(size_t)length,file)==(size_t)length&&!fclose(file));
 wena_sha256_hex(bundle,(size_t)length,hash);assert(wena_sqlite_open(argv[2],bundle,(size_t)length,hash,&db));
 roster=(WenaMemberRoster*)calloc(1,sizeof(*roster));people=(WenaCardPeopleSnapshot*)calloc(1,sizeof(*people));
 saved_roster=(WenaMemberRoster*)malloc(sizeof(*saved_roster));saved_people=(WenaCardPeopleSnapshot*)malloc(sizeof(*saved_people));
 assert(roster&&people&&saved_roster&&saved_people);
 sql(db,"INSERT INTO boards VALUES('b','Board',3),('other','Other',1);"
  "INSERT INTO actors VALUES('a','Repeated',1),('z','Repeated',2);INSERT INTO lists VALUES('l','b','List',0,1);"
  "INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,7)");
 assert(!wena_sqlite_member_roster_read(db,"b",roster)&&!wena_sqlite_card_people_read(db,"b","c",people));
 sql(db,"BEGIN");assert(wena_sqlite_member_roster_read(db,"b",roster)&&!roster->count&&roster->board_version==3);
 assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->card_version==7&&!people->fields[0].count&&!people->fields[1].count);
 assert(!wena_sqlite_card_people_read(db,"other","c",people));assert(!wena_sqlite_member_roster_read(db,"missing",roster));sql(db,"COMMIT");
 sql(db,"INSERT INTO board_members VALUES('b','z',0,2,5,8),('b','a',1,3,0,10);"
  "INSERT INTO card_people VALUES('b','c','members','z',5),('b','c','members','a',8),('b','c','assignees','a',2147483647)");
 sql(db,"BEGIN");assert(wena_sqlite_member_roster_read(db,"b",roster)&&roster->count==2);
 assert(!strcmp(roster->members[0].actor_id,"a")&&!strcmp(roster->members[1].actor_id,"z")&&!roster->members[1].active);
 assert(roster->versions[0]==3&&roster->actor_versions[1]==2&&roster->created[1]==5&&roster->updated[1]==8);
 assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->fields[0].count==2&&people->fields[1].count==1);
 assert(!strcmp(people->fields[0].ids[0],"z")&&people->positions[0][0]==5&&people->positions[0][1]==8&&people->positions[1][0]==2147483647UL);
 *saved_roster=*roster;*saved_people=*people;
 assert(wena_sqlite_member_roster_read(db,roster->board_id,roster)&&!memcmp(roster,saved_roster,sizeof(*roster)));
 assert(wena_sqlite_card_people_read(db,people->fields[0].board_id,people->card_id,people)&&!memcmp(people,saved_people,sizeof(*people)));
 assert(sqlite3_set_authorizer(db,deny,NULL)==SQLITE_OK);
 assert(!wena_sqlite_member_roster_read(db,"b",roster)&&!memcmp(roster,saved_roster,sizeof(*roster)));
 assert(!wena_sqlite_card_people_read(db,"b","c",people)&&!memcmp(people,saved_people,sizeof(*people)));
 assert(sqlite3_set_authorizer(db,NULL,NULL)==SQLITE_OK);sql(db,"ROLLBACK");
 /* Read legacy/corrupt rows without relying on DDL constraints to reject them. */
 sql(db,"PRAGMA foreign_keys=OFF;PRAGMA ignore_check_constraints=ON");
 bad(db,"UPDATE boards SET version=0",roster,people,1,1);
 bad(db,"UPDATE actors SET version=0",roster,people,1,1);
 bad(db,"DELETE FROM actors WHERE id='a'",roster,people,1,1);
 bad(db,"UPDATE actors SET display_name=x'ff' WHERE id='a'",roster,people,1,0);
 bad(db,"UPDATE actors SET display_name='Name'||char(0) WHERE id='a'",roster,people,1,0);
 bad(db,"UPDATE board_members SET active=4294967297",roster,people,1,0);
 bad(db,"UPDATE board_members SET active=0.5",roster,people,1,0);
 bad(db,"UPDATE board_members SET updated_at=created_at-1",roster,people,1,0);
 bad(db,"UPDATE board_members SET actor_id='bad/id' WHERE actor_id='a'",roster,people,1,0);
 bad(db,"UPDATE board_members SET version=0",roster,people,1,0);
 bad(db,"UPDATE card_people SET board_id='other'",roster,people,0,1);
 bad(db,"UPDATE card_people SET field='watchers' WHERE field='members'",roster,people,0,1);
 bad(db,"UPDATE card_people SET position=-1 WHERE actor_id='z'",roster,people,0,1);
 bad(db,"UPDATE card_people SET position=0.5 WHERE actor_id='z'",roster,people,0,1);
 bad(db,"UPDATE card_people SET position=2147483648 WHERE actor_id='z'",roster,people,0,1);
 bad(db,"UPDATE cards SET archived=4294967296",roster,people,0,1);
 bad(db,"UPDATE cards SET version=0",roster,people,0,1);
 sprintf(query,"UPDATE boards SET version=%lu",WENA_VERSION_READ_MAX+1UL);bad(db,query,roster,people,1,1);
 sprintf(query,"UPDATE board_members SET version=%lu",WENA_VERSION_READ_MAX+1UL);bad(db,query,roster,people,1,0);
 sprintf(query,"UPDATE cards SET version=%lu",WENA_VERSION_READ_MAX+1UL);bad(db,query,roster,people,0,1);
 sql(db,"PRAGMA foreign_keys=ON;PRAGMA ignore_check_constraints=OFF");
 /* Terminal revisions remain readable, even though future writes reject them. */
 sql(db,"BEGIN");sprintf(query,"UPDATE cards SET version=%lu,archived=1",WENA_VERSION_READ_MAX);sql(db,query);
 sprintf(query,"UPDATE board_members SET version=%lu",WENA_VERSION_READ_MAX);sql(db,query);
 assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->archived&&people->card_version==WENA_VERSION_READ_MAX);
 assert(wena_sqlite_member_roster_read(db,"b",roster)&&roster->versions[0]==WENA_VERSION_READ_MAX);sql(db,"ROLLBACK");
 /* Both readers see the same snapshot even if a writer changes every source. */
 assert(wena_sqlite_open(argv[2],bundle,(size_t)length,hash,&writer));change.writer=writer;change.fired=0;
 assert(sqlite3_trace_v2(db,SQLITE_TRACE_STMT,concurrent,&change)==SQLITE_OK);sql(db,"BEGIN");
 assert(wena_sqlite_member_roster_read(db,"b",roster)&&change.fired&&roster->board_version==3&&roster->members[0].active);
 assert(!strcmp(roster->names[0],"Repeated"));assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->card_version==7&&people->fields[0].count==2);
 sql(db,"COMMIT");assert(sqlite3_trace_v2(db,0,NULL,NULL)==SQLITE_OK);assert(sqlite3_close(writer)==SQLITE_OK);
 sql(db,"BEGIN");assert(wena_sqlite_member_roster_read(db,"b",roster)&&roster->board_version==4&&!roster->members[0].active&&!strcmp(roster->names[0],"Changed"));
 assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->card_version==8&&people->fields[0].count==1&&!people->fields[1].count);sql(db,"COMMIT");
 /* Independent per-field capacity, full rosters and one extra row fail closed. */
 sql(db,"BEGIN;DELETE FROM card_people;DELETE FROM board_members");
 for(i=0;i<=WENA_BOARD_MEMBER_CAPACITY;++i){sprintf(query,"INSERT INTO actors VALUES('p%04lu','Person',1)",(unsigned long)i);sql(db,query);}
 for(i=0;i<WENA_BOARD_MEMBER_CAPACITY;++i){
  sprintf(query,"INSERT INTO board_members(board_id,actor_id) VALUES('b','p%04lu')",(unsigned long)i);sql(db,query);
  sprintf(query,"INSERT INTO card_people VALUES('b','c','members','p%04lu',%lu),('b','c','assignees','p%04lu',%lu)",(unsigned long)i,(unsigned long)i,(unsigned long)i,(unsigned long)i);sql(db,query);
 }
 assert(wena_sqlite_member_roster_read(db,"b",roster)&&roster->count==2048);
 assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->fields[0].count==2048&&people->fields[1].count==2048);
 *saved_roster=*roster;*saved_people=*people;
 sql(db,"INSERT INTO board_members(board_id,actor_id) VALUES('b','p2048');INSERT INTO card_people VALUES('b','c','members','p2048',2048)");
 assert(!wena_sqlite_member_roster_read(db,"b",roster)&&!memcmp(roster,saved_roster,sizeof(*roster)));
 assert(!wena_sqlite_card_people_read(db,"b","c",people)&&!memcmp(people,saved_people,sizeof(*people)));sql(db,"ROLLBACK");
 assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
 assert(wena_sqlite_open(argv[2],bundle,(size_t)length,hash,&db));sql(db,"BEGIN");
 assert(wena_sqlite_card_people_read(db,"b","c",people)&&people->card_version==8&&people->fields[0].count==1);
 assert(wena_sqlite_member_roster_read(db,"b",roster)&&roster->count==2);sql(db,"COMMIT");assert(sqlite3_close(db)==SQLITE_OK);
 assert(sqlite3_open_v2(argv[2],&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);sql(db,"BEGIN");
 assert(wena_sqlite_member_roster_read(db,"b",roster)&&wena_sqlite_card_people_read(db,"b","c",people));
 *saved_roster=*roster;*saved_people=*people;
 assert(!wena_sqlite_member_roster_read(db,"bad/id",roster)&&!memcmp(roster,saved_roster,sizeof(*roster)));
 assert(!wena_sqlite_card_people_read(db,"b","missing",people)&&!memcmp(people,saved_people,sizeof(*people)));
 assert(!wena_sqlite_member_roster_read(NULL,"b",roster)&&!wena_sqlite_card_people_read(NULL,"b","c",people));
 sql(db,"COMMIT");assert(sqlite3_close(db)==SQLITE_OK);
 free(bundle);free(roster);free(people);free(saved_roster);free(saved_people);
 puts("Strict person snapshots, failed outputs, WAL consistency and capacity passed");return 0;
}
