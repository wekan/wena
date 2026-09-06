#include "legacy_html4.h"

#include "root_url.h"
#include "../imports/ui/page_contract.h"

#include <string.h>

typedef struct WenaWriter {
    char *output;
    size_t capacity;
    size_t length;
    int valid;
} WenaWriter;

static void wena_write(WenaWriter *writer, const char *value)
{
    size_t length;
    if (!writer->valid || value == NULL) { writer->valid = 0; return; }
    length = strlen(value);
    if (length >= writer->capacity - writer->length) { writer->valid = 0; return; }
    memcpy(writer->output + writer->length, value, length);
    writer->length += length;
    writer->output[writer->length] = '\0';
}

static void wena_escape(WenaWriter *writer, const char *value)
{
    const unsigned char *cursor;
    if (value == NULL) { writer->valid = 0; return; }
    for (cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        if (*cursor == '&') wena_write(writer, "&amp;");
        else if (*cursor == '<') wena_write(writer, "&lt;");
        else if (*cursor == '>') wena_write(writer, "&gt;");
        else if (*cursor == '"') wena_write(writer, "&quot;");
        else if (*cursor == '\'') wena_write(writer, "&#39;");
        else {
            char character[2];
            character[0] = (char)*cursor;
            character[1] = '\0';
            wena_write(writer, character);
        }
    }
}

static void wena_writer_init(WenaWriter *writer, char *output, size_t capacity)
{
    writer->output = output;
    writer->capacity = capacity;
    writer->length = 0;
    writer->valid = output != NULL && capacity > 0;
    if (writer->valid) output[0] = '\0';
}

static const char *wena_theme(const char *name)
{
    const WenaUiColorContract *colors;
    size_t count;
    size_t index;
    colors = wena_ui_colors(&count);
    for (index = 0; index < count; ++index)
        if (name != NULL && strcmp(colors[index].name, name) == 0) return colors[index].rgb;
    return "#ffffff";
}

int wena_html4_render_page(const WenaRootUrl *root, const WenaHtml4Page *page,
                           char *output, size_t capacity)
{
    WenaWriter writer;
    char canonical[WENA_SERVER_ROOT_URL_CAPACITY + 128];
    char capability[WENA_SERVER_ROOT_URL_CAPACITY + 128];
    size_t index;
    wena_writer_init(&writer, output, capacity);
    if (page == NULL || page->rows == NULL ||
        !wena_root_url_join(root, page->route_path, canonical, sizeof(canonical))) {
        if (output != NULL && capacity > 0) output[0] = '\0';
        return 0;
    }
    wena_write(&writer, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html lang=\"");
    wena_escape(&writer, page->language); wena_write(&writer, "\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><title>");
    wena_escape(&writer, page->title);
    wena_write(&writer, "</title><script type=\"text/javascript\" src=\"");
    if (!wena_root_url_join(root, "/legacy-html4-capabilities.js", capability,
                            sizeof(capability))) writer.valid = 0;
    wena_escape(&writer, capability);
    wena_write(&writer, "\"></script></head><body style=\"background-color:");
    wena_write(&writer, wena_theme(page->theme_name)); wena_write(&writer, "\"><h1>");
    wena_escape(&writer, page->heading); wena_write(&writer, "</h1><table summary=\"");
    wena_escape(&writer, page->heading); wena_write(&writer, "\"><caption>");
    wena_escape(&writer, page->heading); wena_write(&writer, "</caption><tbody>");
    for (index = 0; index < page->row_count; ++index) {
        wena_write(&writer, "<tr><th scope=\"row\">"); wena_escape(&writer, page->rows[index].heading);
        wena_write(&writer, "</th><td>"); wena_escape(&writer, page->rows[index].content);
        wena_write(&writer, "</td></tr>");
    }
    wena_write(&writer, "</tbody></table><p><a href=\""); wena_escape(&writer, canonical);
    wena_write(&writer, "\">[R] "); wena_escape(&writer, page->heading);
    wena_write(&writer, "</a></p></body></html>");
    if (!writer.valid) { if (output != NULL && capacity > 0) output[0] = '\0'; return 0; }
    return 1;
}

int wena_html4_render_post_form(const WenaRootUrl *root, const char *route_path,
                                const char *operation, const char *csrf_token,
                                const char *label, const char *ascii_control,
                                char *output, size_t capacity)
{
    WenaWriter writer;
    char action[WENA_SERVER_ROOT_URL_CAPACITY + 128];
    wena_writer_init(&writer, output, capacity);
    action[0] = '\0';
    if (operation == NULL || operation[0] == '\0' || csrf_token == NULL ||
        csrf_token[0] == '\0' || !wena_root_url_join(root, route_path, action, sizeof(action))) {
        if (output != NULL && capacity > 0) output[0] = '\0';
        return 0;
    }
    wena_write(&writer, "<form method=\"post\" action=\""); wena_escape(&writer, action);
    wena_write(&writer, "\"><input type=\"hidden\" name=\"legacyOperation\" value=\"");
    wena_escape(&writer, operation); wena_write(&writer, "\"><input type=\"hidden\" name=\"csrf\" value=\"");
    wena_escape(&writer, csrf_token); wena_write(&writer, "\"><button type=\"submit\">");
    wena_escape(&writer, ascii_control); wena_write(&writer, " "); wena_escape(&writer, label);
    wena_write(&writer, "</button></form>");
    if (!writer.valid) { if (output != NULL && capacity > 0) output[0] = '\0'; return 0; }
    return 1;
}
