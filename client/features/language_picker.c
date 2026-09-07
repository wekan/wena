#include "language_picker.h"

#include "../../imports/i18n/ui_catalog.h"
#include "../../imports/ui/page_contract.h"

#include <nuklear.h>
#include <string.h>

int wena_language_picker_init(WenaLanguagePicker *state,
    WenaLanguageState *language, const char *settings_path, int readonly)
{
    const char *const *languages;
    size_t count, i, length;
    if (state == NULL) return 0;
    memset(state, 0, sizeof(*state));
    if (language == NULL ||
        memchr(language->current, '\0', sizeof(language->current)) == NULL)
        return 0;
    languages = wena_ui_catalog_languages(&count);
    if (count == 0 || count > WENA_LANGUAGE_PICKER_CAPACITY) return 0;
    length = 0;
    if (settings_path != NULL) {
        while (length < sizeof(state->settings_path) && settings_path[length])
            ++length;
        /* The existing atomic settings writer adds a ".tmp" suffix. */
        if (length + 5 >= sizeof(state->settings_path)) return 0;
    }
    if (!readonly && length == 0) return 0;
    state->selected = count;
    for (i = 0; i < count; ++i) {
        state->items[i] = languages[i];
        if (strcmp(languages[i], language->current) == 0) state->selected = i;
    }
    if (state->selected == count) {
        memset(state, 0, sizeof(*state));
        return 0;
    }
    state->language = language;
    state->count = count;
    state->writable = !readonly;
    if (length) memcpy(state->settings_path, settings_path, length + 1);
    return 1;
}

int wena_language_picker_select(WenaLanguagePicker *state, size_t index)
{
    if (state == NULL) return 0;
    if (state->language == NULL || !state->writable || index >= state->count ||
        state->count > WENA_LANGUAGE_PICKER_CAPACITY) {
        state->error = 1;
        return 0;
    }
    if (strcmp(state->language->current, state->items[index]) == 0) {
        state->selected = index;
        state->error = 0;
        return 1;
    }
    if (!wena_language_set(state->language, state->settings_path,
                           state->items[index], state->items, state->count)) {
        state->error = 1;
        return 0;
    }
    state->selected = index;
    state->error = 0;
    return 1;
}

void wena_language_picker_render(struct nk_context *context, void *opaque)
{
    WenaLanguagePicker *state;
    int selected;
    size_t i;
    state = (WenaLanguagePicker *)opaque;
    if (context == NULL || state == NULL || state->language == NULL ||
        state->count == 0 || state->count > WENA_LANGUAGE_PICKER_CAPACITY ||
        state->selected >= state->count) return;
    /* Keep the toolbar current if a caller changed the shared language state. */
    for (i = 0; i < state->count; ++i)
        if (strcmp(state->items[i], state->language->current) == 0)
            state->selected = i;
    nk_layout_row_begin(context, NK_STATIC, 26.0f, 2);
    nk_layout_row_push(context, 130.0f);
    nk_label(context, wena_ui_text(WENA_UI_TEXT_LANGUAGE), NK_TEXT_LEFT);
    nk_layout_row_push(context, 130.0f);
    if (state->writable) {
        selected = nk_combo(context, state->items, (int)state->count,
                             (int)state->selected, 24, nk_vec2(160.0f, 220.0f));
        if (selected >= 0 && (size_t)selected != state->selected)
            wena_language_picker_select(state, (size_t)selected);
    } else {
        nk_label(context, state->items[state->selected], NK_TEXT_LEFT);
    }
    nk_layout_row_end(context);
    if (state->error) {
        nk_layout_row_dynamic(context, 24.0f, 1);
        nk_label(context, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED), NK_TEXT_LEFT);
    }
}
