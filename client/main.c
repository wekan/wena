#include <stdio.h>
#include "../imports/i18n/catalog.h"

int main(int argc, char **argv)
{
    WenaI18nCatalog catalog;

    if (argc < 1 || !wena_i18n_catalog_open(&catalog, argv[0])) {
        fputs("Wena offline translation catalog is missing or invalid\n", stderr);
        return 1;
    }
    wena_i18n_catalog_close(&catalog);
    puts("WeKan Native");
    return 0;
}
