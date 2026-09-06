#include "http.h"

#include <ctype.h>
#include <string.h>

static const char *wena_find_crlf(const char *start, const char *end)
{
    const char *cursor;
    for (cursor = start; cursor + 1 < end; ++cursor)
        if (cursor[0] == '\r' && cursor[1] == '\n') return cursor;
    return NULL;
}

static int wena_copy_token(char *output, size_t capacity,
                           const char *start, const char *end, int lower)
{
    size_t length;
    size_t index;
    length = (size_t)(end - start);
    if (length == 0 || length >= capacity) return 0;
    for (index = 0; index < length; ++index) {
        unsigned char character;
        character = (unsigned char)start[index];
        if (character < 0x21 || character > 0x7e) return 0;
        output[index] = lower ? (char)tolower(character) : (char)character;
    }
    output[length] = '\0';
    return 1;
}

static int wena_header_name_valid(const char *name)
{
    const unsigned char *cursor;
    for (cursor = (const unsigned char *)name; *cursor != '\0'; ++cursor)
        if (!isalnum(*cursor) && *cursor != '-') return 0;
    return name[0] != '\0';
}

const char *wena_http_header(const WenaHttpRequest *request, const char *name)
{
    size_t index;
    if (request == NULL || name == NULL) return NULL;
    for (index = 0; index < request->header_count; ++index)
        if (strcmp(request->headers[index].name, name) == 0)
            return request->headers[index].value;
    return NULL;
}

WenaHttpParseResult wena_http_parse(const char *input, size_t length,
                                    WenaHttpRequest *request)
{
    const char *cursor;
    const char *end;
    const char *line_end;
    const char *space_one;
    const char *space_two;
    const char *headers_end;
    size_t content_length;
    int has_content_length;
    if (input == NULL || request == NULL) return WENA_HTTP_PARSE_INVALID;
    if (length > WENA_HTTP_MAX_REQUEST_BYTES) return WENA_HTTP_PARSE_TOO_LARGE;
    if (memchr(input, '\0', length) != NULL) return WENA_HTTP_PARSE_INVALID;
    memset(request, 0, sizeof(*request));
    end = input + length;
    line_end = wena_find_crlf(input, end);
    if (line_end == NULL) return WENA_HTTP_PARSE_INCOMPLETE;
    space_one = memchr(input, ' ', (size_t)(line_end - input));
    if (space_one == NULL) return WENA_HTTP_PARSE_INVALID;
    space_two = memchr(space_one + 1, ' ', (size_t)(line_end - space_one - 1));
    if (space_two == NULL || memchr(space_two + 1, ' ', (size_t)(line_end - space_two - 1)) != NULL)
        return WENA_HTTP_PARSE_INVALID;
    if (!wena_copy_token(request->method, sizeof(request->method), input, space_one, 0) ||
        (strcmp(request->method, "GET") != 0 && strcmp(request->method, "POST") != 0) ||
        !wena_copy_token(request->target, sizeof(request->target), space_one + 1, space_two, 0) ||
        request->target[0] != '/' || strstr(request->target, "//") == request->target)
        return WENA_HTTP_PARSE_INVALID;
    if (strncmp(space_two + 1, "HTTP/1.", 7) != 0 || line_end - (space_two + 1) != 8 ||
        (space_two[8] != '0' && space_two[8] != '1')) return WENA_HTTP_PARSE_INVALID;
    request->http_minor = space_two[8] - '0';
    cursor = line_end + 2;
    headers_end = NULL;
    content_length = 0;
    has_content_length = 0;
    while (cursor < end) {
        const char *colon;
        const char *value;
        size_t value_length;
        line_end = wena_find_crlf(cursor, end);
        if (line_end == NULL) return WENA_HTTP_PARSE_INCOMPLETE;
        if (line_end == cursor) { headers_end = line_end + 2; break; }
        if (*cursor == ' ' || *cursor == '\t' || request->header_count >= WENA_HTTP_MAX_HEADERS)
            return WENA_HTTP_PARSE_INVALID;
        colon = memchr(cursor, ':', (size_t)(line_end - cursor));
        if (colon == NULL || !wena_copy_token(request->headers[request->header_count].name,
                                               sizeof(request->headers[0].name), cursor, colon, 1))
            return WENA_HTTP_PARSE_INVALID;
        if (!wena_header_name_valid(request->headers[request->header_count].name) ||
            wena_http_header(request, request->headers[request->header_count].name) != NULL)
            return WENA_HTTP_PARSE_INVALID;
        value = colon + 1;
        while (value < line_end && (*value == ' ' || *value == '\t')) ++value;
        value_length = (size_t)(line_end - value);
        while (value_length > 0 && (value[value_length - 1] == ' ' || value[value_length - 1] == '\t'))
            --value_length;
        if (value_length >= sizeof(request->headers[0].value)) return WENA_HTTP_PARSE_INVALID;
        memcpy(request->headers[request->header_count].value, value, value_length);
        request->headers[request->header_count].value[value_length] = '\0';
        if (strcmp(request->headers[request->header_count].name, "transfer-encoding") == 0)
            return WENA_HTTP_PARSE_INVALID;
        if (strcmp(request->headers[request->header_count].name, "content-length") == 0) {
            size_t index;
            if (has_content_length || value_length == 0) return WENA_HTTP_PARSE_INVALID;
            has_content_length = 1;
            for (index = 0; index < value_length; ++index) {
                if (!isdigit((unsigned char)value[index])) return WENA_HTTP_PARSE_INVALID;
                content_length = content_length * 10u + (size_t)(value[index] - '0');
                if (content_length > WENA_HTTP_MAX_BODY_BYTES) return WENA_HTTP_PARSE_TOO_LARGE;
            }
        }
        ++request->header_count;
        cursor = line_end + 2;
    }
    if (headers_end == NULL) return WENA_HTTP_PARSE_INCOMPLETE;
    if (request->http_minor == 1 && (wena_http_header(request, "host") == NULL ||
                                    wena_http_header(request, "host")[0] == '\0'))
        return WENA_HTTP_PARSE_INVALID;
    if (strcmp(request->method, "POST") == 0 && !has_content_length)
        return WENA_HTTP_PARSE_INVALID;
    if ((size_t)(end - headers_end) < content_length) return WENA_HTTP_PARSE_INCOMPLETE;
    request->body = headers_end;
    request->body_length = content_length;
    request->consumed = (size_t)(headers_end - input) + content_length;
    return WENA_HTTP_PARSE_OK;
}

int wena_http_allow_connection(size_t active_connections)
{
    return active_connections < WENA_HTTP_MAX_CONNECTIONS;
}

int wena_http_allow_request(size_t requests_on_connection)
{
    return requests_on_connection < WENA_HTTP_MAX_REQUESTS_PER_CONNECTION;
}
