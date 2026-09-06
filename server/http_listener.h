#ifndef WENA_SERVER_HTTP_LISTENER_H
#define WENA_SERVER_HTTP_LISTENER_H

#include "settings.h"

#include <stddef.h>

typedef struct WenaHttpListener {
    size_t socket_handle;
    int open;
    char bound_ipv4[WENA_SERVER_IPV4_CAPACITY];
    unsigned int bound_port;
} WenaHttpListener;

typedef enum WenaHttpServeResult {
    WENA_HTTP_SERVE_ERROR = 0,
    WENA_HTTP_SERVE_OK = 1,
    WENA_HTTP_SERVE_TIMEOUT = 2,
    WENA_HTTP_SERVE_REJECTED = 3
} WenaHttpServeResult;

void wena_http_listener_init(WenaHttpListener *listener);
int wena_http_listener_start(WenaHttpListener *listener, WenaServerSettings *settings);
void wena_http_listener_stop(WenaHttpListener *listener, WenaServerSettings *settings);
int wena_http_listener_restart(WenaHttpListener *listener, WenaServerSettings *settings);
WenaHttpServeResult wena_http_listener_serve_once(WenaHttpListener *listener,
                                                  const WenaServerSettings *settings,
                                                  unsigned int accept_timeout_ms);

#endif
