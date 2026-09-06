#ifndef WENA_CLIENT_SERVER_SETTINGS_H
#define WENA_CLIENT_SERVER_SETTINGS_H

#include "../../server/settings.h"
#include "../../server/runtime.h"
#include "../../server/sqlite_backup.h"
#include "../../server/sqlite_restore.h"

typedef struct WenaServerSettingsForm {
    int enabled;
    char bind_ipv4[WENA_SERVER_IPV4_CAPACITY];
    unsigned int port;
    char root_url[WENA_SERVER_ROOT_URL_CAPACITY];
    char validation_error[WENA_SERVER_ERROR_CAPACITY];
} WenaServerSettingsForm;

void wena_server_settings_form_init(WenaServerSettingsForm *form,
                                    const WenaServerSettings *settings);
int wena_server_settings_form_submit(WenaServerSettingsForm *form,
                                     WenaServerSettings *settings);
const char *wena_server_status_name(WenaServerStatus status);
int wena_server_settings_form_activate(WenaServerSettingsForm *form,
 WenaServerSettings *settings,WenaServerRuntime *runtime,const char *database_path,
 const char *executable_path,WenaSecurityStore *security,WenaHttpNow now,void *context);

#define WENA_ADMIN_STORAGE_AUDIT_MAX 16u
typedef enum WenaAdminStorageStatus { WENA_ADMIN_STORAGE_IDLE=0,
 WENA_ADMIN_STORAGE_BUSY,WENA_ADMIN_STORAGE_SUCCESS,WENA_ADMIN_STORAGE_ERROR } WenaAdminStorageStatus;
typedef struct WenaAdminStorageState { WenaAdminStorageStatus status;unsigned long sequence;
 unsigned int audit_count;char audit[WENA_ADMIN_STORAGE_AUDIT_MAX][32];char error[64]; } WenaAdminStorageState;
void wena_admin_storage_init(WenaAdminStorageState *state);
int wena_admin_storage_backup(WenaAdminStorageState *state,WenaServerRuntime *runtime,
 const char *backup_path,WenaBackupFreeSpace free_space,void *space_context);
int wena_admin_storage_restore(WenaAdminStorageState *state,WenaServerRuntime *runtime,
 WenaServerSettings *settings,const char *backup_path,const char *database_path,
 const char *executable_path,WenaBackupFreeSpace free_space,void *space_context,
 WenaSecurityStore *security,WenaHttpNow now,void *now_context);

#endif
