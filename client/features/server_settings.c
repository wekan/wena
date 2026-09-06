#include "server_settings.h"

#include <string.h>

void wena_server_settings_form_init(WenaServerSettingsForm *form,
                                    const WenaServerSettings *settings)
{
    if (form == NULL || settings == NULL) return;
    memset(form, 0, sizeof(*form));
    form->enabled = settings->enabled;
    strcpy(form->bind_ipv4, settings->bind_ipv4);
    form->port = settings->port;
    strcpy(form->root_url, settings->root_url);
}

int wena_server_settings_form_submit(WenaServerSettingsForm *form,
                                     WenaServerSettings *settings)
{
    if (form == NULL || settings == NULL) return 0;
    form->validation_error[0] = '\0';
    if (!wena_server_settings_apply(settings, form->enabled, form->bind_ipv4,
                                    form->port, form->root_url)) {
        strcpy(form->validation_error, "Invalid IPv4, port, or ROOT_URL");
        return 0;
    }
    return 1;
}

const char *wena_server_status_name(WenaServerStatus status)
{
    switch (status) {
    case WENA_SERVER_STOPPED: return "stopped";
    case WENA_SERVER_RESTART_REQUIRED: return "restart-required";
    case WENA_SERVER_STARTING: return "starting";
    case WENA_SERVER_RUNNING: return "running";
    case WENA_SERVER_ERROR: return "error";
    }
    return "unknown";
}
