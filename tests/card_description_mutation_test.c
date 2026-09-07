#include "../client/features/card_description_mutation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sql(sqlite3*d,const char*q){assert(sqlite3_exec(d,q,NULL,NULL,NULL)==SQLITE_OK);}
static int number(sqlite3*d,const char*q){sqlite3_stmt*s;int n;assert(sqlite3_prepare_v2(d,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
static char *readall(const char*p){FILE*f;long n;char*s;f=fopen(p,"rb");assert(f);assert(!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0);rewind(f);s=(char*)malloc((size_t)n+1);assert(s);assert(fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);return s;}
static void command(WenaDomainCommand*c,unsigned long version,unsigned long request,const char*body){memset(c,0,sizeof(*c));c->operation=WENA_DOMAIN_EDIT_CARD_DESCRIPTION;c->request_version=request;strcpy(c->user_id,"u");strcpy(c->route,"/b/b/native");sprintf(c->form_body,"cardId=c&expectedVersion=%lu&%s",version,body);c->form_body_length=strlen(c->form_body);}
int main(int argc,char**argv)
{
    sqlite3*d;char *schema;WenaCardDescriptionMutation a,other;WenaDomainCommand c;WenaRegionResponse response;
    char text[WENA_DESCRIPTION_CAPACITY],oversized[WENA_DESCRIPTION_CAPACITY+1];
    const char *multiline=" \tFirst & + %= \303\244\r\nSecond\n ";
    const char *bad[]={"", "description=%", "description=%GG", "description=%00", "description=%0B", "description=%7F", "description=%C2%80", "description=%C0%AF", "description=%ED%A0%80", "description=one&description=two"};
    unsigned long version;size_t i;
    assert(argc==4);assert(sqlite3_open(argv[3],&d)==SQLITE_OK);sql(d,"PRAGMA foreign_keys=ON");schema=readall(argv[1]);sql(d,schema);free(schema);sql(d,"PRAGMA user_version=1");
    sql(d,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO boards VALUES('other','Other',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1);");
    assert(wena_card_description_mutation_init(&a,d,"u","b"));
    strcpy(text,"Unchanged");version=99;
    assert(!wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));
    assert(!strcmp(text,"Unchanged")&&version==99);
    assert(!wena_card_description_mutation_save(&a,"b","c",1,"Requires schema 2"));
    assert(number(d,"SELECT version FROM cards")==1&&number(d,"SELECT count(*) FROM idempotency_keys")==0);
    /* This is the exact isolated v2 additive fixture, not a claim that the main
     * application's migration chain already knows how to upgrade to v2. */
    schema=readall(argv[2]);sql(d,schema);free(schema);sql(d,"PRAGMA user_version=2");
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!text[0]&&version==1);
    assert(wena_model_description_valid("",0));assert(wena_model_description_valid(multiline,strlen(multiline)));
    assert(!wena_model_description_valid("a\0b",3));assert(!wena_model_title_valid("x\nx",3,129));
    for(i=0;i<sizeof(bad)/sizeof(bad[0]);++i){command(&c,1,40,bad[i]);assert(!wena_sqlite_persistence_apply(&a.persistence,&c,&response));}
    memset(oversized,'x',sizeof(oversized));oversized[sizeof(oversized)-1]=0;
    assert(!wena_card_description_mutation_save(&a,"b","c",1,oversized));
    assert(!wena_card_description_mutation_save(&a,"b","c",1,"\001"));
    assert(!wena_card_description_mutation_save(&a,"b","c",1,"\300\257"));
    assert(!wena_card_description_mutation_save(&a,"b","c",0,"Bad version"));
    assert(!wena_card_description_mutation_save_request(&a,"b","c",1,0,"Bad request"));
    assert(!wena_card_description_mutation_save(&a,"other","c",1,"Wrong board"));
    assert(!wena_card_description_mutation_save(&a,"b","missing",1,"Missing"));
    assert(wena_card_description_mutation_init(&other,d,"unknown","b"));
    assert(!wena_card_description_mutation_load(&other,"b","c",text,sizeof(text),&version));
    assert(!wena_card_description_mutation_save(&other,"b","c",1,"Unknown actor"));
    sql(d,"CREATE TRIGGER reject_description BEFORE INSERT ON card_descriptions BEGIN SELECT RAISE(ABORT,'description'); END;");
    assert(!wena_card_description_mutation_save(&a,"b","c",1,"Rollback"));sql(d,"DROP TRIGGER reject_description");
    assert(number(d,"SELECT version FROM cards")==1&&number(d,"SELECT count(*) FROM card_descriptions")==0);
    assert(wena_card_description_mutation_save_request(&a,"b","c",1,40,multiline));
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!strcmp(text,multiline)&&version==2);
    strcpy(text,"Untouched");version=99;
    assert(!wena_card_description_mutation_load(&a,"b","c",text,2,&version));assert(!strcmp(text,"Untouched")&&version==99);
    assert(!wena_card_description_mutation_save(&a,"b","c",1,"Stale"));
    assert(!wena_card_description_mutation_save_request(&a,"b","c",2,40,"Replay"));
    command(&c,2,41,"description=Scope");strcpy(c.route,"/b/other/native");assert(!wena_sqlite_persistence_apply(&a.persistence,&c,&response));
    sql(d,"UPDATE cards SET archived=1");assert(!wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!wena_card_description_mutation_save(&a,"b","c",2,"Archived"));sql(d,"UPDATE cards SET archived=0");
    sql(d,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'metadata'); END;");
    assert(!wena_card_description_mutation_save(&a,"b","c",2,"Rollback"));sql(d,"DROP TRIGGER reject_metadata");
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!strcmp(text,multiline)&&version==2);
    assert(number(d,"SELECT count(*) FROM idempotency_keys")==1);
    assert(wena_card_description_mutation_save(&a,"b","c",2,""));
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!text[0]&&version==3);
    memset(text,'x',sizeof(text)-1);text[sizeof(text)-1]=0;assert(wena_card_description_mutation_save(&a,"b","c",3,text));
    sql(d,"UPDATE cards SET title='Other mutation',version=version+1");
    assert(!wena_card_description_mutation_save(&a,"b","c",4,"Cross-field stale"));
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(strlen(text)==1024&&version==5);
    assert(sqlite3_close(d)==SQLITE_OK);assert(sqlite3_open(argv[3],&d)==SQLITE_OK);sql(d,"PRAGMA foreign_keys=ON");
    assert(number(d,"PRAGMA user_version")==2);assert(wena_card_description_mutation_init(&a,d,"u","b"));
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(strlen(text)==1024&&version==5);
    assert(!wena_card_description_mutation_save_request(&a,"b","c",5,40,"Old replay"));
    assert(wena_card_description_mutation_save(&a,"b","c",5,"Reopened"));
    assert(number(d,"SELECT max(request_version) FROM idempotency_keys")==43);
    sql(d,"UPDATE card_descriptions SET description=char(1)");strcpy(text,"Unchanged");version=99;
    assert(!wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!strcmp(text,"Unchanged")&&version==99);
    sql(d,"UPDATE card_descriptions SET description='Reopened'");
    command(&c,6,44,"description=a%0Db%09c%0Ad");assert(wena_sqlite_persistence_apply(&a.persistence,&c,&response));
    assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(!strcmp(text,"a\rb\tc\nd")&&version==7);
    c.operation=WENA_DOMAIN_EDIT_CARD_TITLE;strcpy(c.form_body,"cardId=c&expectedVersion=7&title=Bad%0ATitle");c.form_body_length=strlen(c.form_body);c.request_version=45;
    assert(!wena_sqlite_persistence_apply(&a.persistence,&c,&response));assert(number(d,"SELECT version FROM cards")==7);
    assert(sqlite3_close(d)==SQLITE_OK);puts("card description mutation tests passed (isolated v2 fixture)");return 0;
}
