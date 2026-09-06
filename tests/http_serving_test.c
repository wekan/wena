#include "../server/http_listener.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int connect_client(unsigned int port)
{
    int client;
    struct sockaddr_in address;
    client = socket(AF_INET, SOCK_STREAM, 0);
    assert(client >= 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(connect(client, (struct sockaddr *)&address, sizeof(address)) == 0);
    return client;
}

static WenaHttpServeResult exchange(WenaHttpListener *listener,
                                    const WenaServerSettings *settings,
                                    const char *request, char *response,
                                    size_t capacity)
{
    int client;
    int amount;
    WenaHttpServeResult result;
    client = connect_client(listener->bound_port);
    assert(send(client, request, strlen(request), 0) == (int)strlen(request));
    shutdown(client, SHUT_WR);
    result = wena_http_listener_serve_once(listener, settings, 1000u);
    amount = recv(client, response, (int)capacity - 1, 0);
    assert(amount > 0);
    response[amount] = '\0';
    close(client);
    return result;
}

int main(void)
{
    WenaHttpListener listener;
    WenaServerSettings settings;
    unsigned int port;
    char root_url[64];
    char response[16384];
    char request[1024];
    int started;

    wena_http_listener_init(&listener);
    wena_server_settings_init(&settings);
    started = 0;
    for (port = 39100u; port < 39200u && !started; ++port) {
        sprintf(root_url, "http://127.0.0.1:%u/base", port);
        assert(wena_server_settings_apply(&settings, 1, "127.0.0.1", port, root_url));
        started = wena_http_listener_start(&listener, &settings);
    }
    assert(started);
    assert(wena_http_listener_serve_once(&listener, &settings, 10u) == WENA_HTTP_SERVE_TIMEOUT);
    assert(wena_http_listener_serve_once(&listener, &settings, 5001u) == WENA_HTTP_SERVE_ERROR);

    sprintf(request, "GET /base/allboards HTTP/1.1\r\nHost: 127.0.0.1:%u\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request,
           response, sizeof(response)) == WENA_HTTP_SERVE_OK);
    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "Content-Security-Policy:") != NULL);
    assert(strstr(response, "href=\"http://127.0.0.1:") != NULL);
    assert(strstr(response, "/base/allboards\"") != NULL);

    sprintf(request, "GET /base/legacy-html4-capabilities.js HTTP/1.1\r\nHost: 127.0.0.1:%u\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_OK);
    assert(strstr(response, "Content-Type: application/javascript; charset=utf-8") != NULL);
    assert(strstr(response, "window.WenaLegacyEnhancement") != NULL);

    sprintf(request, "GET /base/legacy-html4-capabilities.css HTTP/1.1\r\nHost: 127.0.0.1:%u\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_OK);
    assert(strstr(response, "Content-Type: text/css; charset=utf-8") != NULL);
    assert(strstr(response, ".wena-drag-control{display:none}") != NULL);

    sprintf(request, "GET /base/allboards HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nOrigin: http://127.0.0.1:%u\r\n\r\n",
            listener.bound_port, listener.bound_port);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_OK);
    assert(strstr(response, "Access-Control-Allow-Origin: http://127.0.0.1:") != NULL);

    sprintf(request, "GET /base/allboards HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nOrigin: https://evil.example:443\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request,
           response, sizeof(response)) == WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);
    assert(strstr(response, "Access-Control-Allow-Origin") == NULL);

    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Length: 0\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request,
           response, sizeof(response)) == WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 503 Service Unavailable") != NULL);
    assert(strstr(response, "Mutation dispatch is not enabled") != NULL);

    /* A cookieless/no-JS client gets semantic HTML and can follow the GET link;
       no inline script or enhancement-only control is needed to read the page. */
    sprintf(request, "GET /base/allboards HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nCookie:\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_OK);
    assert(strstr(response, "<!DOCTYPE HTML PUBLIC") != NULL);
    assert(strstr(response, "<script>") == NULL);
    assert(strstr(response, "<table ") != NULL);

    sprintf(request, "GET /allboards HTTP/1.1\r\nHost: 127.0.0.1:%u\r\n\r\n",
            listener.bound_port);
    assert(exchange(&listener, &settings, request,
           response, sizeof(response)) == WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 404 Not Found") != NULL);

    assert(exchange(&listener, &settings,
           "GET /base/allboards HTTP/1.1\r\nHost: evil.example:80\r\n\r\n",
           response, sizeof(response)) == WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 421 Misdirected Request") != NULL);

    assert(exchange(&listener, &settings,
           "GET / HTTP/1.1\nHost: bad\n\n", response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
    wena_http_listener_stop(&listener, &settings);
    return 0;
}
