#include "locale.h"

#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__amigaos__) || defined(__AROS__)
#include <proto/exec.h>
#include <proto/locale.h>
#endif

static int wena_copy(char *output, size_t capacity, const char *value)
{
    size_t length;
    if (output == NULL || capacity == 0 || value == NULL) {
        return 0;
    }
    length = strlen(value);
    if (length == 0 || length >= capacity) {
        output[0] = '\0';
        return 0;
    }
    memcpy(output, value, length + 1);
    return 1;
}

int wena_locale_normalize(const char *input, char *output, size_t capacity)
{
    size_t source;
    size_t destination;
    size_t segment;
    unsigned char character;

    if (output == NULL || capacity == 0) {
        return 0;
    }
    output[0] = '\0';
    if (input == NULL || input[0] == '\0' || strcmp(input, "C") == 0 ||
        strcmp(input, "POSIX") == 0) {
        return 0;
    }
    destination = 0;
    segment = 0;
    for (source = 0; input[source] != '\0' && input[source] != '.' &&
                     input[source] != '@'; ++source) {
        character = (unsigned char)input[source];
        if (character == '_' || character == '-') {
            if (segment == 0 || destination + 1 >= capacity) {
                output[0] = '\0';
                return 0;
            }
            output[destination++] = '-';
            segment = 0;
            continue;
        }
        if (!isalnum(character) || destination + 1 >= capacity) {
            output[0] = '\0';
            return 0;
        }
        output[destination++] = (char)tolower(character);
        ++segment;
    }
    if (segment == 0 || destination == 0) {
        output[0] = '\0';
        return 0;
    }
    output[destination] = '\0';
    {
        char *part;
        char *next;
        int part_index;
        part = output;
        part_index = 0;
        while (part != NULL) {
            size_t length;
            size_t index;
            next = strchr(part, '-');
            length = next == NULL ? strlen(part) : (size_t)(next - part);
            if (part_index > 0 && length == 2) {
                for (index = 0; index < length; ++index) {
                    part[index] = (char)toupper((unsigned char)part[index]);
                }
            } else if (part_index > 0 && length == 4) {
                part[0] = (char)toupper((unsigned char)part[0]);
            }
            part = next == NULL ? NULL : next + 1;
            ++part_index;
        }
    }
    return 1;
}

#if defined(__amigaos__) || defined(__AROS__)
/* locale.library names a language after its catalog drawer, in that language
 * and in ISO-8859-1 ("fran\347ais"); these are the ones AmigaOS and AROS ship. */
static const char *const wena_amiga_languages[][2] = {
    {"english", "en"}, {"deutsch", "de"}, {"fran\347ais", "fr"}, {"italiano", "it"},
    {"espa\361ol", "es"}, {"catal\340", "ca"}, {"portugu\352s", "pt"},
    {"portugu\352s-brasil", "pt-BR"}, {"nederlands", "nl"}, {"dansk", "da"},
    {"norsk", "nb"}, {"svenska", "sv"}, {"suomi", "fi"}, {"polski", "pl"},
    {"czech", "cs"}, {"\350e\271tina", "cs"}, {"magyar", "hu"}, {"greek", "el"},
    {"russian", "ru"}, {"srpski", "sr"}, {"hrvatski", "hr"}, {"slovensko", "sl"},
    {"turkish", "tr"}, {"t\374rk\347e", "tr"}
};

/* The user's first preferred language in Locale prefs that Wena can name. */
static int wena_amiga_locale(char *output, size_t capacity)
{
#if defined(__amigaos4__)
    struct Library *LocaleBase;
    struct LocaleIFace *ILocale;
#else
    struct LocaleBase *LocaleBase;
#endif
    struct Locale *locale;
    size_t preference, index, length;
    int found = 0;
#if defined(__amigaos4__)
    LocaleBase = IExec->OpenLibrary("locale.library", 38);
    if (LocaleBase == NULL) return 0;
    ILocale = (struct LocaleIFace *)IExec->GetInterface(LocaleBase, "main", 1, NULL);
    locale = ILocale == NULL ? NULL : ILocale->OpenLocale(NULL);
#else
    LocaleBase = (struct LocaleBase *)OpenLibrary((CONST_STRPTR)"locale.library", 38);
    if (LocaleBase == NULL) return 0;
    locale = OpenLocale(NULL);
#endif
    for (preference = 0; locale != NULL && !found && preference < 10 &&
         locale->loc_PrefLanguages[preference] != NULL; ++preference) {
        const char *name = (const char *)locale->loc_PrefLanguages[preference];
        /* "deutsch" or "deutsch.language" */
        length = strlen(name);
        if (length > 9 && strcmp(name + length - 9, ".language") == 0) length -= 9;
        for (index = 0; !found && index < sizeof(wena_amiga_languages) /
             sizeof(wena_amiga_languages[0]); ++index) {
            const char *known = wena_amiga_languages[index][0];
            size_t i;
            for (i = 0; i < length && known[i] != '\0' &&
                 tolower((unsigned char)name[i]) == (unsigned char)known[i]; ++i) {
            }
            if (i == length && known[i] == '\0')
                found = wena_locale_normalize(wena_amiga_languages[index][1], output, capacity);
        }
    }
#if defined(__amigaos4__)
    if (locale != NULL) ILocale->CloseLocale(locale);
    if (ILocale != NULL) IExec->DropInterface((struct Interface *)ILocale);
    IExec->CloseLibrary(LocaleBase);
#else
    if (locale != NULL) CloseLocale(locale);
    CloseLibrary((struct Library *)LocaleBase);
#endif
    return found;
}
#endif

