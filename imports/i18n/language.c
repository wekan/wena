#include "language.h"

#include "locale.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

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
    if (path == NULL || value == NULL || capacity < 2) {
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
    FILE *file;
    int failed;
    if (path == NULL || value == NULL ||
        strlen(path) + 5 >= sizeof(temporary)) {
        return 0;
    }
    strcpy(temporary, path);
    strcat(temporary, ".tmp");
    file = fopen(temporary, "wb");
    if (file == NULL) {
        return 0;
    }
    failed = fprintf(file, "%s\n", value) < 0;
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed) {
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
    if (!wena_language_apply(&changed, detected_locale, available,
                             available_count, 0)) {
        return 0;
    }
    if (settings_path != NULL && remove(settings_path) != 0) {
        FILE *file;
        file = fopen(settings_path, "rb");
        if (file != NULL) {
            fclose(file);
            return 0;
        }
    }
    *state = changed;
    return 1;
}
