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

void wena_http_listener_init(WenaHttpListener *listener);
int wena_http_listener_start(WenaHttpListener *listener, WenaServerSettings *settings);
void wena_http_listener_stop(WenaHttpListener *listener, WenaServerSettings *settings);
int wena_http_listener_restart(WenaHttpListener *listener, WenaServerSettings *settings);

#endif
