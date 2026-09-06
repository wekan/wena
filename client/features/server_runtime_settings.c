#include "server_settings.h"
int wena_server_settings_form_activate(WenaServerSettingsForm *form,WenaServerSettings *settings,WenaServerRuntime *runtime,const char *database_path,const char *executable_path,WenaSecurityStore *security,WenaHttpNow now,void *context)
{
    if(!form||!settings||!runtime||!wena_server_settings_form_submit(form,settings))return 0;
    if(runtime->running)wena_server_runtime_stop(runtime,NULL);
    if(!settings->enabled){wena_server_settings_stopped(settings);return 1;}
    if(!wena_server_runtime_start_executable(runtime,settings,database_path,executable_path,security,now,context)){wena_server_settings_error(settings,"Server failed to start");return 0;}
    return 1;
}
