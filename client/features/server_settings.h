#ifndef WENA_CLIENT_SERVER_SETTINGS_H
#define WENA_CLIENT_SERVER_SETTINGS_H

#include "../../server/settings.h"
#include "../../server/runtime.h"

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

#endif
