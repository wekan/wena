#include "../server/embedded_migration.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void copy(const char*a,const char*b,int truncate,int corrupt){FILE*i=fopen(a,"rb"),*o=fopen(b,"wb");long n;size_t z;unsigned char*x;assert(i&&o);fseek(i,0,SEEK_END);n=ftell(i);fseek(i,0,SEEK_SET);x=malloc((size_t)n);assert(x&&fread(x,1,(size_t)n,i)==(size_t)n);z=((size_t)x[n-20]<<24)|((size_t)x[n-19]<<16)|((size_t)x[n-18]<<8)|x[n-17];if(corrupt)x[n-56-(long)z-1]^=1;if(truncate)n--;assert(fwrite(x,1,(size_t)n,o)==(size_t)n);free(x);fclose(i);fclose(o);}
int main(int ac,char**av){WenaEmbeddedMigration m;char truncated[512],corrupt[512];assert(ac==3);assert(wena_embedded_migration_load(av[1],&m));assert(m.length>100&&strlen(m.sha256)==64);wena_embedded_migration_free(&m);assert(!wena_embedded_migration_load("/missing/wena",&m));sprintf(truncated,"%s/truncated",av[2]);sprintf(corrupt,"%s/corrupt",av[2]);copy(av[1],truncated,1,0);copy(av[1],corrupt,0,1);assert(!wena_embedded_migration_load(truncated,&m));assert(!wena_embedded_migration_load(corrupt,&m));return 0;}
