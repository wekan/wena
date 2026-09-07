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
typedef struct WenaHtml4MoveFields { const char *object_name;const char *object_id;
 const char *target_name;const char *target_id;const char *target_swimlane_id;
 unsigned long expected_version;unsigned long target_position; } WenaHtml4MoveFields;

int wena_html4_render_page(const WenaRootUrl *root, const WenaHtml4Page *page,
                           char *output, size_t capacity);
int wena_html4_render_post_form(const WenaRootUrl *root, const char *route_path,
                                const char *operation, const char *session_token,
                                const char *csrf_token,
                                const char *label, const char *ascii_control,
                                char *output, size_t capacity);
int wena_html4_render_move_control(const WenaRootUrl *root, const char *route_path,
                                   const char *control_id, const char *operation,
                                   const char *session_token, const char *csrf_token,
                                   const char *label, const char *ascii_control,
                                   char *output, size_t capacity);
int wena_html4_render_move_control_fields(const WenaRootUrl *root,const char *route_path,
 const char *control_id,const char *operation,const char *session_token,
 const char *csrf_token,const WenaHtml4MoveFields *fields,const char *label,
 const char *ascii_control,char *output,size_t capacity);

#endif
