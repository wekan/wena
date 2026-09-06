#include "settings.h"

#include <ctype.h>
#include <string.h>

static int wena_bounded_copy(char *destination, size_t capacity, const char *source)
{
    size_t length;
    if (destination == NULL || capacity == 0 || source == NULL) {
        return 0;
    }
    length = strlen(source);
    if (length >= capacity) {
        destination[0] = '\0';
        return 0;
    }
    memcpy(destination, source, length + 1);
    return 1;
}

int wena_server_ipv4_valid(const char *value)
{
    int part;
    const char *cursor;
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    cursor = value;
    for (part = 0; part < 4; ++part) {
        unsigned int octet;
        int digits;
        octet = 0u;
        digits = 0;
        if (!isdigit((unsigned char)*cursor)) {
            return 0;
        }
        if (*cursor == '0' && isdigit((unsigned char)cursor[1])) {
            return 0;
        }
        while (isdigit((unsigned char)*cursor)) {
            octet = octet * 10u + (unsigned int)(*cursor - '0');
            ++digits;
            ++cursor;
            if (digits > 3 || octet > 255u) {
                return 0;
            }
        }
        if (part < 3) {
            if (*cursor != '.') {
                return 0;
            }
            ++cursor;
        } else if (*cursor != '\0') {
            return 0;
        }
    }
    return 1;
}

static int wena_host_valid(const char *host)
{
    size_t index;
    size_t label_length;
    if (host == NULL || host[0] == '\0' || strlen(host) >= 128) {
        return 0;
    }
    label_length = 0;
    for (index = 0; host[index] != '\0'; ++index) {
        unsigned char character;
        character = (unsigned char)host[index];
        if (character == '.') {
            if (label_length == 0 || host[index - 1] == '-') {
                return 0;
            }
            label_length = 0;
        } else if (isalnum(character) || character == '-') {
            if (label_length == 0 && character == '-') {
                return 0;
            }
            ++label_length;
            if (label_length > 63) {
                return 0;
            }
        } else {
            return 0;
        }
    }
    return label_length > 0 && host[index - 1] != '-';
}

static int wena_base_path_valid(const char *path)
{
    const char *segment;
    const char *cursor;
    if (path == NULL || (path[0] != '\0' && path[0] != '/')) {
        return 0;
    }
    segment = path;
    cursor = path;
    while (*cursor != '\0') {
        unsigned char character;
        character = (unsigned char)*cursor;
        if (character == '/') {
            size_t length;
            length = (size_t)(cursor - segment);
            if (length == 0 && cursor != path) {
                return 0;
            }
            if ((length == 1 && segment[0] == '.') ||
                (length == 2 && segment[0] == '.' && segment[1] == '.')) {
                return 0;
            }
            segment = cursor + 1;
        } else if (!(isalnum(character) || strchr("-._~%", character) != NULL)) {
            return 0;
        } else if (character == '%' &&
                   (!isxdigit((unsigned char)cursor[1]) ||
                    !isxdigit((unsigned char)cursor[2]))) {
            return 0;
        } else if (character == '%') {
            int high;
            int low;
            int decoded;
            high = isdigit((unsigned char)cursor[1]) ? cursor[1] - '0' :
                   tolower((unsigned char)cursor[1]) - 'a' + 10;
            low = isdigit((unsigned char)cursor[2]) ? cursor[2] - '0' :
                  tolower((unsigned char)cursor[2]) - 'a' + 10;
            decoded = high * 16 + low;
            if (decoded == '.' || decoded == '/' || decoded == '\\' || decoded == 0) {
                return 0;
            }
            cursor += 2;
        }
        ++cursor;
    }
    if ((strcmp(segment, ".") == 0) || (strcmp(segment, "..") == 0)) {
        return 0;
    }
    return strlen(path) < 96;
}

