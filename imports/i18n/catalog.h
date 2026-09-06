#ifndef WENA_I18N_CATALOG_H
#define WENA_I18N_CATALOG_H

#include <stdio.h>

typedef struct WenaI18nCatalog {
    FILE *file;
    long offset;
    unsigned long size;
} WenaI18nCatalog;

int wena_i18n_catalog_open(WenaI18nCatalog *catalog, const char *executable_path);
void wena_i18n_catalog_close(WenaI18nCatalog *catalog);

#endif
