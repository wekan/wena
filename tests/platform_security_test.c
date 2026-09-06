#include "../server/os_entropy.h"
#include "../server/response_policy.h"
#include "../server/security.h"
#include "../server/settings.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    unsigned char first[WENA_SECURITY_TOKEN_BYTES];
    unsigned char second[WENA_SECURITY_TOKEN_BYTES];
    WenaSecurityStore security;
    WenaRootUrl root;
    WenaResponsePolicy policy;
    char token[WENA_SECURITY_TOKEN_CAPACITY];

    assert(!wena_os_entropy(NULL, NULL, sizeof(first)));
    assert(!wena_os_entropy(NULL, first, 0));
    assert(wena_os_entropy(NULL, first, sizeof(first)));
    assert(wena_os_entropy(NULL, second, sizeof(second)));
    assert(memcmp(first, second, sizeof(first)) != 0);
    wena_security_init(&security, wena_os_entropy, NULL);
    assert(wena_security_session_create(&security, "production-user", 1ul, 60ul,
                                        token, sizeof(token)));
    assert(strlen(token) == 64);

    assert(wena_server_root_url_parse("https://Kanban.Example:8443/wekan", &root));
    assert(wena_response_policy(&root, NULL, &policy));
    assert(policy.origin_allowed);
    assert(wena_response_header(&policy, "Access-Control-Allow-Origin") == NULL);
    assert(strstr(wena_response_header(&policy, "Content-Security-Policy"),
                  "form-action 'self'") != NULL);
    assert(strstr(wena_response_header(&policy, "Content-Security-Policy"),
                  "frame-ancestors 'none'") != NULL);
    assert(strcmp(wena_response_header(&policy, "X-Content-Type-Options"), "nosniff") == 0);
    assert(strcmp(wena_response_header(&policy, "Cache-Control"), "no-store") == 0);

    assert(wena_response_policy(&root, "https://kanban.example:8443", &policy));
    assert(strcmp(wena_response_header(&policy, "Access-Control-Allow-Origin"),
                  "https://Kanban.Example:8443") == 0);
    assert(strcmp(wena_response_header(&policy, "Vary"), "Origin") == 0);
    assert(!wena_response_policy(&root, "http://kanban.example:8443", &policy));
    assert(!wena_response_policy(&root, "https://kanban.example:443", &policy));
    assert(!wena_response_policy(&root, "https://kanban.example.evil:8443", &policy));
    assert(!wena_response_policy(&root, "https://kanban.example:8443/path", &policy));
    assert(!wena_response_policy(&root, "null", &policy));
    assert(!wena_response_policy(&root,
           "https://kanban.example:8443\r\nAccess-Control-Allow-Origin: *", &policy));
    assert(wena_response_header(&policy, "Access-Control-Allow-Origin") == NULL);
    return 0;
}
