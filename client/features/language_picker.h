#ifndef WENA_LANGUAGE_PICKER_H
#define WENA_LANGUAGE_PICKER_H

#include "../../imports/i18n/language.h"

#define WENA_LANGUAGE_PICKER_CAPACITY 512
#define WENA_LANGUAGE_PICKER_PATH_CAPACITY 512

struct nk_context;

typedef struct WenaLanguagePicker {
    WenaLanguageState *language;
    const char *items[WENA_LANGUAGE_PICKER_CAPACITY];
    size_t count;
    size_t selected;
    char settings_path[WENA_LANGUAGE_PICKER_PATH_CAPACITY];
    int writable;
    int error;
} WenaLanguagePicker;

/* Caller keeps language alive. Paths are copied; catalog pointers are static. */
int wena_language_picker_init(WenaLanguagePicker *state,
    WenaLanguageState *language, const char *settings_path, int readonly);
/* A same-language selection succeeds without rewriting settings. Failed
 * selection never changes language, persisted settings, or selected index. */
int wena_language_picker_select(WenaLanguagePicker *state, size_t index);
/* Toolbar callback: caller owns the current Nuklear window. */
void wena_language_picker_render(struct nk_context *context, void *state);

#endif