int wena_locale_detect(char *output, size_t capacity)
{
    const char *detected;
#if defined(_WIN32)
    wchar_t wide[LOCALE_NAME_MAX_LENGTH];
    char utf8[LOCALE_NAME_MAX_LENGTH * 4];
    int length;
    if (GetUserDefaultLocaleName(wide, LOCALE_NAME_MAX_LENGTH) > 0) {
        length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8,
                                     (int)sizeof(utf8), NULL, NULL);
        if (length > 0 && wena_locale_normalize(utf8, output, capacity)) {
            return 1;
        }
    }
#elif defined(__amigaos__) || defined(__AROS__)
    if (wena_amiga_locale(output, capacity)) {
        return 1;
    }
#endif
    /* POSIX covers Linux/BSD/macOS/iOS/Android. AmigaOS and AROS use
       LANGUAGE/LANG when locale.library names no language Wena knows. */
    detected = setlocale(LC_ALL, "");
    if (detected != NULL && wena_locale_normalize(detected, output, capacity)) {
        return 1;
    }
    detected = getenv("LANGUAGE");
    if (detected == NULL || detected[0] == '\0') {
        detected = getenv("LC_ALL");
    }
    if (detected == NULL || detected[0] == '\0') {
        detected = getenv("LC_MESSAGES");
    }
    if (detected == NULL || detected[0] == '\0') {
        detected = getenv("LANG");
    }
    return wena_locale_normalize(detected, output, capacity);
}

static const char *wena_find(const char *candidate,
                             const char *const *available, size_t count)
{
    size_t index;
    for (index = 0; index < count; ++index) {
        if (strcmp(candidate, available[index]) == 0) {
            return available[index];
        }
    }
    return NULL;
}

int wena_locale_resolve(const char *requested, const char *const *available,
                        size_t available_count, char *output, size_t capacity)
{
    char normalized[64];
    char base[64];
    char *separator;
    const char *match;
    if (available == NULL || available_count == 0) return 0;
    /* Canonical tags include underscore and modifier spellings. Preserve an
       exact catalog selection before applying OS-locale normalization. */
    match = requested == NULL ? NULL :
            wena_find(requested, available, available_count);
    if (!wena_locale_normalize(requested, normalized, sizeof(normalized))) {
        normalized[0] = '\0';
    }
    if (match == NULL && normalized[0] != '\0') {
        match = wena_find(normalized, available, available_count);
    }
    if (match == NULL && normalized[0] != '\0') {
        size_t index;
        char candidate[64];
        /* Catalog tags retain canonical spelling. Resolve normalized OS
           locales against those spellings in deterministic inventory order. */
        for (index = 0; index < available_count; ++index) {
            if (wena_locale_normalize(available[index], candidate, sizeof(candidate)) &&
                strcmp(candidate, normalized) == 0) {
                match = available[index];
                break;
            }
        }
    }
    if (match == NULL && normalized[0] != '\0') {
        wena_copy(base, sizeof(base), normalized);
        separator = strchr(base, '-');
        if (separator != NULL) {
            *separator = '\0';
            match = wena_find(base, available, available_count);
        }
    }
    if (match == NULL) {
        match = wena_find("en", available, available_count);
    }
    return match != NULL && wena_copy(output, capacity, match);
}

int wena_locale_is_rtl(const char *language_tag)
{
    char normalized[64];
    char *separator;
    static const char *const rtl[] = {
        "ar", "arc", "ckb", "dv", "fa", "he", "ks", "ku", "ps", "sd", "ug", "ur", "yi"
    };
    size_t index;
    if (!wena_locale_normalize(language_tag, normalized, sizeof(normalized))) {
        return 0;
    }
    separator = strchr(normalized, '-');
    if (separator != NULL) {
        *separator = '\0';
    }
    for (index = 0; index < sizeof(rtl) / sizeof(rtl[0]); ++index) {
        if (strcmp(normalized, rtl[index]) == 0) {
            return 1;
        }
    }
    return 0;
}
