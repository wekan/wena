#include "nuklear.h"
#include <stddef.h>
#include <string.h>

struct nk_rect nk_rect(float x, float y, float w, float h)
{
    struct nk_rect rectangle;

    rectangle.x = x;
    rectangle.y = y;
    rectangle.w = w;
    rectangle.h = h;
    return rectangle;
}

int nk_begin(struct nk_context *context, const char *title,
             struct nk_rect bounds, unsigned int flags)
{
    (void)title;
    (void)bounds;
    (void)flags;
    ++context->begin_count;
    return 1;
}

void nk_end(struct nk_context *context)
{
    ++context->end_count;
}

void nk_layout_row_dynamic(struct nk_context *context, float height, int columns)
{
    (void)context;
    (void)height;
    (void)columns;
}

void nk_layout_row_begin(struct nk_context *context, int format,
                         float row_height, int columns)
{
    (void)context;
    (void)format;
    (void)row_height;
    (void)columns;
}

void nk_layout_row_push(struct nk_context *context, float value)
{
    (void)context;
    (void)value;
}

void nk_layout_row_end(struct nk_context *context)
{
    (void)context;
}

void nk_label(struct nk_context *context, const char *text, int alignment)
{
    (void)alignment;
    context->labels[context->label_count++] = text;
}

int nk_button_label(struct nk_context *context, const char *title)
{
    int result;

    ++context->button_count;
    result = context->button_to_press != NULL &&
             strcmp(context->button_to_press, title) == 0;
    if (result) {
        context->button_to_press = NULL;
    }
    return result;
}

int nk_group_begin(struct nk_context *context, const char *title,
                   unsigned int flags)
{
    (void)title;
    (void)flags;
    ++context->group_depth;
    return 1;
}

void nk_group_end(struct nk_context *context)
{
    --context->group_depth;
}

int nk_filter_default(const struct nk_text_edit *edit, nk_rune rune)
{
    (void)edit; (void)rune; return 1;
}

unsigned int nk_edit_string(struct nk_context *context, unsigned int flags,
    char *buffer, int *length, int max, nk_plugin_filter filter)
{
    size_t size;
    (void)flags; (void)filter;
    ++context->edit_count;
    if (context->edit_text != NULL) {
        size = strlen(context->edit_text);
        if (size >= (size_t)max) size = (size_t)max - 1u;
        memcpy(buffer, context->edit_text, size);
        *length = (int)size;
        context->edit_text = NULL;
    }
    return 0u;
}

