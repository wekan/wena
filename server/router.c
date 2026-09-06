#include "router.h"

#include <ctype.h>
#include <string.h>

static int wena_route_copy(char *output, size_t capacity, const char *value)
{
    size_t length;
    if (output == NULL || value == NULL) return 0;
    length = strlen(value);
    if (length == 0 || length >= capacity) return 0;
    memcpy(output, value, length + 1);
    return 1;
}

static int wena_safe_segment(const char *start, const char *end)
{
    const char *cursor;
    if (start == end) return 0;
    for (cursor = start; cursor < end; ++cursor)
        if (!isalnum((unsigned char)*cursor) && *cursor != '-' && *cursor != '_') return 0;
    return 1;
}

static int wena_safe_subpath(const char *path)
{
    const char *segment;
    const char *separator;
    segment = path;
    while (*segment != '\0') {
        separator = strchr(segment, '/');
        if (separator == NULL) separator = segment + strlen(segment);
        if (!wena_safe_segment(segment, separator)) return 0;
        segment = *separator == '\0' ? separator : separator + 1;
    }
    return 1;
}

static const WenaUiPageContract *wena_page(const char *target)
{
    const WenaUiPageContract *pages;
    size_t count;
    size_t index;
    pages = wena_ui_pages(&count);
    for (index = 0; index < count - 1; ++index) {
        size_t route_length;
        route_length = strlen(pages[index].route_family);
        if (strcmp(target, pages[index].route_family) == 0) return &pages[index];
        if (index >= 3 && strncmp(target, pages[index].route_family, route_length) == 0 &&
            target[route_length] == '/' && wena_safe_subpath(target + route_length + 1))
            return &pages[index];
    }
    if (strncmp(target, "/b/", 3) == 0) {
        const char *board_end;
        const char *slug;
        board_end = strchr(target + 3, '/');
        if (board_end != NULL && wena_safe_segment(target + 3, board_end)) {
            slug = board_end + 1;
            if (wena_safe_segment(slug, target + strlen(target)) && strchr(slug, '/') == NULL)
                return &pages[count - 1];
        }
    }
    return NULL;
}

static int wena_hex(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    character = (char)tolower((unsigned char)character);
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    return -1;
}

static int wena_form_value(const char *body, size_t body_length, const char *wanted,
                           char *output, size_t capacity)
{
    size_t cursor;
    int found;
    found = 0;
    cursor = 0;
    while (cursor < body_length) {
        size_t pair_end;
        size_t equal;
        size_t name_length;
        pair_end = cursor;
        while (pair_end < body_length && body[pair_end] != '&') ++pair_end;
        equal = cursor;
        while (equal < pair_end && body[equal] != '=') ++equal;
        if (equal == pair_end) return 0;
        name_length = equal - cursor;
        if (strlen(wanted) == name_length && memcmp(body + cursor, wanted, name_length) == 0) {
            size_t source;
            size_t destination;
            if (found) return 0;
            found = 1;
            destination = 0;
            for (source = equal + 1; source < pair_end; ++source) {
                unsigned char character;
                character = (unsigned char)body[source];
                if (character == '+') character = ' ';
                else if (character == '%') {
                    int high;
                    int low;
                    if (source + 2 >= pair_end || (high = wena_hex(body[source + 1])) < 0 ||
                        (low = wena_hex(body[source + 2])) < 0) return 0;
                    character = (unsigned char)(high * 16 + low);
                    source += 2;
                }
                if (character == 0 || character < 0x20 || destination + 1 >= capacity) return 0;
                output[destination++] = (char)character;
            }
            if (destination == 0) return 0;
            output[destination] = '\0';
        }
        cursor = pair_end + 1;
    }
    return found;
}

static int wena_operation_allowed(const char *operation)
{
    int id;
    for (id = WENA_UI_BOARD_MENU; id <= WENA_UI_CLOSE; ++id) {
        const WenaUiControlContract *control;
        control = wena_ui_control((WenaUiControlId)id);
        if (control != NULL && strcmp(control->http_method, "POST") == 0 &&
            strcmp(control->domain_operation, operation) == 0) return 1;
    }
    return 0;
}

WenaRouteResult wena_route_dispatch(const WenaHttpRequest *request,
                                    WenaSecurityStore *security,
                                    unsigned long now,
                                    WenaRouteIntent *intent)
{
    const WenaUiPageContract *page;
    char session[WENA_SECURITY_TOKEN_CAPACITY];
    char csrf[WENA_SECURITY_TOKEN_CAPACITY];
    char operation[65];
    if (request == NULL || intent == NULL) return WENA_ROUTE_REJECT;
    memset(intent, 0, sizeof(*intent));
    if (strchr(request->target, '?') != NULL || strchr(request->target, '#') != NULL)
        return WENA_ROUTE_REJECT;
    page = wena_page(request->target);
    if (page == NULL) return WENA_ROUTE_REJECT;
    if (strcmp(request->method, "GET") == 0) {
        intent->result = WENA_ROUTE_READ_PAGE;
        intent->page = page;
        wena_route_copy(intent->route, sizeof(intent->route), request->target);
        return intent->result;
    }
    if (strcmp(request->method, "POST") != 0 || security == NULL ||
        strncmp(request->target, "/b/", 3) != 0 ||
        wena_http_header(request, "content-type") == NULL ||
        strcmp(wena_http_header(request, "content-type"),
               "application/x-www-form-urlencoded") != 0 ||
        !wena_form_value(request->body, request->body_length, "legacySession",
                         session, sizeof(session)) ||
        !wena_form_value(request->body, request->body_length, "csrf", csrf, sizeof(csrf)) ||
        !wena_form_value(request->body, request->body_length, "legacyOperation",
                         operation, sizeof(operation)) || !wena_operation_allowed(operation) ||
        !wena_security_session_authenticate(security, session, now,
                                            intent->user_id, sizeof(intent->user_id)) ||
        !wena_security_csrf_consume(security, session, csrf, request->target,
                                    operation, now)) {
        memset(intent, 0, sizeof(*intent));
        return WENA_ROUTE_REJECT;
    }
    intent->result = WENA_ROUTE_MUTATION_INTENT;
    intent->page = page;
    wena_route_copy(intent->route, sizeof(intent->route), request->target);
    wena_route_copy(intent->operation, sizeof(intent->operation), operation);
    intent->form_body = request->body;
    intent->form_body_length = request->body_length;
    return intent->result;
}
