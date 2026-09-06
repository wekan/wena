#include "embedded_migration.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long read32(const unsigned char*p){return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3];}
static int footer_size(const unsigned char*p,size_t*n){unsigned long hi=read32(p),lo=read32(p+4);if(hi!=0ul)return 0;*n=(size_t)lo;return (unsigned long)*n==lo;}
int wena_embedded_migration_load(const char *path,WenaEmbeddedMigration *m){FILE*f;long end,sql_end;unsigned char foot[56],actual[32];size_t i,i18n,sql_size;char hex[65];if(m)memset(m,0,sizeof(*m));if(!path||!m||(f=fopen(path,"rb"))==NULL)return 0;if(fseek(f,0,SEEK_END)!=0||(end=ftell(f))<112)goto bad;if(fseek(f,end-56,SEEK_SET)!=0||fread(foot,1,56,f)!=56||memcmp(foot+40,"WENA-I18N-END-v1",16)!=0||!footer_size(foot+32,&i18n)||(long)i18n>end-112)goto bad;sql_end=end-56-(long)i18n;if(fseek(f,sql_end-56,SEEK_SET)!=0||fread(foot,1,56,f)!=56||memcmp(foot+40,"WENA-SQL-END-v1!",16)!=0||!footer_size(foot+32,&sql_size)||(long)sql_size>sql_end-56)goto bad;memcpy(actual,foot,32);m->bytes=(unsigned char*)malloc(sql_size);if(!m->bytes)goto bad;if(fseek(f,sql_end-56-(long)sql_size,SEEK_SET)!=0||fread(m->bytes,1,sql_size,f)!=sql_size)goto bad;wena_sha256_hex(m->bytes,sql_size,hex);for(i=0;i<32;i++){static const char x[]="0123456789abcdef";if(hex[i*2]!=x[actual[i]>>4]||hex[i*2+1]!=x[actual[i]&15])goto bad;}m->length=sql_size;strcpy(m->sha256,hex);fclose(f);return 1;bad:if(f)fclose(f);wena_embedded_migration_free(m);return 0;}
void wena_embedded_migration_free(WenaEmbeddedMigration*m){if(m){free(m->bytes);memset(m,0,sizeof(*m));}}
