/* Included by the shared all-prefix upgrade/rollback harness. */
static void people_reject(sqlite3 *db,const char *text)
{assert(sqlite3_exec(db,text,NULL,NULL,NULL)!=SQLITE_OK);}
static void people_tables(sqlite3 *db)
{
 const char *tables[]={"board_members","card_people"};
 const char *id_bad[]={"NULL","''","'bad/id'","'a'||char(0)","x'61'","printf('%065d',1)"};
 const char *int_bad[]={"NULL","-1","0.5","'bad'","x'31'","1e100"};
 const char *member_int[]={"active","version","created_at","updated_at"};
 const char *objects[]={"board_members","card_people","board_members_actor_idx","card_people_actor_idx","card_people_card_order_idx"};
 char q[512];size_t i,j;
 assert(number(db,"SELECT count(*) FROM card_people")==0);
 sql(db,"INSERT INTO actors VALUES('a','Alice',1),('z','Alice',2);"
        "INSERT INTO board_members(board_id,actor_id) VALUES('b','a'),('b','z');"
        "INSERT INTO board_members(board_id,actor_id) VALUES('other','a')");
 assert(number(db,"SELECT count(*) FROM board_members WHERE active=1 AND version=1 AND created_at=0 AND updated_at=0")==3);
 sql(db,"INSERT INTO card_people VALUES('b','c','members','a',0);"
        "INSERT INTO card_people VALUES('b','c','assignees','a',0)");
 for(i=0;i<2;++i)for(j=0;j<sizeof(id_bad)/sizeof(id_bad[0]);++j){
  sprintf(q,"UPDATE %s SET actor_id=%s WHERE actor_id='a'",tables[i],id_bad[j]);people_reject(db,q);
  sprintf(q,"UPDATE %s SET board_id=%s WHERE actor_id='a'",tables[i],id_bad[j]);people_reject(db,q);
 }
 for(i=0;i<sizeof(id_bad)/sizeof(id_bad[0]);++i){
  sprintf(q,"UPDATE card_people SET card_id=%s",id_bad[i]);people_reject(db,q);
 }
 for(i=0;i<4;++i)for(j=0;j<sizeof(int_bad)/sizeof(int_bad[0]);++j){
  sprintf(q,"UPDATE board_members SET %s=%s WHERE actor_id='a'",member_int[i],int_bad[j]);people_reject(db,q);
 }
 for(i=0;i<sizeof(int_bad)/sizeof(int_bad[0]);++i){
  sprintf(q,"UPDATE card_people SET position=%s",int_bad[i]);people_reject(db,q);
 }
 people_reject(db,"UPDATE board_members SET active=2");
 people_reject(db,"UPDATE board_members SET version=0");
 people_reject(db,"UPDATE board_members SET created_at=1");
 people_reject(db,"UPDATE card_people SET position=2147483648");
 people_reject(db,"UPDATE card_people SET field='Members'");
 people_reject(db,"UPDATE card_people SET field='watchers'");
 people_reject(db,"UPDATE card_people SET field='members'||char(0)");
 people_reject(db,"UPDATE card_people SET field=x'6d656d62657273'");
 people_reject(db,"UPDATE card_people SET field=NULL");
 people_reject(db,"UPDATE card_people SET board_id='other'");
 people_reject(db,"UPDATE card_people SET actor_id='missing'");
 people_reject(db,"INSERT INTO board_members(board_id,actor_id) VALUES('b','a')");
 people_reject(db,"INSERT INTO board_members(board_id,actor_id) VALUES('missing','a')");
 people_reject(db,"INSERT INTO board_members(board_id,actor_id) VALUES('b','missing')");
 people_reject(db,"INSERT INTO card_people VALUES('b','c','members','a',1)");
 people_reject(db,"INSERT INTO card_people VALUES('b','c','members','z',0)");
 people_reject(db,"INSERT INTO card_people VALUES('b','missing','members','a',1)");
 people_reject(db,"DELETE FROM cards WHERE id='c'");
 people_reject(db,"DELETE FROM actors WHERE id='a'");
 people_reject(db,"DELETE FROM boards WHERE id='other'");
 sql(db,"INSERT INTO card_people VALUES('b','c','members','z',2147483647)");
 assert(number(db,"SELECT position FROM card_people WHERE actor_id='z'")==2147483647);
 sql(db,"UPDATE board_members SET active=0,version=2,created_at=12,updated_at=13 WHERE actor_id='a'");
 people_reject(db,"UPDATE board_members SET updated_at=11 WHERE actor_id='a'");
 /* Removal of roster membership must not cascade away existing assignments. */
 sql(db,"DELETE FROM board_members WHERE actor_id='z'");
 assert(number(db,"SELECT count(*) FROM card_people WHERE actor_id='z'")==1);
 sql(db,"DELETE FROM card_people WHERE actor_id='z';DELETE FROM actors WHERE id='z'");
 assert(number(db,"SELECT count(*) FROM card_people WHERE actor_id='a'")==2);
 assert(number(db,"SELECT version FROM cards WHERE id='c'")==3);
 assert(number(db,"SELECT version FROM boards WHERE id='b'")==42);
 assert(number(db,"SELECT version FROM actors WHERE id='a'")==1);
 /* Every schema object is verified, including indexes used by scoped reads. */
 for(i=0;i<5;++i){
  sql(db,"BEGIN");sprintf(q,"DROP %s %s",i<2?"TABLE":"INDEX",objects[i]);sql(db,q);
  assert(!wena_sqlite_schema_validate(db,NULL));sql(db,"ROLLBACK");
  assert(wena_sqlite_schema_validate(db,NULL));
 }
 assert(wena_sqlite_integrity(db));
}
