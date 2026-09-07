#define _POSIX_C_SOURCE 200809L
#include "language.h"

#include "locale.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Existing settings must be ordinary files. Missing paths are valid only for
 * creation/clear. In particular, never follow a settings symlink. */
static int wena_language_path_valid(const char *path, int allow_missing)
{
#if defined(_WIN32)
    DWORD attributes, error;
    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = GetLastError();
        return allow_missing && (error == ERROR_FILE_NOT_FOUND ||
                                  error == ERROR_PATH_NOT_FOUND);
    }
    return !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
#else
    struct stat info;
    if (lstat(path, &info) != 0) return allow_missing && errno == ENOENT;
    return S_ISREG(info.st_mode);
#endif
}

static int wena_language_apply(WenaLanguageState *state, const char *requested,
                               const char *const *available, size_t count,
                               int explicit_override)
{
    char resolved[64];
    if (state == NULL ||
        !wena_locale_resolve(requested, available, count, resolved, sizeof(resolved))) {
        return 0;
    }
    strcpy(state->current, resolved);
    state->explicit_override = explicit_override;
    state->rtl = wena_locale_is_rtl(resolved);
    return 1;
}

static int wena_language_load(const char *path, char *value, size_t capacity)
{
    FILE *file;
    size_t length;
    int extra;
    if (path == NULL || value == NULL || capacity < 2 ||
        !wena_language_path_valid(path, 0)) {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (fgets(value, (int)capacity, file) == NULL) {
        fclose(file);
        return 0;
    }
    length = strlen(value);
    if (length > 0 && value[length - 1] == '\n') {
        value[--length] = '\0';
    }
    extra = fgetc(file);
    fclose(file);
    return length > 0 && extra == EOF;
}

static int wena_language_save(const char *path, const char *value)
{
    char temporary[512];
    int failed;
#if defined(_WIN32)
    HANDLE file;
    DWORD written;
    size_t length;
#else
    FILE *file;
    int descriptor;
#endif
    if (path == NULL || value == NULL ||
        strlen(path) + 5 >= sizeof(temporary) ||
        !wena_language_path_valid(path, 1)) return 0;
    strcpy(temporary, path);
    strcat(temporary, ".tmp");
    /* A collision is not ours to truncate or remove, including dangling links. */
#if defined(_WIN32)
    file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    length = strlen(value);
    failed = !WriteFile(file, value, (DWORD)length, &written, NULL) ||
        written != (DWORD)length;
    if (!failed) failed = !WriteFile(file, "\n", 1, &written, NULL) || written != 1;
    if (!failed && !FlushFileBuffers(file)) failed = 1;
    if (!CloseHandle(file)) failed = 1;
#else
    descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) return 0;
    file = fdopen(descriptor, "wb");
    if (file == NULL) { close(descriptor); remove(temporary); return 0; }
    failed = fprintf(file, "%s\n", value) < 0;
    if (fflush(file) != 0) failed = 1;
    if (!failed && fsync(descriptor) != 0) failed = 1;
    if (fclose(file) != 0) failed = 1;
#endif
    if (failed || !wena_language_path_valid(path, 1)) {
        remove(temporary);
        return 0;
    }
#if defined(_WIN32)
    if (!MoveFileExA(temporary, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    if (rename(temporary, path) != 0) {
#endif
        remove(temporary);
        return 0;
    }
    return 1;
}

int wena_language_init(WenaLanguageState *state, const char *settings_path,
                       const char *detected_locale,
                       const char *const *available, size_t available_count)
{
    char saved[64];
    if (state == NULL) {
        return 0;
    }
    memset(state, 0, sizeof(*state));
    if (wena_language_load(settings_path, saved, sizeof(saved)) &&
        wena_language_apply(state, saved, available, available_count, 1)) {
        return 1;
    }
    return wena_language_apply(state, detected_locale, available,
                               available_count, 0);
}

int wena_language_set(WenaLanguageState *state, const char *settings_path,
                      const char *requested, const char *const *available,
                      size_t available_count)
{
    WenaLanguageState changed;
    if (state == NULL) return 0;
    if (!wena_language_apply(&changed, requested, available, available_count, 1) ||
        !wena_language_save(settings_path, changed.current)) {
        return 0;
    }
    *state = changed;
    return 1;
}

int wena_language_clear(WenaLanguageState *state, const char *settings_path,
                        const char *detected_locale,
                        const char *const *available, size_t available_count)
{
    WenaLanguageState changed;
    if (state == NULL) return 0;
    if (!wena_language_apply(&changed, detected_locale, available,
                             available_count, 0)) {
        return 0;
    }
    if (settings_path != NULL) {
        if (!wena_language_path_valid(settings_path, 1)) return 0;
        if (remove(settings_path) != 0 && errno != ENOENT) return 0;
    }
    *state = changed;
    return 1;
}
