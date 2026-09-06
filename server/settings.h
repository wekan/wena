#ifndef WENA_SERVER_SETTINGS_H
#define WENA_SERVER_SETTINGS_H

#include <stddef.h>

#define WENA_SERVER_IPV4_CAPACITY 16
#define WENA_SERVER_ROOT_URL_CAPACITY 256
#define WENA_SERVER_ERROR_CAPACITY 160

typedef enum WenaServerStatus {
    WENA_SERVER_STOPPED,
    WENA_SERVER_RESTART_REQUIRED,
    WENA_SERVER_STARTING,
    WENA_SERVER_RUNNING,
    WENA_SERVER_ERROR
} WenaServerStatus;

typedef struct WenaRootUrl {
    char scheme[6];
    char host[128];
    unsigned int port;
    char base_path[96];
} WenaRootUrl;

typedef struct WenaServerSettings {
    int enabled;
    char bind_ipv4[WENA_SERVER_IPV4_CAPACITY];
    unsigned int port;
    char root_url[WENA_SERVER_ROOT_URL_CAPACITY];
    WenaRootUrl parsed_root_url;
    WenaServerStatus status;
    char error[WENA_SERVER_ERROR_CAPACITY];
} WenaServerSettings;

void wena_server_settings_init(WenaServerSettings *settings);
int wena_server_ipv4_valid(const char *value);
int wena_server_root_url_parse(const char *value, WenaRootUrl *parsed);
int wena_server_settings_apply(WenaServerSettings *settings, int enabled,
                               const char *bind_ipv4, unsigned int port,
                               const char *root_url);
void wena_server_settings_starting(WenaServerSettings *settings);
void wena_server_settings_running(WenaServerSettings *settings);
void wena_server_settings_stopped(WenaServerSettings *settings);
void wena_server_settings_error(WenaServerSettings *settings, const char *message);

#endif
