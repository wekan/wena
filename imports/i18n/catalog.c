#include "catalog.h"

#include <string.h>

#define WENA_I18N_FOOTER_SIZE 56L

static const unsigned char wena_footer_magic[16] = {
    'W', 'E', 'N', 'A', '-', 'I', '1', '8', 'N', '-', 'E', 'N', 'D', '-', 'v', '1'
};
static const unsigned char wena_catalog_magic[12] = {
    'W', 'E', 'N', 'A', '-', 'I', '1', '8', 'N', '-', '1', '\n'
};

int wena_i18n_catalog_open(WenaI18nCatalog *catalog, const char *executable_path)
{
    unsigned char footer[WENA_I18N_FOOTER_SIZE];
    unsigned char marker[sizeof(wena_catalog_magic)];
    unsigned long size;
    int index;
    long end;

    if (catalog == NULL || executable_path == NULL) {
        return 0;
    }
    memset(catalog, 0, sizeof(*catalog));
    catalog->file = fopen(executable_path, "rb");
    if (catalog->file == NULL || fseek(catalog->file, 0L, SEEK_END) != 0) {
        wena_i18n_catalog_close(catalog);
        return 0;
    }
    end = ftell(catalog->file);
    if (end < WENA_I18N_FOOTER_SIZE ||
        fseek(catalog->file, end - WENA_I18N_FOOTER_SIZE, SEEK_SET) != 0 ||
        fread(footer, 1, sizeof(footer), catalog->file) != sizeof(footer) ||
        memcmp(footer + 40, wena_footer_magic, sizeof(wena_footer_magic)) != 0) {
        wena_i18n_catalog_close(catalog);
        return 0;
    }
    size = 0UL;
    for (index = 32; index < 40; ++index) {
        if (size > (~0UL >> 8)) {
            wena_i18n_catalog_close(catalog);
            return 0;
        }
        size = (size << 8) | footer[index];
    }
    if (size < sizeof(marker) || size > (unsigned long)(end - WENA_I18N_FOOTER_SIZE)) {
        wena_i18n_catalog_close(catalog);
        return 0;
    }
    catalog->offset = end - WENA_I18N_FOOTER_SIZE - (long)size;
    catalog->size = size;
    if (fseek(catalog->file, catalog->offset, SEEK_SET) != 0 ||
        fread(marker, 1, sizeof(marker), catalog->file) != sizeof(marker) ||
        memcmp(marker, wena_catalog_magic, sizeof(marker)) != 0) {
        wena_i18n_catalog_close(catalog);
        return 0;
    }
    return 1;
}

void wena_i18n_catalog_close(WenaI18nCatalog *catalog)
{
    if (catalog != NULL && catalog->file != NULL) {
        fclose(catalog->file);
        catalog->file = NULL;
    }
}
