#include "http_listener.h"

#include "http.h"

#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define WENA_SOCKET SOCKET
#define WENA_INVALID_SOCKET INVALID_SOCKET
#define wena_close_socket closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define WENA_SOCKET int
#define WENA_INVALID_SOCKET (-1)
#define wena_close_socket close
#endif

void wena_http_listener_init(WenaHttpListener *listener)
{
    if (listener != NULL) memset(listener, 0, sizeof(*listener));
}

int wena_http_listener_start(WenaHttpListener *listener, WenaServerSettings *settings)
{
    WENA_SOCKET socket_handle;
    struct sockaddr_in address;
    int reuse;
#if defined(_WIN32)
    WSADATA winsock;
#endif
    if (listener == NULL || settings == NULL || listener->open || !settings->enabled ||
        !wena_server_ipv4_valid(settings->bind_ipv4) || settings->port == 0u ||
        settings->port > 65535u) return 0;
    wena_server_settings_starting(settings);
#if defined(_WIN32)
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        wena_server_settings_error(settings, "Winsock startup failed");
        return 0;
    }
#endif
    socket_handle = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_handle == WENA_INVALID_SOCKET) {
        wena_server_settings_error(settings, "IPv4 socket creation failed");
        return 0;
    }
    reuse = 1;
    setsockopt(socket_handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)settings->port);
    if (inet_pton(AF_INET, settings->bind_ipv4, &address.sin_addr) != 1 ||
        bind(socket_handle, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(socket_handle, (int)WENA_HTTP_MAX_CONNECTIONS) != 0) {
        wena_close_socket(socket_handle);
        wena_server_settings_error(settings, "IPv4 bind/listen failed");
        return 0;
    }
    listener->socket_handle = (size_t)socket_handle;
    listener->open = 1;
    strcpy(listener->bound_ipv4, settings->bind_ipv4);
    listener->bound_port = settings->port;
    wena_server_settings_running(settings);
    return 1;
}

void wena_http_listener_stop(WenaHttpListener *listener, WenaServerSettings *settings)
{
    if (listener != NULL && listener->open) {
        wena_close_socket((WENA_SOCKET)listener->socket_handle);
        listener->open = 0;
#if defined(_WIN32)
        WSACleanup();
#endif
    }
    if (settings != NULL) wena_server_settings_stopped(settings);
}

int wena_http_listener_restart(WenaHttpListener *listener, WenaServerSettings *settings)
{
    if (listener == NULL || settings == NULL || !settings->enabled) return 0;
    wena_http_listener_stop(listener, NULL);
    return wena_http_listener_start(listener, settings);
}
