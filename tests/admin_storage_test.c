#include "support/test_files.h"
#include "../client/features/server_settings.h"
#include "../server/sqlite_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The migration is compiled in by the test script (tests/support/test_files.h). */
static unsigned char*readall(const char*p,size_t*n){long z;unsigned char*b=(unsigned char*)wena_test_file_copy(p,&z);*n=(size_t)z;return b;}
static int room(void*x,const char*p,unsigned long*b){(void)x;(void)p;*b=(unsigned long)-1;return 1;}
static unsigned long now(void*x){(void)x;return 1;}
int main(int ac,char**av){static const char h[]="e4760a2b70d6651ee84dce93642ccdd4ce8991b488dece5d231e66053f065da5";unsigned char*sql;size_t n;char db[512],bak[512];WenaServerRuntime r;WenaServerSettings s;WenaAdminStorageState a;WenaSecurityStore security;assert(ac==3);sql=readall(av[1],&n);sprintf(db,"%s/live.sqlite",av[2]);sprintf(bak,"%s/live.backup",av[2]);wena_server_runtime_init(&r);assert(wena_sqlite_open(db,sql,n,h,&r.database));r.running=1;wena_admin_storage_init(&a);assert(wena_admin_storage_backup(&a,&r,bak,room,NULL));assert(a.status==WENA_ADMIN_STORAGE_SUCCESS&&a.audit_count==2&&strcmp(a.audit[0],"backup-start")==0&&strcmp(a.audit[1],"backup-complete")==0);assert(!wena_admin_storage_backup(&a,&r,bak,room,NULL));assert(a.status==WENA_ADMIN_STORAGE_ERROR&&strcmp(a.error,"Storage operation failed")==0&&strcmp(a.audit[3],"backup-rejected")==0);wena_server_settings_init(&s);s.enabled=1;wena_security_init(&security,NULL,NULL);assert(!wena_admin_storage_restore(&a,&r,&s,bak,db,"/missing/exe",room,NULL,&security,now,NULL));assert(r.running&&strcmp(a.audit[a.audit_count-1],"restore-rejected")==0);sqlite3_close(r.database);free(sql);return 0;}
