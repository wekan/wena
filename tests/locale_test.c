#include "../imports/i18n/locale.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    const char *const languages[] = {"ar", "en", "fi", "fi-FI", "pt-BR", "zh-CN"};
    char value[64];

    assert(wena_locale_normalize("FI_fi.UTF-8@euro", value, sizeof(value)));
    assert(strcmp(value, "fi-FI") == 0);
    assert(wena_locale_normalize("pt-BR", value, sizeof(value)));
    assert(strcmp(value, "pt-BR") == 0);
    assert(wena_locale_normalize("zh_hant_tw", value, sizeof(value)));
    assert(strcmp(value, "zh-Hant-TW") == 0);
    assert(!wena_locale_normalize("C", value, sizeof(value)));
    assert(!wena_locale_normalize("fi/../../x", value, sizeof(value)));
    assert(!wena_locale_normalize("fi__FI", value, sizeof(value)));
    assert(!wena_locale_normalize("fi-FI", value, 3));

    assert(wena_locale_resolve("fi_FI.UTF-8", languages, 6, value, sizeof(value)));
    assert(strcmp(value, "fi-FI") == 0);
    assert(wena_locale_resolve("pt_PT", languages, 6, value, sizeof(value)));
    assert(strcmp(value, "en") == 0);
    assert(wena_locale_resolve("zh_TW", languages, 6, value, sizeof(value)));
    assert(strcmp(value, "en") == 0);
    assert(wena_locale_resolve("fi_SE", languages, 6, value, sizeof(value)));
    assert(strcmp(value, "fi") == 0);
    assert(wena_locale_resolve(NULL, languages, 6, value, sizeof(value)));
    assert(strcmp(value, "en") == 0);
    assert(!wena_locale_resolve("fi", languages, 0, value, sizeof(value)));

    assert(wena_locale_is_rtl("ar-EG"));
    assert(wena_locale_is_rtl("he_IL.UTF-8"));
    assert(!wena_locale_is_rtl("fi-FI"));
    return 0;
}
