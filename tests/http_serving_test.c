#include "../server/http_listener.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct TestSecurityContext {
    unsigned int entropy_value;
    unsigned long now;
} TestSecurityContext;

typedef struct FakeDomain { int calls; int fail; } FakeDomain;

static int domain_apply(void *context, const WenaDomainCommand *command,
                        WenaRegionResponse *response)
{
    FakeDomain *fake;
    fake = (FakeDomain *)context;
    ++fake->calls;
    if (fake->fail || command->operation != WENA_DOMAIN_ARCHIVE_CARD) return 0;
    response->region_count = 1u;
    strcpy(response->regions[0].name, "board");
    response->regions[0].version = (unsigned long)fake->calls;
    strcpy(response->regions[0].content, "Board updated");
    response->regions[0].content_length = strlen(response->regions[0].content);
    return 1;
}

static int entropy_bytes(void *context, unsigned char *output, size_t length)
{
    TestSecurityContext *state;
    size_t index;
    state = (TestSecurityContext *)context;
    ++state->entropy_value;
    for (index = 0; index < length; ++index)
        output[index] = (unsigned char)(state->entropy_value + (unsigned int)index);
    return 1;
}

static unsigned long test_now(void *context)
{
    return ((TestSecurityContext *)context)->now;
}

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
    WenaSecurityStore security;
    TestSecurityContext security_context;
    char session[WENA_SECURITY_TOKEN_CAPACITY];
    char csrf[WENA_SECURITY_TOKEN_CAPACITY];
    char body[512];
    WenaDomainAdapter domain_adapter;
    FakeDomain fake_domain;

    wena_http_listener_init(&listener);
    wena_server_settings_init(&settings);
    memset(&security_context, 0, sizeof(security_context));
    memset(&fake_domain, 0, sizeof(fake_domain));
    security_context.now = 100ul;
    wena_security_init(&security, entropy_bytes, &security_context);
    wena_http_listener_set_security(&listener, &security, test_now, &security_context);
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
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);

    assert(wena_security_session_create(&security, "user-1", 100ul, 100ul,
                                        session, sizeof(session)));
    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=archive-card", session, csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 503 Service Unavailable") != NULL);
    assert(strstr(response, "Mutation dispatch is not enabled") != NULL);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);

    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=create-card", session, csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);

    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=bad-session&csrf=%s&legacyOperation=archive-card", csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=archive-card", session, csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 503 Service Unavailable") != NULL);

    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=archive-card", session, csrf);
    sprintf(request, "POST /base/b/two/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 403 Forbidden") != NULL);

    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=archive-card", session, csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nAccept: application/vnd.wena.regions-v1\r\nX-Wena-Request-Version: 1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 503 Service Unavailable") != NULL);
    assert(fake_domain.calls == 0);

    wena_http_listener_stop(&listener, NULL);
    wena_domain_adapter_init(&domain_adapter, domain_apply, &fake_domain);
    wena_http_listener_set_domain_adapter(&listener, &domain_adapter);
    assert(wena_http_listener_start(&listener, &settings));
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_OK);
    assert(strstr(response, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(response, "Content-Type: application/vnd.wena.regions-v1") != NULL);
    assert(strstr(response, "WENA-REGIONS/1\nrequest-version 1\n") != NULL);
    assert(fake_domain.calls == 1);

    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=archive-card", session, csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",listener.bound_port,(unsigned long)strlen(body),body);
    assert(exchange(&listener,&settings,request,response,sizeof(response))==WENA_HTTP_SERVE_OK);
    assert(strstr(response,"HTTP/1.1 303 See Other")!=NULL);
    assert(strstr(response,"Location: http://127.0.0.1:")!=NULL);
    assert(strstr(response,"/base/b/one/demo\r\n")!=NULL);

    assert(wena_security_csrf_issue(&security, session, "/b/one/demo", "archive-card",
                                    100ul, 50ul, csrf, sizeof(csrf)));
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=archive-card", session, csrf);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nAccept: application/vnd.wena.regions-v1\r\nX-Wena-Request-Version: broken\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 406 Not Acceptable") != NULL);
    assert(fake_domain.calls == 2);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nAccept: application/vnd.wena.regions-v1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 406 Not Acceptable") != NULL);
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nAccept: application/json\r\nX-Wena-Request-Version: 2\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 406 Not Acceptable") != NULL);
    fake_domain.fail = 1;
    sprintf(request, "POST /base/b/one/demo HTTP/1.1\r\nHost: 127.0.0.1:%u\r\nAccept: application/vnd.wena.regions-v1\r\nX-Wena-Request-Version: 3\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            listener.bound_port, (unsigned long)strlen(body), body);
    assert(exchange(&listener, &settings, request, response, sizeof(response)) ==
           WENA_HTTP_SERVE_REJECTED);
    assert(strstr(response, "HTTP/1.1 409 Conflict") != NULL);
    assert(fake_domain.calls == 3 && domain_adapter.last_request_version == 2ul);

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
