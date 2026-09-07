#include "../client/features/card_description.h"
#include "../client/features/card_description_mutation.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void sql(sqlite3*d,const char*q){assert(sqlite3_exec(d,q,NULL,NULL,NULL)==SQLITE_OK);}
static void schema(sqlite3*d,const char*p){FILE*f;long n;char*s;f=fopen(p,"rb");assert(f);assert(!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0);rewind(f);s=(char*)malloc((size_t)n+1);assert(s);assert(fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);sql(d,s);free(s);}
static void frame(WenaCardDescriptionState*s,WenaCard*c,const char*button,const char*text)
{struct nk_context n;memset(&n,0,sizeof(n));n.button_to_press=button;n.edit_text=text;assert(wena_card_description_render(&n,s,c,1,800,600));}
int main(int argc,char**argv)
{
 sqlite3*d;WenaCardDescriptionMutation a;WenaCardDescriptionState s;WenaCard c;char text[WENA_DESCRIPTION_CAPACITY];unsigned long version;
 assert(argc==4);assert(sqlite3_open(argv[3],&d)==SQLITE_OK);sql(d,"PRAGMA foreign_keys=ON");schema(d,argv[1]);schema(d,argv[2]);sql(d,"PRAGMA user_version=2");
 sql(d,"INSERT INTO actors VALUES('u','User',1);INSERT INTO boards VALUES('b','Board',1);INSERT INTO lists VALUES('l','b','List',0,1);INSERT INTO swimlanes VALUES('s','b','Lane',0,1);INSERT INTO cards VALUES('c','b','s','l','Card',0,0,1)");
 assert(wena_card_init(&c,"c","b","s","l","Card",0,0));assert(wena_card_description_mutation_init(&a,d,"u","b"));
 wena_card_description_init(&s,wena_card_description_mutation_load,wena_card_description_mutation_save,&a);
 assert(wena_card_description_open(&s,&c));assert(!s.length&&s.version==1);frame(&s,&c,"Cancel","Discard");
 assert(wena_card_description_open(&s,&c));assert(!s.length&&s.version==1);frame(&s,&c,"Save","  # Plain\nText\t& + %= \303\244\r\n ");assert(!s.visible);
 assert(wena_card_description_open(&s,&c));assert(s.version==2&&!strcmp(s.input,"  # Plain\nText\t& + %= \303\244\r\n "));
 sql(d,"UPDATE cards SET title='Concurrent',version=version+1");frame(&s,&c,"Save","Stale");assert(s.visible&&s.error&&!strcmp(s.input,"Stale"));frame(&s,&c,"Cancel",NULL);
 assert(wena_card_description_open(&s,&c));assert(s.version==3);sql(d,"CREATE TRIGGER reject_metadata BEFORE INSERT ON idempotency_keys BEGIN SELECT RAISE(ABORT,'rollback'); END");frame(&s,&c,"Save","Retry\nRetained");assert(s.error&&s.visible&&!strcmp(s.input,"Retry\nRetained"));
 assert(wena_card_description_mutation_load(&a,"b","c",text,sizeof(text),&version));assert(version==3&&!strcmp(text,"  # Plain\nText\t& + %= \303\244\r\n "));sql(d,"DROP TRIGGER reject_metadata");frame(&s,&c,"Save",NULL);assert(!s.visible);
 assert(!wena_card_description_mutation_save_request(&a,"b","c",4,1,"Replay"));
 assert(wena_card_description_open(&s,&c));assert(s.version==4);frame(&s,&c,"Save","");assert(!s.visible);
 strcpy(c.board_id,"wrong");assert(!wena_card_description_open(&s,&c));strcpy(c.board_id,"b");
 assert(wena_card_description_open(&s,&c));frame(&s,&c,"Save","Persisted\nDescription");assert(!s.visible);
 assert(sqlite3_close(d)==SQLITE_OK);assert(sqlite3_open(argv[3],&d)==SQLITE_OK);assert(wena_card_description_mutation_init(&a,d,"u","b"));
 assert(wena_card_description_open(&s,&c));assert(s.version==6&&!strcmp(s.input,"Persisted\nDescription"));frame(&s,&c,"Cancel",NULL);assert(sqlite3_close(d)==SQLITE_OK);
 puts("Description editor SQLite save, clear, stale, rollback, replay, scope and reopen passed");return 0;
}
