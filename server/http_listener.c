#include "http_listener.h"

#include "http.h"
#include "legacy_html4.h"
#include "response_policy.h"
#include "router.h"

#include <ctype.h>
#include <stdio.h>
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
#include <sys/select.h>
#include <sys/time.h>
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

static int wena_send_all(WENA_SOCKET socket_handle, const char *value, size_t length)
{
    size_t sent;
    sent = 0;
    while (sent < length) {
        int amount;
        amount = send(socket_handle, value + sent, (int)(length - sent), 0);
        if (amount <= 0) return 0;
        sent += (size_t)amount;
    }
    return 1;
}

static int wena_response(WENA_SOCKET client, int status, const char *reason,
                         const WenaResponsePolicy *policy, const char *body)
{
    char response[16384];
    char line[512];
    size_t index;
    size_t used;
    int written;
    written = sprintf(response, "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %lu\r\n",
                      status, reason, (unsigned long)strlen(body));
    if (written < 0) return 0;
    used = (size_t)written;
    for (index = 0; index < policy->header_count; ++index) {
        written = sprintf(line, "%s: %s\r\n", policy->headers[index].name,
                          policy->headers[index].value);
        if (written < 0 || used + (size_t)written + 2 + strlen(body) >= sizeof(response)) return 0;
        memcpy(response + used, line, (size_t)written);
        used += (size_t)written;
    }
    memcpy(response + used, "\r\n", 2); used += 2;
    memcpy(response + used, body, strlen(body)); used += strlen(body);
    return wena_send_all(client, response, used);
}

static int wena_strip_base_path(WenaHttpRequest *request, const char *base_path)
{
    size_t base_length;
    if (base_path == NULL || base_path[0] == '\0') return 1;
    base_length = strlen(base_path);
    if (strncmp(request->target, base_path, base_length) != 0 ||
        (request->target[base_length] != '\0' && request->target[base_length] != '/')) return 0;
    if (request->target[base_length] == '\0') strcpy(request->target, "/");
    else memmove(request->target, request->target + base_length,
                 strlen(request->target + base_length) + 1);
    return 1;
}

static int wena_host_allowed(const WenaRootUrl *root, const char *host)
{
    char expected[160];
    size_t index;
    if (root == NULL || host == NULL || strchr(host, '\r') != NULL || strchr(host, '\n') != NULL)
        return 0;
    sprintf(expected, "%s:%u", root->host, root->port);
    if (strlen(expected) != strlen(host)) return 0;
    for (index = 0; expected[index] != '\0'; ++index)
        if (tolower((unsigned char)expected[index]) != tolower((unsigned char)host[index])) return 0;
    return 1;
}

WenaHttpServeResult wena_http_listener_serve_once(WenaHttpListener *listener,
                                                  const WenaServerSettings *settings,
                                                  unsigned int accept_timeout_ms)
{
    WENA_SOCKET server_socket;
    WENA_SOCKET client;
    fd_set readable;
    struct timeval wait;
    char input[WENA_HTTP_MAX_REQUEST_BYTES];
    size_t received;
    WenaHttpRequest request;
    WenaHttpParseResult parsed;
    WenaResponsePolicy policy;
    const char *origin;
    WenaRouteIntent intent;
    char body[8192];
    WenaHtml4Row row;
    WenaHtml4Page page;
    int selected;
    if (listener == NULL || settings == NULL || !listener->open ||
        accept_timeout_ms > WENA_HTTP_TIMEOUT_SECONDS * 1000u) return WENA_HTTP_SERVE_ERROR;
    server_socket = (WENA_SOCKET)listener->socket_handle;
    FD_ZERO(&readable);
    FD_SET(server_socket, &readable);
    wait.tv_sec = (long)(accept_timeout_ms / 1000u);
    wait.tv_usec = (long)((accept_timeout_ms % 1000u) * 1000u);
    selected = select((int)server_socket + 1, &readable, NULL, NULL, &wait);
    if (selected == 0) return WENA_HTTP_SERVE_TIMEOUT;
    if (selected < 0) return WENA_HTTP_SERVE_ERROR;
    client = accept(server_socket, NULL, NULL);
    if (client == WENA_INVALID_SOCKET) return WENA_HTTP_SERVE_ERROR;
#if defined(_WIN32)
    {
        DWORD timeout;
        timeout = WENA_HTTP_TIMEOUT_SECONDS * 1000u;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
    }
#else
    {
        struct timeval timeout;
        timeout.tv_sec = WENA_HTTP_TIMEOUT_SECONDS;
        timeout.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    }
#endif
    received = 0;
    parsed = WENA_HTTP_PARSE_INCOMPLETE;
    while (received < sizeof(input) && parsed == WENA_HTTP_PARSE_INCOMPLETE) {
        int amount;
        amount = recv(client, input + received, (int)(sizeof(input) - received), 0);
        if (amount <= 0) break;
        received += (size_t)amount;
        parsed = wena_http_parse(input, received, &request);
    }
    if (parsed != WENA_HTTP_PARSE_OK) {
        wena_response_policy(&settings->parsed_root_url, NULL, &policy);
        wena_response(client, 400, "Bad Request", &policy, "Bad Request");
        wena_close_socket(client);
        return WENA_HTTP_SERVE_REJECTED;
    }
    if (!wena_host_allowed(&settings->parsed_root_url, wena_http_header(&request, "host"))) {
        wena_response_policy(&settings->parsed_root_url, NULL, &policy);
        wena_response(client, 421, "Misdirected Request", &policy, "Misdirected Request");
        wena_close_socket(client);
        return WENA_HTTP_SERVE_REJECTED;
    }
    origin = wena_http_header(&request, "origin");
    if (!wena_response_policy(&settings->parsed_root_url, origin, &policy)) {
        wena_response_policy(&settings->parsed_root_url, NULL, &policy);
        wena_response(client, 403, "Forbidden", &policy, "Forbidden");
        wena_close_socket(client);
        return WENA_HTTP_SERVE_REJECTED;
    }
    if (!wena_strip_base_path(&request, settings->parsed_root_url.base_path)) {
        wena_response(client, 404, "Not Found", &policy, "Not Found");
        wena_close_socket(client);
        return WENA_HTTP_SERVE_REJECTED;
    }
    if (strcmp(request.method, "POST") == 0) {
        wena_response(client, 503, "Service Unavailable", &policy,
                      "Mutation dispatch is not enabled");
        wena_close_socket(client);
        return WENA_HTTP_SERVE_REJECTED;
    }
    if (wena_route_dispatch(&request, NULL, 0ul, &intent) != WENA_ROUTE_READ_PAGE) {
        wena_response(client, 404, "Not Found", &policy, "Not Found");
        wena_close_socket(client);
        return WENA_HTTP_SERVE_REJECTED;
    }
    row.heading = intent.page->heading_i18n_key;
    row.content = intent.page->route_family;
    page.language = "en";
    page.title = intent.page->heading_i18n_key;
    page.heading = intent.page->heading_i18n_key;
    page.route_path = request.target;
    page.theme_name = "belize";
    page.rows = &row;
    page.row_count = 1;
    if (!wena_html4_render_page(&settings->parsed_root_url, &page, body, sizeof(body)) ||
        !wena_response(client, 200, "OK", &policy, body)) {
        wena_close_socket(client);
        return WENA_HTTP_SERVE_ERROR;
    }
    wena_close_socket(client);
    return WENA_HTTP_SERVE_OK;
}