int wena_server_root_url_parse(const char *value, WenaRootUrl *parsed)
{
    const char *host_start;
    const char *port_start;
    const char *path_start;
    const char *cursor;
    char port_text[6];
    size_t length;
    unsigned int port;
    WenaRootUrl result;
    memset(&result, 0, sizeof(result));
    if (value == NULL || parsed == NULL || strlen(value) >= WENA_SERVER_ROOT_URL_CAPACITY) {
        return 0;
    }
    if (strncmp(value, "http://", 7) == 0) {
        strcpy(result.scheme, "http");
        host_start = value + 7;
    } else if (strncmp(value, "https://", 8) == 0) {
        strcpy(result.scheme, "https");
        host_start = value + 8;
    } else {
        return 0;
    }
    port_start = strchr(host_start, ':');
    if (port_start == NULL || strchr(host_start, '@') != NULL) {
        return 0;
    }
    length = (size_t)(port_start - host_start);
    if (length == 0 || length >= sizeof(result.host)) {
        return 0;
    }
    memcpy(result.host, host_start, length);
    result.host[length] = '\0';
    if (!wena_host_valid(result.host)) {
        return 0;
    }
    path_start = strchr(port_start + 1, '/');
    cursor = path_start == NULL ? value + strlen(value) : path_start;
    length = (size_t)(cursor - (port_start + 1));
    if (length == 0 || length >= sizeof(port_text)) {
        return 0;
    }
    memcpy(port_text, port_start + 1, length);
    port_text[length] = '\0';
    port = 0u;
    for (cursor = port_text; *cursor != '\0'; ++cursor) {
        if (!isdigit((unsigned char)*cursor)) {
            return 0;
        }
        port = port * 10u + (unsigned int)(*cursor - '0');
        if (port > 65535u) {
            return 0;
        }
    }
    if (port == 0u || !wena_base_path_valid(path_start == NULL ? "" : path_start)) {
        return 0;
    }
    result.port = port;
    if (!wena_bounded_copy(result.base_path, sizeof(result.base_path),
                           path_start == NULL || strcmp(path_start, "/") == 0 ? "" : path_start)) {
        return 0;
    }
    length = strlen(result.base_path);
    if (length > 0 && result.base_path[length - 1] == '/') {
        result.base_path[length - 1] = '\0';
    }
    *parsed = result;
    return 1;
}

void wena_server_settings_init(WenaServerSettings *settings)
{
    if (settings == NULL) {
        return;
    }
    memset(settings, 0, sizeof(*settings));
    strcpy(settings->bind_ipv4, "127.0.0.1");
    settings->port = 3000u;
    strcpy(settings->root_url, "http://127.0.0.1:3000");
    wena_server_root_url_parse(settings->root_url, &settings->parsed_root_url);
    settings->status = WENA_SERVER_STOPPED;
}

int wena_server_settings_apply(WenaServerSettings *settings, int enabled,
                               const char *bind_ipv4, unsigned int port,
                               const char *root_url)
{
    WenaRootUrl parsed;
    int changed;
    if (settings == NULL || !wena_server_ipv4_valid(bind_ipv4) ||
        port == 0u || port > 65535u ||
        !wena_server_root_url_parse(root_url, &parsed)) {
        return 0;
    }
    changed = settings->enabled != (enabled != 0) || settings->port != port ||
              strcmp(settings->bind_ipv4, bind_ipv4) != 0 ||
              strcmp(settings->root_url, root_url) != 0;
    settings->enabled = enabled != 0;
    strcpy(settings->bind_ipv4, bind_ipv4);
    settings->port = port;
    strcpy(settings->root_url, root_url);
    settings->parsed_root_url = parsed;
    settings->error[0] = '\0';
    if (!settings->enabled) {
        settings->status = WENA_SERVER_STOPPED;
    } else if (changed) {
        settings->status = WENA_SERVER_RESTART_REQUIRED;
    }
    return 1;
}

void wena_server_settings_starting(WenaServerSettings *settings)
{
    if (settings != NULL && settings->enabled) settings->status = WENA_SERVER_STARTING;
}
void wena_server_settings_running(WenaServerSettings *settings)
{
    if (settings != NULL && settings->enabled) {
        settings->status = WENA_SERVER_RUNNING;
        settings->error[0] = '\0';
    }
}
void wena_server_settings_stopped(WenaServerSettings *settings)
{
    if (settings != NULL) settings->status = WENA_SERVER_STOPPED;
}
void wena_server_settings_error(WenaServerSettings *settings, const char *message)
{
    if (settings != NULL) {
        settings->status = WENA_SERVER_ERROR;
        if (!wena_bounded_copy(settings->error, sizeof(settings->error), message)) {
            strcpy(settings->error, "Server error");
        }
    }
}
