#include "../server/sqlite_backup.h"
#include "../server/sqlite_restore.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Life{sqlite3*d;int stops,starts,fail_start;}Life;
static unsigned char*readall(const char*p,size_t*n){FILE*f=fopen(p,"rb");long z;unsigned char*b;assert(f);fseek(f,0,SEEK_END);z=ftell(f);rewind(f);b=malloc((size_t)z);assert(b);assert(fread(b,1,(size_t)z,f)==(size_t)z);fclose(f);*n=(size_t)z;return b;}
static int space(void*c,const char*p,unsigned long*b){(void)p;*b=*(unsigned long*)c;return 1;}
static int stop(void*x){Life*l=x;l->stops++;if(l->d){sqlite3_close(l->d);l->d=NULL;}return 1;}
static int start(void*x,const char*p){Life*l=x;l->starts++;if(l->fail_start){l->fail_start=0;return 0;}return sqlite3_open_v2(p,&l->d,SQLITE_OPEN_READWRITE,NULL)==SQLITE_OK;}
static int count(sqlite3*d,const char*q){sqlite3_stmt*s;int n;assert(sqlite3_prepare_v2(d,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
static void alter(const char*p){FILE*f=fopen(p,"r+b");assert(f);assert(fseek(f,100,SEEK_SET)==0);fputc(0xff,f);fclose(f);}
int main(int ac,char**av){static const char h[]="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";unsigned char*sql;size_t n;char live[512],bak[512],side[528];unsigned long room=(unsigned long)-1;sqlite3*d;Life l;WenaRestoreLifecycle lc;FILE*f;assert(ac==3);sql=readall(av[1],&n);sprintf(live,"%s/live.sqlite",av[2]);sprintf(bak,"%s/backup.sqlite",av[2]);sprintf(side,"%s.sha256",bak);assert(wena_sqlite_open(live,sql,n,h,&d));assert(sqlite3_exec(d,"INSERT INTO actors VALUES('old','Old',1);",NULL,NULL,NULL)==SQLITE_OK);assert(wena_sqlite_backup_create(d,bak,space,&room));assert(sqlite3_exec(d,"DELETE FROM actors;INSERT INTO actors VALUES('new','New',1);",NULL,NULL,NULL)==SQLITE_OK);l.d=d;l.stops=l.starts=l.fail_start=0;lc.stop=stop;lc.start=start;lc.context=&l;assert(wena_sqlite_restore(bak,live,h,space,&room,&lc));assert(l.stops==1&&l.starts==1&&count(l.d,"SELECT count(*) FROM actors WHERE id='old'")==1);sqlite3_close(l.d);l.d=NULL;
f=fopen(side,"wb");assert(f);fputs("wrong\n",f);fclose(f);assert(!wena_sqlite_restore(bak,live,h,space,&room,&lc));assert(l.stops==1);remove(bak);remove(side);assert(wena_sqlite_open(live,sql,n,h,&d));assert(wena_sqlite_backup_create(d,bak,space,&room));sqlite3_close(d);l.d=NULL;assert(sqlite3_open(bak,&d)==SQLITE_OK);assert(sqlite3_exec(d,"PRAGMA user_version=2",NULL,NULL,NULL)==SQLITE_OK);sqlite3_close(d);assert(!wena_sqlite_restore(bak,live,h,space,&room,&lc));remove(bak);remove(side);
assert(wena_sqlite_open(live,sql,n,h,&d));assert(wena_sqlite_backup_create(d,bak,space,&room));sqlite3_close(d);l.d=NULL;room=1;assert(!wena_sqlite_restore(bak,live,h,space,&room,&lc));room=(unsigned long)-1;l.d=NULL;assert(sqlite3_open(live,&l.d)==SQLITE_OK);l.fail_start=1;assert(!wena_sqlite_restore(bak,live,h,space,&room,&lc));assert(l.d&&count(l.d,"SELECT count(*) FROM actors WHERE id='old'")==1);assert(l.starts>=3);sqlite3_close(l.d);alter(bak);assert(!wena_sqlite_restore(bak,live,h,space,&room,&lc));free(sql);return 0;}
