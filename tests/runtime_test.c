#include "../server/runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Clock{unsigned int entropy;unsigned long now;}Clock;
static int entropy(void*x,unsigned char*out,size_t n){Clock*c=x;size_t i;c->entropy++;for(i=0;i<n;i++)out[i]=(unsigned char)(c->entropy+i);return 1;}
static unsigned long now(void*x){return ((Clock*)x)->now;}
static unsigned char*readall(const char*p,size_t*n){FILE*f=fopen(p,"rb");long z;unsigned char*b;assert(f);fseek(f,0,SEEK_END);z=ftell(f);fseek(f,0,SEEK_SET);b=malloc((size_t)z);assert(b);fread(b,1,(size_t)z,f);fclose(f);*n=(size_t)z;return b;}
int main(int ac,char**av){static const char hash[]="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";WenaServerRuntime r;WenaServerSettings s;WenaSecurityStore security;Clock c;unsigned char*sql;size_t n;char root[80],db[512];unsigned int port;int ok=0;assert(ac==3);memset(&c,0,sizeof(c));c.now=1;sql=readall(av[1],&n);sprintf(db,"%s/runtime.sqlite",av[2]);wena_server_runtime_init(&r);wena_server_settings_init(&s);wena_security_init(&security,entropy,&c);assert(!wena_server_runtime_start(&r,&s,db,sql,n,"wrong",&security,now,&c));assert(!r.running&&r.database==NULL&&!r.listener.open);for(port=39300;port<39400&&!ok;port++){sprintf(root,"http://127.0.0.1:%u/base",port);assert(wena_server_settings_apply(&s,1,"127.0.0.1",port,root));ok=wena_server_runtime_start(&r,&s,db,sql,n,hash,&security,now,&c);}assert(ok&&r.running&&r.database&&r.listener.open);wena_server_runtime_stop(&r,&s);assert(!r.running&&r.database==NULL&&!r.listener.open);assert(wena_server_runtime_start(&r,&s,db,sql,n,hash,&security,now,&c));wena_server_runtime_stop(&r,&s);free(sql);return 0;}
