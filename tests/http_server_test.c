#include "../server/http.h"
#include "../server/http_listener.h"

#include <assert.h>
#include <string.h>

static void parse_ok(const char *raw, WenaHttpRequest *request)
{
    assert(wena_http_parse(raw, strlen(raw), request) == WENA_HTTP_PARSE_OK);
}

static WenaHttpParseResult parse_result(const char *raw, WenaHttpRequest *request)
{
    return wena_http_parse(raw, strlen(raw), request);
}

int main(void)
{
    WenaHttpRequest request;
    WenaHttpListener listener;
    WenaServerSettings settings;
    unsigned int port;
    int started;
    char oversized[WENA_HTTP_MAX_REQUEST_BYTES + 2];

    parse_ok("GET /b/one/demo HTTP/1.1\r\nHost: localhost\r\nAccept: text/html\r\n\r\n", &request);
    assert(strcmp(request.method, "GET") == 0 && request.body_length == 0);
    assert(strcmp(wena_http_header(&request, "host"), "localhost") == 0);
    parse_ok("POST /b/one/demo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\na=1", &request);
    assert(request.body_length == 3 && memcmp(request.body, "a=1", 3) == 0);

    assert(parse_result("GET / HTTP/1.1\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("POST / HTTP/1.1\r\nHost: x\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("GET / HTTP/1.1\r\nHost: x\r\nHost: evil\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("GET / HTTP/1.1\r\nHost: \r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("GET / HTTP/1.1\r\nBad(Name): x\r\nHost: x\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 99999\r\n\r\n", &request) == WENA_HTTP_PARSE_TOO_LARGE);
    assert(parse_result("POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("GET / HTTP/1.1\r\nHost: x\r\n folded: bad\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("GET //evil HTTP/1.1\r\nHost: x\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("PUT / HTTP/1.1\r\nHost: x\r\n\r\n", &request) == WENA_HTTP_PARSE_INVALID);
    assert(parse_result("GET / HTTP/1.1\r\nHost: x\r\n", &request) == WENA_HTTP_PARSE_INCOMPLETE);
    memset(oversized, 'x', sizeof(oversized));
    assert(wena_http_parse(oversized, sizeof(oversized), &request) == WENA_HTTP_PARSE_TOO_LARGE);
    assert(wena_http_allow_connection(WENA_HTTP_MAX_CONNECTIONS - 1));
    assert(!wena_http_allow_connection(WENA_HTTP_MAX_CONNECTIONS));
    assert(wena_http_allow_request(WENA_HTTP_MAX_REQUESTS_PER_CONNECTION - 1));
    assert(!wena_http_allow_request(WENA_HTTP_MAX_REQUESTS_PER_CONNECTION));
    assert(WENA_HTTP_TIMEOUT_SECONDS == 5u);

    wena_http_listener_init(&listener);
    wena_server_settings_init(&settings);
    assert(!wena_http_listener_start(&listener, &settings));
    assert(!listener.open);
    settings.enabled = 1;
    strcpy(settings.bind_ipv4, "not-an-ip");
    assert(!wena_http_listener_start(&listener, &settings));
    strcpy(settings.bind_ipv4, "127.0.0.1");
    started = 0;
    for (port = 39000u; port < 39100u && !started; ++port) {
        settings.port = port;
        started = wena_http_listener_start(&listener, &settings);
    }
    assert(started && listener.open);
    assert(strcmp(listener.bound_ipv4, "127.0.0.1") == 0);
    assert(listener.bound_port == settings.port);
    assert(settings.status == WENA_SERVER_RUNNING);
    settings.status = WENA_SERVER_RESTART_REQUIRED;
    assert(wena_http_listener_restart(&listener, &settings));
    assert(listener.open && settings.status == WENA_SERVER_RUNNING);
    wena_http_listener_stop(&listener, &settings);
    assert(!listener.open && settings.status == WENA_SERVER_STOPPED);
    return 0;
}
