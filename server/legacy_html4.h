#ifndef WENA_SERVER_LEGACY_HTML4_H
#define WENA_SERVER_LEGACY_HTML4_H

#include "settings.h"

#include <stddef.h>

typedef struct WenaHtml4Row {
    const char *heading;
    const char *content;
} WenaHtml4Row;

typedef struct WenaHtml4Page {
    const char *language;
    const char *title;
    const char *heading;
    const char *route_path;
    const char *theme_name;
    const WenaHtml4Row *rows;
    size_t row_count;
} WenaHtml4Page;

int wena_html4_render_page(const WenaRootUrl *root, const WenaHtml4Page *page,
                           char *output, size_t capacity);
int wena_html4_render_post_form(const WenaRootUrl *root, const char *route_path,
                                const char *operation, const char *csrf_token,
                                const char *label, const char *ascii_control,
                                char *output, size_t capacity);

#endif
