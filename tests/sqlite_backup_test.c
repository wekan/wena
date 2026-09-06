#include "../server/sqlite_backup.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *readall(const char*p,size_t*n){FILE*f=fopen(p,"rb");long z;unsigned char*b;assert(f);assert(fseek(f,0,SEEK_END)==0);z=ftell(f);assert(z>0);rewind(f);b=(unsigned char*)malloc((size_t)z);assert(b);assert(fread(b,1,(size_t)z,f)==(size_t)z);assert(fclose(f)==0);*n=(size_t)z;return b;}
static int space(void*c,const char*p,unsigned long*b){(void)p;*b=*(unsigned long*)c;return 1;}
static int count(sqlite3*d,const char*q){sqlite3_stmt*s;int n;assert(sqlite3_prepare_v2(d,q,-1,&s,NULL)==SQLITE_OK);assert(sqlite3_step(s)==SQLITE_ROW);n=sqlite3_column_int(s,0);sqlite3_finalize(s);return n;}
int main(int ac,char**av){static const char h[]="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";unsigned char*sql;size_t n;char dbp[512],bak[512],side[528],tmp[528];sqlite3*d,*copy;unsigned long room=(unsigned long)-1;FILE*f;char sum[66];assert(ac==3);sql=readall(av[1],&n);sprintf(dbp,"%s/live.sqlite",av[2]);sprintf(bak,"%s/backup.sqlite",av[2]);sprintf(side,"%s.sha256",bak);assert(wena_sqlite_open(dbp,sql,n,h,&d));assert(sqlite3_exec(d,"INSERT INTO actors VALUES('u1','User',1);",NULL,NULL,NULL)==SQLITE_OK);assert(wena_sqlite_backup_create(d,bak,space,&room));assert(!wena_sqlite_backup_create(d,bak,space,&room));assert(sqlite3_open_v2(bak,&copy,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);assert(wena_sqlite_integrity(copy));assert(count(copy,"SELECT count(*) FROM actors")==1);sqlite3_close(copy);f=fopen(side,"rb");assert(f);assert(fread(sum,1,65,f)==65);sum[65]='\0';assert(sum[64]=='\n');fclose(f);sprintf(tmp,"%s.tmp",bak);remove(bak);remove(side);f=fopen(tmp,"wb");assert(f);fputs("interrupted",f);fclose(f);assert(!wena_sqlite_backup_create(d,bak,space,&room));remove(tmp);room=1;assert(!wena_sqlite_backup_create(d,bak,space,&room));assert(!wena_sqlite_backup_create(d,"relative.sqlite",space,&room));sqlite3_close(d);free(sql);return 0;}
