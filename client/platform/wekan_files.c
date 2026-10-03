#include "wekan_files.h"
#include "debug_log.h"
#include "files.h"

#include <string.h>

static char separator_for(int system)
{
    return system == WENA_SYSTEM_WINDOWS ? '\\' : '/';
}

static int is_separator(char c, int system)
{
    return c == '/' || (system == WENA_SYSTEM_WINDOWS && c == '\\') ||
           (system == WENA_SYSTEM_AMIGA && c == ':');
}

static int append(char *out, size_t capacity, size_t *used, const char *text, size_t length)
{
    if (*used + length + 1 > capacity) return 0;
    memcpy(out + *used, text, length);
    *used += length;
    out[*used] = '\0';
    return 1;
}

/* Joins `name` to `directory` with exactly one separator between them. */
static int join(char *out, size_t capacity, size_t *used, int system, const char *name)
{
    char separator = separator_for(system);
    if (*used > 0 && !is_separator(out[*used - 1], system) && !append(out, capacity, used, &separator, 1))
        return 0;
    return append(out, capacity, used, name, strlen(name));
}

/* The last path component, without separators after it. */
static int last_component_is(const char *path, int system, const char *name)
{
    size_t end = strlen(path), start;
    while (end > 0 && is_separator(path[end - 1], system)) --end;
    start = end;
    while (start > 0 && !is_separator(path[start - 1], system)) --start;
    return end - start == strlen(name) && strncmp(path + start, name, end - start) == 0;
}

static int absolute(const char *path, int system)
{
    return system == WENA_SYSTEM_AMIGA ? wena_path_absolute_amiga(path) :
           wena_path_absolute_for(path, system == WENA_SYSTEM_WINDOWS);
}

int wena_wekan_files_root(const char *writable_path, const char *executable, const char *home,
                          int system, char *out, size_t capacity)
{
    size_t used = 0, directory;
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    if (writable_path != NULL && writable_path[0] != '\0') {
        if (!absolute(writable_path, system) || !append(out, capacity, &used, writable_path, strlen(writable_path)))
            return 0;
        if (last_component_is(writable_path, system, "files") ||
            last_component_is(writable_path, system, "wekan-files")) return 1;
        return join(out, capacity, &used, system, "files") || (out[0] = '\0', 0);
    }
    if (system == WENA_SYSTEM_AMIGA)
        return append(out, capacity, &used, "PROGDIR:wekan-files", strlen("PROGDIR:wekan-files"));
    if (system == WENA_SYSTEM_MOBILE) {
        if (home == NULL || !absolute(home, system) || !append(out, capacity, &used, home, strlen(home))) return 0;
        return join(out, capacity, &used, system, "wekan-files") || (out[0] = '\0', 0);
    }
    if (executable == NULL || !absolute(executable, system)) return 0;
    directory = strlen(executable);
    while (directory > 0 && !is_separator(executable[directory - 1], system)) --directory;
    if (directory == 0 || !append(out, capacity, &used, executable, directory)) return 0;
    return join(out, capacity, &used, system, "wekan-files") || (out[0] = '\0', 0);
}

int wena_wekan_files_database(const char *root, int system, char *out, size_t capacity)
{
    size_t used = 0;
    if (root == NULL || out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    if (!append(out, capacity, &used, root, strlen(root)) || !join(out, capacity, &used, system, "db") ||
        !join(out, capacity, &used, system, "wekan.sqlite")) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

int wena_wekan_files_prepare(const char *root, int system)
{
    static const char *const folders[] = {"attachments", "avatars", "db"};
    char path[4096];
    size_t index, used;
    char separator = separator_for(system);
    for (index = 0; index < sizeof(folders) / sizeof(folders[0]); ++index) {
        /* mkdir -p of root/folder: the parent directories of root/folder/x. */
        used = 0;
        path[0] = '\0';
        if (!append(path, sizeof(path), &used, root, strlen(root)) ||
            !join(path, sizeof(path), &used, system, folders[index]) ||
            !append(path, sizeof(path), &used, &separator, 1) ||
            !append(path, sizeof(path), &used, "x", 1) ||
            !wena_make_parent_directories(path)) return 0;
    }
    return 1;
}
