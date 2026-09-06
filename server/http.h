#ifndef WENA_SERVER_HTTP_H
#define WENA_SERVER_HTTP_H

#include <stddef.h>

#define WENA_HTTP_MAX_REQUEST_BYTES 16384u
#define WENA_HTTP_MAX_TARGET_BYTES 2048u
#define WENA_HTTP_MAX_BODY_BYTES 8192u
#define WENA_HTTP_MAX_HEADERS 32u
#define WENA_HTTP_MAX_CONNECTIONS 32u
#define WENA_HTTP_MAX_REQUESTS_PER_CONNECTION 16u
#define WENA_HTTP_TIMEOUT_SECONDS 5u

typedef struct WenaHttpHeader {
    char name[64];
    char value[512];
} WenaHttpHeader;

typedef struct WenaHttpRequest {
    char method[8];
    char target[WENA_HTTP_MAX_TARGET_BYTES + 1];
    int http_minor;
    WenaHttpHeader headers[WENA_HTTP_MAX_HEADERS];
    size_t header_count;
    const char *body;
    size_t body_length;
    size_t consumed;
} WenaHttpRequest;

typedef enum WenaHttpParseResult {
    WENA_HTTP_PARSE_INVALID = 0,
    WENA_HTTP_PARSE_OK = 1,
    WENA_HTTP_PARSE_INCOMPLETE = 2,
    WENA_HTTP_PARSE_TOO_LARGE = 3
} WenaHttpParseResult;

WenaHttpParseResult wena_http_parse(const char *input, size_t length,
                                    WenaHttpRequest *request);
const char *wena_http_header(const WenaHttpRequest *request, const char *name);
int wena_http_allow_connection(size_t active_connections);
int wena_http_allow_request(size_t requests_on_connection);

#endif
