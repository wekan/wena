#include "../imports/i18n/language.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *const available[] = {"ar", "en", "fi", "fi-FI"};
    WenaLanguageState state;
    FILE *file;

    assert(argc == 2);
    assert(wena_language_init(&state, argv[1], "fi_FI.UTF-8", available, 4));
    assert(strcmp(state.current, "fi-FI") == 0);
    assert(!state.explicit_override && !state.rtl);

    assert(wena_language_set(&state, argv[1], "ar_EG", available, 4));
    assert(strcmp(state.current, "ar") == 0);
    assert(state.explicit_override && state.rtl);
    assert(wena_language_init(&state, argv[1], "fi", available, 4));
    assert(strcmp(state.current, "ar") == 0);
    assert(state.explicit_override && state.rtl);

    assert(wena_language_set(&state, argv[1], "unknown", available, 4));
    assert(strcmp(state.current, "en") == 0);
    assert(state.explicit_override && !state.rtl);
    assert(wena_language_clear(&state, argv[1], "fi_SE", available, 4));
    assert(strcmp(state.current, "fi") == 0);
    assert(!state.explicit_override && !state.rtl);

    file = fopen(argv[1], "wb");
    assert(file != NULL);
    assert(fputs("ar\nforged-extra\n", file) >= 0);
    assert(fclose(file) == 0);
    assert(wena_language_init(&state, argv[1], "fi", available, 4));
    assert(strcmp(state.current, "fi") == 0);
    assert(!state.explicit_override);
    remove(argv[1]);
    return 0;
}
