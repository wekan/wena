#include "server_settings.h"
#include <string.h>
int wena_server_settings_form_activate(WenaServerSettingsForm *form,WenaServerSettings *settings,WenaServerRuntime *runtime,const char *database_path,const char *executable_path,WenaSecurityStore *security,WenaHttpNow now,void *context)
{
    if(!form||!settings||!runtime||!wena_server_settings_form_submit(form,settings))return 0;
    if(runtime->running)wena_server_runtime_stop(runtime,NULL);
    if(!settings->enabled){wena_server_settings_stopped(settings);return 1;}
    if(!wena_server_runtime_start_executable(runtime,settings,database_path,executable_path,security,now,context)){wena_server_settings_error(settings,"Server failed to start");return 0;}
    return 1;
}

static void audit(WenaAdminStorageState*s,const char*event){unsigned int i;if(s->audit_count==WENA_ADMIN_STORAGE_AUDIT_MAX){for(i=1;i<s->audit_count;i++)strcpy(s->audit[i-1],s->audit[i]);s->audit_count--;}strcpy(s->audit[s->audit_count++],event);s->sequence++;}
static int fail(WenaAdminStorageState*s,const char*event){s->status=WENA_ADMIN_STORAGE_ERROR;strcpy(s->error,"Storage operation failed");audit(s,event);return 0;}
void wena_admin_storage_init(WenaAdminStorageState*s){if(s)memset(s,0,sizeof(*s));}
int wena_admin_storage_backup(WenaAdminStorageState*s,WenaServerRuntime*r,const char*p,WenaBackupFreeSpace f,void*c){if(!s||s->status==WENA_ADMIN_STORAGE_BUSY||!r||!r->running||!r->database)return 0;s->status=WENA_ADMIN_STORAGE_BUSY;s->error[0]='\0';audit(s,"backup-start");if(!wena_sqlite_backup_create(r->database,p,f,c))return fail(s,"backup-rejected");s->status=WENA_ADMIN_STORAGE_SUCCESS;audit(s,"backup-complete");return 1;}
typedef struct RestoreContext{WenaServerRuntime*r;WenaServerSettings*s;const char*db;WenaEmbeddedMigration*m;WenaSecurityStore*security;WenaHttpNow now;void*clock;}RestoreContext;
static int restore_stop(void*x){RestoreContext*c=(RestoreContext*)x;wena_server_runtime_stop(c->r,c->s);return !c->r->running;}
static int restore_start(void*x,const char*p){RestoreContext*c=(RestoreContext*)x;(void)p;return wena_server_runtime_start(c->r,c->s,c->db,c->m->bytes,c->m->length,c->m->sha256,c->security,c->now,c->clock);}
int wena_admin_storage_restore(WenaAdminStorageState*state,WenaServerRuntime*r,WenaServerSettings*s,const char*backup,const char*db,const char*exe,WenaBackupFreeSpace f,void*space,WenaSecurityStore*security,WenaHttpNow now,void*clock){WenaEmbeddedMigration m;RestoreContext c;WenaRestoreLifecycle life;int ok;if(!state||state->status==WENA_ADMIN_STORAGE_BUSY||!r||!r->running||!s||!s->enabled)return 0;state->status=WENA_ADMIN_STORAGE_BUSY;state->error[0]='\0';audit(state,"restore-start");if(!wena_embedded_migration_load(exe,&m))return fail(state,"restore-rejected");c.r=r;c.s=s;c.db=db;c.m=&m;c.security=security;c.now=now;c.clock=clock;life.stop=restore_stop;life.start=restore_start;life.context=&c;ok=wena_sqlite_restore(backup,db,m.sha256,f,space,&life);wena_embedded_migration_free(&m);if(!ok)return fail(state,"restore-rolled-back");state->status=WENA_ADMIN_STORAGE_SUCCESS;audit(state,"restore-complete");return 1;}
