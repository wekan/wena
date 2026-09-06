#include "../server/settings.h"
#include "../client/features/server_settings.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    WenaServerSettings settings;
    WenaRootUrl root;
    WenaServerSettingsForm form;

    wena_server_settings_init(&settings);
    assert(!settings.enabled && settings.status == WENA_SERVER_STOPPED);
    wena_server_settings_form_init(&form, &settings);
    assert(!form.enabled && form.port == 3000u);
    assert(strcmp(settings.bind_ipv4, "127.0.0.1") == 0 && settings.port == 3000u);
    assert(wena_server_ipv4_valid("0.0.0.0"));
    assert(wena_server_ipv4_valid("192.168.1.20"));
    assert(!wena_server_ipv4_valid("256.1.1.1"));
    assert(!wena_server_ipv4_valid("127.00.0.1"));
    assert(!wena_server_ipv4_valid("localhost"));

    assert(wena_server_root_url_parse("https://kanban.example:8443/team/a", &root));
    assert(strcmp(root.scheme, "https") == 0);
    assert(strcmp(root.host, "kanban.example") == 0);
    assert(root.port == 8443u && strcmp(root.base_path, "/team/a") == 0);
    assert(wena_server_root_url_parse("http://127.0.0.1:3000/", &root));
    assert(root.base_path[0] == '\0');
    assert(!wena_server_root_url_parse("ftp://host:21", &root));
    assert(!wena_server_root_url_parse("http://user@host:3000", &root));
    assert(!wena_server_root_url_parse("http://host:3000/a/../b", &root));
    assert(!wena_server_root_url_parse("http://host:3000/a/%2e%2e/b", &root));
    assert(!wena_server_root_url_parse("http://host:3000/a//b", &root));
    assert(!wena_server_root_url_parse("http://host:0", &root));
    assert(!wena_server_root_url_parse("http://host:3000/a?next=evil", &root));
    assert(!wena_server_root_url_parse("http://host:3000/%ZZ", &root));

    assert(wena_server_settings_apply(&settings, 1, "127.0.0.1", 3000u,
                                      "http://localhost:3000/wekan"));
    assert(settings.enabled && settings.status == WENA_SERVER_RESTART_REQUIRED);
    wena_server_settings_starting(&settings);
    assert(settings.status == WENA_SERVER_STARTING);
    wena_server_settings_running(&settings);
    assert(settings.status == WENA_SERVER_RUNNING);
    assert(wena_server_settings_apply(&settings, 1, "127.0.0.1", 3000u,
                                      "http://localhost:3000/wekan"));
    assert(settings.status == WENA_SERVER_RUNNING);
    assert(!wena_server_settings_apply(&settings, 1, "::1", 3000u,
                                       "http://localhost:3000"));
    assert(settings.status == WENA_SERVER_RUNNING);
    wena_server_settings_error(&settings, "address already in use");
    assert(settings.status == WENA_SERVER_ERROR);
    assert(strcmp(settings.error, "address already in use") == 0);
    assert(wena_server_settings_apply(&settings, 0, "127.0.0.1", 3000u,
                                      "http://localhost:3000/wekan"));
    assert(!settings.enabled && settings.status == WENA_SERVER_STOPPED);
    form.enabled = 1;
    strcpy(form.bind_ipv4, "not-ipv4");
    assert(!wena_server_settings_form_submit(&form, &settings));
    assert(form.validation_error[0] != '\0');
    strcpy(form.bind_ipv4, "127.0.0.1");
    assert(wena_server_settings_form_submit(&form, &settings));
    assert(strcmp(wena_server_status_name(settings.status), "restart-required") == 0);
    return 0;
}
