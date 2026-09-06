#ifndef WENA_I18N_LANGUAGE_H
#define WENA_I18N_LANGUAGE_H

#include <stddef.h>

typedef struct WenaLanguageState {
    char current[64];
    int explicit_override;
    int rtl;
} WenaLanguageState;

int wena_language_init(WenaLanguageState *state, const char *settings_path,
                       const char *detected_locale,
                       const char *const *available, size_t available_count);
int wena_language_set(WenaLanguageState *state, const char *settings_path,
                      const char *requested, const char *const *available,
                      size_t available_count);
int wena_language_clear(WenaLanguageState *state, const char *settings_path,
                        const char *detected_locale,
                        const char *const *available, size_t available_count);

#endif
