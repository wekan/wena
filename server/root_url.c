#include "root_url.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int wena_route_path_valid(const char *path)
{
    const char *cursor;
    const char *segment;
    if (path == NULL || path[0] != '/' || path[1] == '/') return 0;
    cursor = path;
    segment = path + 1;
    while (*cursor != '\0') {
        unsigned char character;
        character = (unsigned char)*cursor;
        if (character < 0x20 || character == 0x7f || character == '\\' ||
            character == '?' || character == '#') return 0;
        if (character == '/') {
            size_t length;
            length = cursor == path ? 0 : (size_t)(cursor - segment);
            if (cursor != path && (length == 0 ||
                (length == 1 && segment[0] == '.') ||
                (length == 2 && segment[0] == '.' && segment[1] == '.'))) return 0;
            segment = cursor + 1;
        } else if (!(isalnum(character) || strchr("-._~%", character) != NULL)) {
            return 0;
        } else if (character == '%') {
            int high;
            int low;
            int decoded;
            if (!isxdigit((unsigned char)cursor[1]) ||
                !isxdigit((unsigned char)cursor[2])) return 0;
            high = isdigit((unsigned char)cursor[1]) ? cursor[1] - '0' :
                   tolower((unsigned char)cursor[1]) - 'a' + 10;
            low = isdigit((unsigned char)cursor[2]) ? cursor[2] - '0' :
                  tolower((unsigned char)cursor[2]) - 'a' + 10;
            decoded = high * 16 + low;
            if (decoded == '.' || decoded == '/' || decoded == '\\' || decoded == 0)
                return 0;
            cursor += 2;
        }
        ++cursor;
    }
    if (strcmp(segment, ".") == 0 || strcmp(segment, "..") == 0) return 0;
    return 1;
}

int wena_root_url_join(const WenaRootUrl *root, const char *route_path,
                       char *output, size_t capacity)
{
    char port[8];
    size_t required;
    if (output == NULL || capacity == 0) return 0;
    output[0] = '\0';
    if (root == NULL || !wena_route_path_valid(route_path)) return 0;
    sprintf(port, "%u", root->port);
    required = strlen(root->scheme) + 3 + strlen(root->host) + 1 + strlen(port) +
               strlen(root->base_path) + strlen(route_path) + 1;
    if (required > capacity) return 0;
    strcpy(output, root->scheme);
    strcat(output, "://");
    strcat(output, root->host);
    strcat(output, ":");
    strcat(output, port);
    strcat(output, root->base_path);
    if (strcmp(route_path, "/") != 0 || root->base_path[0] == '\0')
        strcat(output, route_path);
    else
        strcat(output, "/");
    return 1;
}
