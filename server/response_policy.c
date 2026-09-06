#include "response_policy.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int wena_case_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) return 0;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static int wena_same_origin(const WenaRootUrl *root, const char *origin)
{
    WenaRootUrl parsed;
    if (origin == NULL || origin[0] == '\0') return 1;
    if (strchr(origin, '\r') != NULL || strchr(origin, '\n') != NULL ||
        !wena_server_root_url_parse(origin, &parsed) || parsed.base_path[0] != '\0') return 0;
    return wena_case_equal(root->scheme, parsed.scheme) &&
           wena_case_equal(root->host, parsed.host) && root->port == parsed.port;
}

static int wena_add(WenaResponsePolicy *policy, const char *name, const char *value)
{
    WenaResponseHeader *header;
    if (policy->header_count >= WENA_RESPONSE_MAX_HEADERS || strlen(value) >= 384) return 0;
    header = &policy->headers[policy->header_count++];
    header->name = name;
    strcpy(header->value, value);
    return 1;
}

int wena_response_policy(const WenaRootUrl *root, const char *origin,
                         WenaResponsePolicy *policy)
{
    char allowed_origin[256];
    if (root == NULL || policy == NULL) return 0;
    memset(policy, 0, sizeof(*policy));
    policy->origin_allowed = wena_same_origin(root, origin);
    if (!policy->origin_allowed) return 0;
    if (!wena_add(policy, "Content-Security-Policy",
        "default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'") ||
        !wena_add(policy, "X-Content-Type-Options", "nosniff") ||
        !wena_add(policy, "Referrer-Policy", "no-referrer") ||
        !wena_add(policy, "X-Frame-Options", "DENY") ||
        !wena_add(policy, "Cache-Control", "no-store") ||
        !wena_add(policy, "Cross-Origin-Resource-Policy", "same-origin")) return 0;
    if (origin != NULL && origin[0] != '\0') {
        sprintf(allowed_origin, "%s://%s:%u", root->scheme, root->host, root->port);
        if (!wena_add(policy, "Access-Control-Allow-Origin", allowed_origin) ||
            !wena_add(policy, "Vary", "Origin")) return 0;
    }
    return 1;
}

const char *wena_response_header(const WenaResponsePolicy *policy, const char *name)
{
    size_t index;
    if (policy == NULL || name == NULL) return NULL;
    for (index = 0; index < policy->header_count; ++index)
        if (strcmp(policy->headers[index].name, name) == 0) return policy->headers[index].value;
    return NULL;
}
