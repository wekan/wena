#include "../server/router.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Entropy { unsigned int value; } Entropy;

static int entropy_bytes(void *context, unsigned char *output, size_t length)
{
    Entropy *entropy;
    size_t index;
    entropy = (Entropy *)context;
    ++entropy->value;
    for (index = 0; index < length; ++index)
        output[index] = (unsigned char)(entropy->value + index);
    return 1;
}

static void parse_get(const char *target, WenaHttpRequest *request)
{
    char raw[512];
    sprintf(raw, "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", target);
    assert(wena_http_parse(raw, strlen(raw), request) == WENA_HTTP_PARSE_OK);
}

static void parse_post(const char *target, const char *session, const char *csrf,
                       const char *operation, WenaHttpRequest *request,
                       char *raw, size_t capacity)
{
    char body[512];
    sprintf(body, "legacySession=%s&csrf=%s&legacyOperation=%s&title=Card+title",
            session, csrf, operation);
    sprintf(raw, "POST %s HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %lu\r\n\r\n%s",
            target, (unsigned long)strlen(body), body);
    assert(strlen(raw) < capacity);
    assert(wena_http_parse(raw, strlen(raw), request) == WENA_HTTP_PARSE_OK);
}

int main(void)
{
    WenaSecurityStore security;
    Entropy entropy;
    WenaHttpRequest request;
    WenaRouteIntent intent;
    char session[WENA_SECURITY_TOKEN_CAPACITY];
    char csrf[WENA_SECURITY_TOKEN_CAPACITY];
    char raw[1024];

    memset(&entropy, 0, sizeof(entropy));
    wena_security_init(&security, entropy_bytes, &entropy);
    parse_get("/allboards", &request);
    assert(wena_route_dispatch(&request, &security, 10ul, &intent) == WENA_ROUTE_READ_PAGE);
    assert(strcmp(intent.page->heading_i18n_key, "all-boards") == 0);
    assert(intent.operation[0] == '\0' && security.audit_count == 0);
    parse_get("/admin/settings/server", &request);
    assert(wena_route_dispatch(&request, &security, 10ul, &intent) == WENA_ROUTE_READ_PAGE);
    assert(strcmp(intent.page->heading_i18n_key, "admin-panel") == 0);
    parse_get("/b/board_1/demo-board", &request);
    assert(wena_route_dispatch(&request, &security, 10ul, &intent) == WENA_ROUTE_READ_PAGE);
    parse_get("/unknown", &request);
    assert(wena_route_dispatch(&request, &security, 10ul, &intent) == WENA_ROUTE_REJECT);
    parse_get("/allboards?legacyOperation=archive-card", &request);
    assert(wena_route_dispatch(&request, &security, 10ul, &intent) == WENA_ROUTE_REJECT);

    assert(wena_security_session_create(&security, "user-1", 20ul, 100ul,
                                        session, sizeof(session)));
    assert(wena_security_csrf_issue(&security, session, "/b/board_1/demo", "create-card",
                                    20ul, 30ul, csrf, sizeof(csrf)));
    parse_post("/b/board_1/demo", session, csrf, "create-card", &request, raw, sizeof(raw));
    assert(wena_route_dispatch(&request, &security, 21ul, &intent) == WENA_ROUTE_MUTATION_INTENT);
    assert(strcmp(intent.user_id, "user-1") == 0);
    assert(strcmp(intent.operation, "create-card") == 0);
    assert(intent.form_body == request.body && intent.form_body_length == request.body_length);
    assert(wena_route_dispatch(&request, &security, 22ul, &intent) == WENA_ROUTE_REJECT);
    assert(wena_security_last_audit(&security)->decision == WENA_SECURITY_REJECT_REPLAY);

    assert(wena_security_csrf_issue(&security, session, "/b/board_1/demo", "archive-card",
                                    23ul, 30ul, csrf, sizeof(csrf)));
    parse_post("/b/other/demo", session, csrf, "archive-card", &request, raw, sizeof(raw));
    assert(wena_route_dispatch(&request, &security, 24ul, &intent) == WENA_ROUTE_REJECT);
    assert(wena_security_last_audit(&security)->decision == WENA_SECURITY_REJECT_SCOPE);
    parse_post("/b/board_1/demo", session, csrf, "archive-card", &request, raw, sizeof(raw));
    assert(wena_route_dispatch(&request, &security, 25ul, &intent) == WENA_ROUTE_REJECT);
    assert(wena_security_last_audit(&security)->decision == WENA_SECURITY_REJECT_REPLAY);

    assert(wena_security_csrf_issue(&security, session, "/b/board_1/demo", "create-card",
                                    26ul, 30ul, csrf, sizeof(csrf)));
    parse_post("/b/board_1/demo", session, csrf, "not-an-operation", &request, raw, sizeof(raw));
    assert(wena_route_dispatch(&request, &security, 27ul, &intent) == WENA_ROUTE_REJECT);
    parse_post("/allboards", session, csrf, "create-card", &request, raw, sizeof(raw));
    assert(wena_route_dispatch(&request, &security, 27ul, &intent) == WENA_ROUTE_REJECT);

    parse_post("/b/board_1/demo", "bad", csrf, "create-card", &request, raw, sizeof(raw));
    assert(wena_route_dispatch(&request, &security, 27ul, &intent) == WENA_ROUTE_REJECT);
    parse_post("/b/board_1/demo", session, csrf, "create-card", &request, raw, sizeof(raw));
    strcpy(request.headers[1].value, "text/plain");
    assert(wena_route_dispatch(&request, &security, 27ul, &intent) == WENA_ROUTE_REJECT);
    return 0;
}
