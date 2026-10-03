#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "files.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define FILES_PATH_CAPACITY 4096

int wena_path_absolute_for(const char *path, int windows)
{
    if (path == NULL || path[0] == '\0') return 0;
    if (!windows) return path[0] == '/';
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/')) return 1;
    return path[0] == '\\' && path[1] == '\\' && path[2] != '\0' && path[2] != '\\';
}

int wena_path_absolute(const char *path)
{
#if defined(_WIN32)
    return wena_path_absolute_for(path, 1);
#else
    return wena_path_absolute_for(path, 0);
#endif
}

char wena_path_separator(void)
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

#if defined(_WIN32)
static int wide(const char *utf8, wchar_t *out, int capacity)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, capacity);
    return length > 0;
}
#endif

int wena_file_kind(const char *path)
{
#if defined(_WIN32)
    wchar_t name[FILES_PATH_CAPACITY];
    DWORD attributes;
    if (path == NULL || !wide(path, name, FILES_PATH_CAPACITY)) return WENA_FILE_ERROR;
    attributes = GetFileAttributesW(name);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            WENA_FILE_MISSING : WENA_FILE_ERROR;
    }
    return attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT) ?
        WENA_FILE_OTHER : WENA_FILE_REGULAR;
#else
    struct stat info;
    if (path == NULL) return WENA_FILE_ERROR;
    if (stat(path, &info) != 0) return errno == ENOENT ? WENA_FILE_MISSING : WENA_FILE_ERROR;
    return S_ISREG(info.st_mode) ? WENA_FILE_REGULAR : WENA_FILE_OTHER;
#endif
}

static int make_directory(const char *path)
{
#if defined(_WIN32)
    wchar_t name[FILES_PATH_CAPACITY];
    if (!wide(path, name, FILES_PATH_CAPACITY)) return 0;
    return CreateDirectoryW(name, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

static int separator(char c)
{
#if defined(_WIN32)
    return c == '\\' || c == '/';
#else
    return c == '/';
#endif
}

int wena_make_parent_directories(const char *file)
{
    char partial[FILES_PATH_CAPACITY];
    size_t length, i, start;
    if (!wena_path_absolute(file)) return 0;
    length = strlen(file);
    while (length > 0 && !separator(file[length - 1])) --length;
    if (length == 0 || length >= sizeof(partial)) return 0;
    memcpy(partial, file, length);
    partial[length] = '\0';
    /* Skip the root: "/", "C:\" or "\\server\share\". */
    start = 1;
#if defined(_WIN32)
    if (partial[1] == ':') start = 3;
    else {
        size_t parts = 0;
        for (start = 2; partial[start] && parts < 2; ++start)
            if (separator(partial[start])) ++parts;
    }
#endif
    for (i = start; i < length; ++i) {
        if (separator(partial[i])) {
            partial[i] = '\0';
            if (!make_directory(partial)) return 0;
            partial[i] = wena_path_separator();
        }
    }
    return 1;
}

int wena_environment(const char *name, char *out, size_t capacity)
{
#if defined(_WIN32)
    wchar_t key[256], value[FILES_PATH_CAPACITY];
    DWORD length;
    int written;
    if (name == NULL || out == NULL || capacity == 0 || !wide(name, key, 256)) return 0;
    out[0] = '\0';
    length = GetEnvironmentVariableW(key, value, FILES_PATH_CAPACITY);
    if (length == 0 || length >= FILES_PATH_CAPACITY) return 0;
    written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, out, (int)capacity, NULL, NULL);
    if (written <= 1) { out[0] = '\0'; return 0; }
    return 1;
#else
    const char *value = name == NULL ? NULL : getenv(name);
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    if (value == NULL || value[0] == '\0' || strlen(value) >= capacity) return 0;
    strcpy(out, value);
    return 1;
#endif
}
