#include "../server/mutations/card_people.h"
#include "../server/sqlite_storage.h"
#include "../server/sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3 *db,const char *s)
{char *error=NULL;int result;result=sqlite3_exec(db,s,NULL,NULL,&error);if(result!=SQLITE_OK)fprintf(stderr,"%s: %s\n",s,error);sqlite3_free(error);assert(result==SQLITE_OK);}
static void capture(sqlite3 *db,WenaMemberRoster *r,WenaCardPeopleSnapshot *c)
{assert(wena_sqlite_member_roster_read(db,"b",r)&&wena_sqlite_card_people_read(db,"b","c",c));}
static int commit_fail(void *unused){(void)unused;return 1;}
static void rejects(sqlite3 *db,const char *setup,int field,const char *actor,int enabled,
 WenaMemberRoster *r,WenaCardPeopleSnapshot *c,WenaCardPeopleSnapshot *out)
{
 WenaCardPeopleSnapshot *before;WenaMemberRoster *before_r;size_t i;
 before=(WenaCardPeopleSnapshot*)malloc(sizeof(*before));before_r=(WenaMemberRoster*)malloc(sizeof(*before_r));assert(before&&before_r);
 sql(db,"BEGIN");capture(db,r,c);*before=*c;*before_r=*r;sql(db,setup);memset(out,0x5a,sizeof(*out));
 assert(!wena_sqlite_card_person_set(db,r,c,field,actor,enabled,out));
 for(i=0;i<sizeof(*out);++i)assert(((unsigned char*)out)[i]==0x5a);
 assert(!memcmp(c,before,sizeof(*c))&&!memcmp(r,before_r,sizeof(*r))&&!sqlite3_get_autocommit(db));sql(db,"ROLLBACK");
 sql(db,"BEGIN");capture(db,r,c);assert(!memcmp(c,before,sizeof(*c))&&!memcmp(r,before_r,sizeof(*r)));sql(db,"COMMIT");
 free(before);free(before_r);
}
static void reboard_cases(sqlite3 *db,WenaCardPeopleSnapshot *before,WenaCardPeopleSnapshot *after)
{
 size_t i;int changes;char query[512];
 const char *triggers[]={
  "CREATE TRIGGER fail_reboard BEFORE DELETE ON card_people BEGIN SELECT RAISE(IGNORE);END",
  "CREATE TRIGGER fail_reboard BEFORE UPDATE OF board_id ON card_people BEGIN SELECT RAISE(IGNORE);END",
  "CREATE TRIGGER fail_reboard AFTER UPDATE OF board_id ON card_people BEGIN UPDATE cards SET title='Changed' WHERE id='c';END",
  "CREATE TRIGGER fail_reboard AFTER DELETE ON card_people BEGIN UPDATE board_members SET active=0 WHERE board_id='other' AND actor_id='a';END"
 };
 sql(db,"INSERT INTO lists VALUES('ol','other','Other list',0,1);INSERT INTO swimlanes VALUES('os','other','Other lane',0,1);"
  "INSERT INTO board_members(board_id,actor_id,active) VALUES('other','a',1),('other','z',0)");
 assert(!wena_sqlite_card_people_reboard(db,"b","c","other"));
 sql(db,"BEGIN");assert(wena_sqlite_card_people_read(db,"b","c",before));
 assert(!wena_sqlite_card_people_reboard(db,"b","c","b"));
 assert(!wena_sqlite_card_people_reboard(db,"b","c","missing"));
 assert(wena_sqlite_card_people_reboard(db,"b","c","other"));
 assert(!wena_sqlite_card_people_read(db,"b","c",after));
 assert(wena_sqlite_card_people_read_staged(db,"b","c","other",after));
 assert(after->card_version==before->card_version&&after->fields[0].count==1&&after->positions[0][0]==6);
 assert(!strcmp(after->fields[0].ids[0],"a")&&after->fields[1].count==2&&after->positions[1][0]==9&&after->positions[1][1]==10);
 assert(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK&&!sqlite3_get_autocommit(db));sql(db,"ROLLBACK");
 for(i=0;i<sizeof(triggers)/sizeof(triggers[0]);++i){
  sql(db,"BEGIN");sql(db,triggers[i]);assert(!wena_sqlite_card_people_reboard(db,"b","c","other"));sql(db,"ROLLBACK");
  sql(db,"BEGIN");assert(wena_sqlite_card_people_read(db,"b","c",after)&&!memcmp(before,after,sizeof(*before)));sql(db,"COMMIT");
 }
 sql(db,"BEGIN");assert(wena_sqlite_card_people_reboard(db,"b","c","other"));
 sql(db,"UPDATE cards SET board_id='other',list_id='ol',swimlane_id='os',version=version+1 WHERE id='c';COMMIT");
 sql(db,"BEGIN");assert(wena_sqlite_card_people_read(db,"other","c",after)&&after->card_version==11&&after->fields[0].count==1&&after->fields[1].count==2);sql(db,"COMMIT");
 sql(db,"BEGIN;INSERT INTO cards VALUES('empty','b','s','l','Empty',1,0,1)");
 assert(wena_sqlite_card_people_reboard(db,"b","empty","other"));
 sql(db,"UPDATE cards SET board_id='other',list_id='ol',swimlane_id='os' WHERE id='empty'");
 assert(wena_sqlite_card_people_read(db,"other","empty",after)&&!after->fields[0].count&&!after->fields[1].count);sql(db,"ROLLBACK");
 /* Both full person fields survive; target roster overflow fails before writes. */
 sql(db,"BEGIN;DELETE FROM board_members WHERE board_id='other';INSERT INTO cards VALUES('full','b','s','l','Full',1,0,1)");
 for(i=0;i<2048;++i){
  sprintf(query,"INSERT INTO actors VALUES('q%04lu','Person',1);INSERT INTO board_members(board_id,actor_id) VALUES('other','q%04lu')",(unsigned long)i,(unsigned long)i);sql(db,query);
  sprintf(query,"INSERT INTO card_people VALUES('b','full','members','q%04lu',%lu),('b','full','assignees','q%04lu',%lu)",(unsigned long)i,(unsigned long)i,(unsigned long)i,(unsigned long)i);sql(db,query);
 }
 sql(db,"INSERT INTO board_members(board_id,actor_id) VALUES('other','a')");changes=sqlite3_total_changes(db);
 assert(!wena_sqlite_card_people_reboard(db,"b","full","other")&&sqlite3_total_changes(db)==changes);
 sql(db,"DELETE FROM board_members WHERE board_id='other' AND actor_id='a'");
 assert(wena_sqlite_card_people_reboard(db,"b","full","other"));
 sql(db,"UPDATE cards SET board_id='other',list_id='ol',swimlane_id='os' WHERE id='full'");
 assert(wena_sqlite_card_people_read(db,"other","full",after)&&after->fields[0].count==2048&&after->fields[1].count==2048&&after->positions[1][2047]==2047);
 sql(db,"ROLLBACK");assert(wena_sqlite_integrity(db));
}
int main(int argc,char **argv)
{
 FILE *file;long length;unsigned char *bundle;char hash[65],query[512];sqlite3 *db;
 WenaMemberRoster *r;WenaCardPeopleSnapshot *c,*out,*before;size_t i;int changes;
 assert(argc==3);file=fopen(argv[1],"rb");assert(file&&!fseek(file,0,SEEK_END));length=ftell(file);assert(length>0);rewind(file);
 bundle=(unsigned char*)malloc((size_t)length);assert(bundle&&fread(bundle,1,(size_t)length,file)==(size_t)length&&!fclose(file));
 wena_sha256_hex(bundle,(size_t)length,hash);assert(wena_sqlite_open(argv[2],bundle,(size_t)length,hash,&db));
 r=(WenaMemberRoster*)malloc(sizeof(*r));c=(WenaCardPeopleSnapshot*)malloc(sizeof(*c));
 out=(WenaCardPeopleSnapshot*)malloc(sizeof(*out));before=(WenaCardPeopleSnapshot*)malloc(sizeof(*before));assert(r&&c&&out&&before);
 sql(db,"INSERT INTO boards VALUES('b','Board',3),('other','Other',1);"
  "INSERT INTO actors VALUES('a','Alice',1),('z','Zed',1),('i','Inactive',1),('f','Foreign',1);"
  "INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);"
  "INSERT INTO cards VALUES('c','b','s','l','Card',0,0,7)");
 sql(db,"INSERT INTO board_members(board_id,actor_id,active) VALUES('b','a',1),('b','z',1),('b','i',0),('other','f',1);"
  "INSERT INTO card_people VALUES('b','c','members','z',5),('b','c','members','i',8),('b','c','assignees','z',9)");
 rejects(db,"SELECT 1",0,"i",1,r,c,out);rejects(db,"SELECT 1",0,"f",1,r,c,out);
 rejects(db,"SELECT 1",0,"missing",1,r,c,out);rejects(db,"SELECT 1",2,"a",1,r,c,out);
 rejects(db,"SELECT 1",-1,"a",1,r,c,out);rejects(db,"SELECT 1",0,"a",2,r,c,out);
 rejects(db,"SELECT 1",0,"bad/id",1,r,c,out);
 rejects(db,"UPDATE boards SET version=version+1 WHERE id='b'",0,"a",1,r,c,out);
 rejects(db,"UPDATE cards SET version=version+1",0,"a",1,r,c,out);
 rejects(db,"UPDATE board_members SET active=0 WHERE actor_id='a'",0,"a",1,r,c,out);
 rejects(db,"UPDATE actors SET display_name='Changed' WHERE id='a'",0,"a",1,r,c,out);
 rejects(db,"UPDATE card_people SET position=6 WHERE actor_id='z' AND field='members'",0,"a",1,r,c,out);
 rejects(db,"INSERT INTO list_archive_state VALUES('l','b',1,10)",0,"a",1,r,c,out);
 rejects(db,"INSERT INTO swimlane_archive_state VALUES('s','b',1,10)",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person BEFORE INSERT ON card_people BEGIN SELECT RAISE(IGNORE);END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person BEFORE INSERT ON card_people BEGIN SELECT RAISE(ABORT,'late');END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person BEFORE UPDATE ON cards BEGIN SELECT RAISE(IGNORE);END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER INSERT ON card_people BEGIN DELETE FROM card_people WHERE actor_id='a';END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER INSERT ON card_people BEGIN DELETE FROM card_people WHERE field='assignees';END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER INSERT ON card_people BEGIN UPDATE board_members SET active=0 WHERE actor_id='a';END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER UPDATE ON cards BEGIN UPDATE actors SET display_name='Changed' WHERE id='a';END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER UPDATE ON cards BEGIN UPDATE cards SET title='Changed';END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER INSERT ON card_people BEGIN INSERT INTO list_archive_state VALUES('l','b',1,10);END",0,"a",1,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person BEFORE DELETE ON card_people BEGIN SELECT RAISE(IGNORE);END",0,"i",0,r,c,out);
 rejects(db,"CREATE TRIGGER fail_person AFTER DELETE ON card_people BEGIN UPDATE card_people SET position=7 WHERE field='members';END",0,"i",0,r,c,out);
 /* Guarded no-op changes neither SQLite rows nor revisions. */
 sql(db,"BEGIN");capture(db,r,c);changes=sqlite3_total_changes(db);
 assert(wena_sqlite_card_person_set(db,r,c,0,"z",1,out)==2&&!memcmp(c,out,sizeof(*c)));
 assert(wena_sqlite_card_person_set(db,r,c,1,"missing",0,out)==2&&!memcmp(c,out,sizeof(*c)));
 assert(sqlite3_total_changes(db)==changes);sql(db,"COMMIT");
 assert(!wena_sqlite_card_person_set(db,r,c,0,"a",1,out));
 sql(db,"BEGIN");capture(db,r,c);*before=*c;
 assert(wena_sqlite_card_person_set(db,r,c,0,"a",1,out)==1&&out->card_version==8&&out->board_version==3);
 assert(out->fields[0].count==3&&out->positions[0][2]==9&&!strcmp(out->fields[0].ids[2],"a"));
 assert(!memcmp(c,before,sizeof(*c)));sql(db,"ROLLBACK");
 sql(db,"BEGIN");capture(db,r,c);assert(!memcmp(c,before,sizeof(*c)));
 assert(wena_sqlite_card_person_set(db,r,c,1,"a",1,out)==1&&out->positions[1][1]==10);
 sqlite3_commit_hook(db,commit_fail,NULL);assert(sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK);sqlite3_commit_hook(db,NULL,NULL);
 assert(sqlite3_get_autocommit(db));sql(db,"BEGIN");capture(db,r,c);assert(!memcmp(c,before,sizeof(*c)));sql(db,"COMMIT");
 /* Removal of an inactive or departed member preserves other positions. */
 sql(db,"DELETE FROM board_members WHERE actor_id='i';BEGIN");capture(db,r,c);
 assert(wena_sqlite_card_person_set(db,r,c,0,c->fields[0].ids[1],0,c)==1&&c->card_version==8&&c->fields[0].count==1&&c->positions[0][0]==5);
 assert(wena_sqlite_card_person_set(db,r,c,0,"a",1,c)==1&&c->card_version==9&&c->positions[0][1]==6);
 assert(wena_sqlite_card_person_set(db,r,c,1,"a",1,c)==1&&c->card_version==10&&c->positions[1][1]==10);sql(db,"COMMIT");
 /* No-op still rejects terminal aggregate revisions and archived cards. */
 sql(db,"BEGIN");sprintf(query,"UPDATE cards SET version=%lu",WENA_VERSION_READ_MAX);sql(db,query);capture(db,r,c);
 assert(!wena_sqlite_card_person_set(db,r,c,0,"a",1,out));sql(db,"ROLLBACK");
 sql(db,"BEGIN");sprintf(query,"UPDATE boards SET version=%lu WHERE id='b'",WENA_VERSION_READ_MAX);sql(db,query);capture(db,r,c);
 assert(!wena_sqlite_card_person_set(db,r,c,0,"a",1,out));sql(db,"ROLLBACK");
 sql(db,"BEGIN;UPDATE cards SET archived=1");capture(db,r,c);assert(!wena_sqlite_card_person_set(db,r,c,0,"a",0,out));sql(db,"ROLLBACK");
 sql(db,"BEGIN;UPDATE card_people SET position=2147483647 WHERE actor_id='a' AND field='members'");capture(db,r,c);
 assert(!wena_sqlite_card_person_set(db,r,c,0,"f",1,out));
 sql(db,"INSERT INTO board_members(board_id,actor_id) VALUES('b','f')");capture(db,r,c);
 assert(!wena_sqlite_card_person_set(db,r,c,0,"f",1,out));assert(wena_sqlite_card_person_set(db,r,c,0,"a",1,out)==2);sql(db,"ROLLBACK");
 /* A full card may contain departed members; the new eligible ID must not
  * evict one. Removal then append restores the full set in stable order. */
 sql(db,"BEGIN;DELETE FROM card_people");
 for(i=0;i<2048;++i){sprintf(query,"INSERT INTO actors VALUES('p%04lu','Person',1);INSERT INTO card_people VALUES('b','c','members','p%04lu',%lu)",(unsigned long)i,(unsigned long)i,(unsigned long)i);sql(db,query);}
 capture(db,r,c);assert(c->fields[0].count==2048&&!wena_sqlite_card_person_set(db,r,c,0,"a",1,out));
 assert(wena_sqlite_card_person_set(db,r,c,0,c->fields[0].ids[0],0,c)==1&&c->fields[0].count==2047&&c->positions[0][0]==1);
 assert(wena_sqlite_card_person_set(db,r,c,0,"a",1,c)==1&&c->fields[0].count==2048&&c->positions[0][2047]==2048);sql(db,"ROLLBACK");
 assert(wena_sqlite_integrity(db)&&sqlite3_close(db)==SQLITE_OK);
 assert(wena_sqlite_open(argv[2],bundle,(size_t)length,hash,&db));sql(db,"BEGIN");capture(db,r,c);
 assert(c->card_version==10&&c->board_version==3&&c->fields[0].count==2&&c->fields[1].count==2);
 assert(c->positions[0][0]==5&&c->positions[0][1]==6&&c->positions[1][0]==9&&c->positions[1][1]==10);
 sql(db,"COMMIT");reboard_cases(db,before,out);assert(sqlite3_close(db)==SQLITE_OK);
 assert(wena_sqlite_open(argv[2],bundle,(size_t)length,hash,&db));sql(db,"BEGIN");
 assert(wena_sqlite_card_people_read(db,"other","c",out)&&out->fields[0].count==1&&out->fields[1].count==2);
 sql(db,"COMMIT");assert(sqlite3_close(db)==SQLITE_OK);
 free(bundle);free(r);free(c);free(out);free(before);
 puts("Shared person writes, exact guards, stable order, no-ops and rollback passed");return 0;
}
