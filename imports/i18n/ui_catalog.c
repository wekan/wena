#include "ui_catalog.h"
#include "locale.h"

#include <string.h>

#include "ui_catalog_data.h"

const char *const *wena_ui_catalog_languages(size_t *count)
{
    if (count != NULL) *count = WENA_UI_CATALOG_LANGUAGE_COUNT;
    return ui_languages;
}

const char *wena_ui_catalog_lookup(const WenaLanguageState *state,
                                  const char *key)
{
    size_t index, key_index, language_index, english_index;
    const char *requested;
    char resolved[64];
    if (key == NULL) return NULL;
    key_index = WENA_UI_CATALOG_KEY_COUNT;
    for (index = 0; index < WENA_UI_CATALOG_KEY_COUNT; ++index) {
        if (strcmp(key, ui_keys[index]) == 0) {
            key_index = index;
            break;
        }
    }
    if (key_index == WENA_UI_CATALOG_KEY_COUNT) return NULL;
    requested = NULL;
    if (state != NULL && memchr(state->current, '\0', sizeof(state->current)) != NULL)
        requested = state->current;
    if (!wena_locale_resolve(requested, ui_languages,
                             WENA_UI_CATALOG_LANGUAGE_COUNT,
                             resolved, sizeof(resolved))) return NULL;
    language_index = 0;
    english_index = 0;
    for (index = 0; index < WENA_UI_CATALOG_LANGUAGE_COUNT; ++index) {
        if (strcmp(ui_languages[index], resolved) == 0) language_index = index;
        if (strcmp(ui_languages[index], "en") == 0) english_index = index;
    }
    return ui_values[language_index][key_index][0] == '\0' ?
           ui_values[english_index][key_index] : ui_values[language_index][key_index];
}

const char *wena_ui_catalog_translate(void *context, const char *key)
{
    return wena_ui_catalog_lookup((const WenaLanguageState *)context, key);
}
