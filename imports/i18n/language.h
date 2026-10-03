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
/* Chooses a language without saving it anywhere: the caller stores it (in
 * WeKan, the user's profile.language). */
int wena_language_choose(WenaLanguageState *state, const char *requested,
                         const char *const *available, size_t available_count);
int wena_language_clear(WenaLanguageState *state, const char *settings_path,
                        const char *detected_locale,
                        const char *const *available, size_t available_count);

#endif
