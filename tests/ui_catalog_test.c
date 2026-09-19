#include "../imports/i18n/ui_catalog.h"
#include "../imports/i18n/locale.h"
#include "../imports/ui/page_contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *const *languages;
    const char *translated, *retained;
    WenaLanguageState state, loaded;
    size_t count, i;
    char resolved[64], key[128];
    const unsigned char *cursor;
    assert(argc == 2);
    languages = wena_ui_catalog_languages(&count);
    assert(languages != NULL && count == 246);
    assert(wena_ui_catalog_languages(NULL) == languages);
    assert(wena_ui_catalog_lookup(NULL, NULL) == NULL);
    assert(wena_ui_catalog_lookup(NULL, "not-a-canonical-key") == NULL);
    assert(strcmp(wena_ui_catalog_lookup(NULL, "save"), "Save") == 0);
    memset(&state, 'x', sizeof(state));
    assert(strcmp(wena_ui_catalog_lookup(&state, "save"), "Save") == 0);
    assert(wena_language_init(&state, argv[1], "fi_FI.UTF-8", languages, count));
    assert(strcmp(state.current, "fi") == 0);
    assert(strcmp(wena_ui_catalog_lookup(&state, "save"), "Tallenna") == 0);
    wena_ui_set_translator(wena_ui_catalog_translate, &state);
    assert(strcmp(wena_ui_control_text(WENA_UI_SAVE), "Tallenna") == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_LANGUAGE), "Kieli") == 0);
    assert(strcmp(wena_ui_control_text(WENA_UI_OPEN_LABELS),
                  wena_ui_catalog_lookup(&state, "cardLabelsPopup-title")) == 0);
    assert(strcmp(wena_ui_control_text(WENA_UI_ADD_LABEL),
                  wena_ui_catalog_lookup(&state, "label-create")) == 0);
    assert(strcmp(wena_ui_control_text(WENA_UI_EDIT_LABEL),
                  wena_ui_catalog_lookup(&state, "editLabelPopup-title")) == 0);
    assert(strcmp(wena_ui_control_text(WENA_UI_CREATE_LABEL),
                  wena_ui_catalog_lookup(&state, "create")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_NAME),
                  wena_ui_catalog_lookup(&state, "name")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_SELECT_COLOR),
                  wena_ui_catalog_lookup(&state, "select-color")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_CUSTOM_COLOR),
                  wena_ui_catalog_lookup(&state, "custom-color")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_CREATE_LABEL),
                  wena_ui_catalog_lookup(&state, "createLabelPopup-title")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_EDIT_LABEL),
                  wena_ui_catalog_lookup(&state, "editLabelPopup-title")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_DELETE_LABEL),
                  wena_ui_catalog_lookup(&state, "deleteLabelPopup-title")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_CARDS),
                  wena_ui_catalog_lookup(&state, "cards")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_CHECKLIST_SPLIT_LINES),
                  wena_ui_catalog_lookup(&state, "newlineBecomesNewChecklistItem")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_CHECKLIST_COUNT_ON_MINICARD),
                  wena_ui_catalog_lookup(&state, "checklist-count-on-minicard")) == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_MOVE_SELECTION),
                  wena_ui_catalog_lookup(&state, "move-selection")) == 0);
    assert(strcmp(wena_ui_control(WENA_UI_CREATE_LABEL)->http_method, "GET") == 0);
    assert(strcmp(wena_ui_control(WENA_UI_OPEN_DELETE_LABEL)->domain_operation,
                  "open-delete-label") == 0);
    retained = wena_ui_catalog_lookup(&state, "save");
    assert(wena_language_set(&state, argv[1], "ar", languages, count));
    assert(state.rtl);
    assert(strcmp(wena_ui_control_text(WENA_UI_SAVE),
                  wena_ui_catalog_lookup(&state, "save")) == 0);
    assert(strcmp(wena_ui_catalog_lookup(&state, "save"), "Save") != 0);
    assert(strcmp(retained, "Tallenna") == 0);
    assert(wena_ui_catalog_translate(&state, "save") == wena_ui_catalog_lookup(&state, "save"));
    assert(wena_language_init(&loaded, argv[1], "en", languages, count));
    assert(strcmp(loaded.current, "ar") == 0 && loaded.explicit_override);
    assert(wena_language_set(&state, argv[1], "unknown-language", languages, count));
    assert(strcmp(state.current, "en") == 0);
    for (i = 0; i < count; ++i) {
        assert(wena_locale_resolve(languages[i], languages, count, resolved, sizeof(resolved)));
        assert(strcmp(resolved, languages[i]) == 0);
        assert(wena_language_set(&state, argv[1], languages[i], languages, count));
        assert(strcmp(state.current, languages[i]) == 0);
        assert(wena_language_init(&loaded, argv[1], "en", languages, count));
        assert(strcmp(loaded.current, languages[i]) == 0);
        translated = wena_ui_catalog_lookup(&state, "save");
        assert(translated != NULL && translated[0] != '\0');
    }
    assert(wena_language_clear(&state, argv[1], "fi", languages, count));
    assert(strcmp(state.current, "fi") == 0 && !state.explicit_override);
    assert(!wena_locale_resolve("fi", NULL, count, resolved, sizeof(resolved)));
    wena_ui_set_translator(NULL, NULL);
    assert(strcmp(wena_ui_control_text(WENA_UI_SAVE), "Save") == 0);
    assert(strcmp(wena_ui_text(WENA_UI_TEXT_DELETE_LABEL), "Delete Label?") == 0);
    assert(strcmp(wena_ui_control_text(WENA_UI_OPEN_LABELS), "Labels") == 0);
    assert(strcmp(wena_ui_control_text(WENA_UI_ADD_LABEL), "Create Label") == 0);
    /* Input keys supplied by the Python verifier come from page_contract.c.
       Dump all compiled UTF-8 bytes for exact comparison with canonical data. */
    while (fgets(key, sizeof(key), stdin) != NULL) {
        key[strcspn(key, "\r\n")] = '\0';
        for (i = 0; i < count; ++i) {
            memset(&state, 0, sizeof(state));
            strcpy(state.current, languages[i]);
            translated = wena_ui_catalog_lookup(&state, key);
            assert(translated != NULL);
            printf("%s\t%s\t", languages[i], key);
            cursor = (const unsigned char *)translated;
            while (*cursor) printf("%02x", (unsigned int)*cursor++);
            putchar('\n');
        }
    }
    return 0;
}
