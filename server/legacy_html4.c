#include "legacy_html4.h"

#include "root_url.h"
#include "../imports/ui/page_contract.h"

#include <string.h>
#include <stdio.h>

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
    char capability_css[WENA_SERVER_ROOT_URL_CAPACITY + 128];
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
    wena_write(&writer, "</title><link rel=\"stylesheet\" type=\"text/css\" href=\"");
    if (!wena_root_url_join(root, "/legacy-html4-capabilities.css", capability_css,
                            sizeof(capability_css))) writer.valid = 0;
    wena_escape(&writer, capability_css);
    wena_write(&writer, "\"><script type=\"text/javascript\" src=\"");
    if (!wena_root_url_join(root, "/legacy-html4-capabilities.js", capability,
                            sizeof(capability))) writer.valid = 0;
    wena_escape(&writer, capability);
    wena_write(&writer, "\"></script></head><body style=\"background-color:");
    wena_write(&writer, wena_theme(page->theme_name)); wena_write(&writer, "\"><h1>");
    wena_escape(&writer, page->heading);
    wena_write(&writer, "</h1><div id=\"wena-region-board\" class=\"wena-visible-region\" data-wena-region=\"board\" data-wena-version=\"1\"><table summary=\"");
    wena_escape(&writer, page->heading); wena_write(&writer, "\"><caption>");
    wena_escape(&writer, page->heading); wena_write(&writer, "</caption><tbody>");
    for (index = 0; index < page->row_count; ++index) {
        wena_write(&writer, "<tr><th scope=\"row\">"); wena_escape(&writer, page->rows[index].heading);
        wena_write(&writer, "</th><td>"); wena_escape(&writer, page->rows[index].content);
        wena_write(&writer, "</td></tr>");
    }
    wena_write(&writer, "</tbody></table></div><p><a href=\""); wena_escape(&writer, canonical);
    wena_write(&writer, "\">[R] "); wena_escape(&writer, page->heading);
    wena_write(&writer, "</a></p></body></html>");
    if (!writer.valid) { if (output != NULL && capacity > 0) output[0] = '\0'; return 0; }
    return 1;
}

int wena_html4_render_post_form(const WenaRootUrl *root, const char *route_path,
                                const char *operation, const char *session_token,
                                const char *csrf_token,
                                const char *label, const char *ascii_control,
                                char *output, size_t capacity)
{
    WenaWriter writer;
    char action[WENA_SERVER_ROOT_URL_CAPACITY + 128];
    wena_writer_init(&writer, output, capacity);
    action[0] = '\0';
    if (operation == NULL || operation[0] == '\0' || session_token == NULL ||
        session_token[0] == '\0' || csrf_token == NULL ||
        csrf_token[0] == '\0' || !wena_root_url_join(root, route_path, action, sizeof(action))) {
        if (output != NULL && capacity > 0) output[0] = '\0';
        return 0;
    }
    wena_write(&writer, "<form method=\"post\" action=\""); wena_escape(&writer, action);
    wena_write(&writer, "\"><input type=\"hidden\" name=\"legacySession\" value=\"");
    wena_escape(&writer, session_token);
    wena_write(&writer, "\"><input type=\"hidden\" name=\"legacyOperation\" value=\"");
    wena_escape(&writer, operation); wena_write(&writer, "\"><input type=\"hidden\" name=\"csrf\" value=\"");
    wena_escape(&writer, csrf_token); wena_write(&writer, "\"><button type=\"submit\">");
    wena_escape(&writer, ascii_control); wena_write(&writer, " "); wena_escape(&writer, label);
    wena_write(&writer, "</button></form>");
    if (!writer.valid) { if (output != NULL && capacity > 0) output[0] = '\0'; return 0; }
    return 1;
}

static int wena_control_id_valid(const char *value)
{
    const unsigned char *cursor;
    if (value == NULL || value[0] == '\0' || strlen(value) > 64) return 0;
    for (cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        if (!( (*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z') ||
               (*cursor >= '0' && *cursor <= '9') || *cursor == '-' || *cursor == '_')) return 0;
    return 1;
}

int wena_html4_render_move_control(const WenaRootUrl *root, const char *route_path,
                                   const char *control_id, const char *operation,
                                   const char *session_token, const char *csrf_token,
                                   const char *label, const char *ascii_control,
                                   char *output, size_t capacity)
{
    WenaWriter writer;
    char action[WENA_SERVER_ROOT_URL_CAPACITY + 128];
    wena_writer_init(&writer, output, capacity);
    if (!wena_control_id_valid(control_id) || operation == NULL || operation[0] == '\0' ||
        session_token == NULL || session_token[0] == '\0' || csrf_token == NULL ||
        csrf_token[0] == '\0' || !wena_root_url_join(root, route_path, action, sizeof(action)))
        return 0;
    wena_write(&writer, "<form class=\"wena-move-baseline\" id=\""); wena_escape(&writer, control_id);
    wena_write(&writer, "-baseline\" method=\"post\" action=\""); wena_escape(&writer, action);
    wena_write(&writer, "\"><input type=\"hidden\" name=\"legacySession\" value=\""); wena_escape(&writer, session_token);
    wena_write(&writer, "\"><input type=\"hidden\" name=\"legacyOperation\" value=\""); wena_escape(&writer, operation);
    wena_write(&writer, "\"><input type=\"hidden\" name=\"csrf\" value=\""); wena_escape(&writer, csrf_token);
    wena_write(&writer, "\"><button type=\"submit\">"); wena_escape(&writer, ascii_control);
    wena_write(&writer, " "); wena_escape(&writer, label); wena_write(&writer, "</button></form><button type=\"button\" class=\"wena-drag-control wena-drag-source wena-drop-target\" id=\"");
    wena_escape(&writer, control_id); wena_write(&writer, "-drag\" data-wena-form=\"");
    wena_escape(&writer, control_id); wena_write(&writer, "-baseline\">");
    wena_escape(&writer, ascii_control); wena_write(&writer, " "); wena_escape(&writer, label);
    wena_write(&writer, "</button>");
    if (!writer.valid) { if (output != NULL && capacity > 0) output[0] = '\0'; return 0; }
    return 1;
}

static int wena_field_name_valid(const char *name)
{return name!=NULL&&(strcmp(name,"cardId")==0||strcmp(name,"listId")==0||strcmp(name,"swimlaneId")==0||strcmp(name,"targetListId")==0);}

int wena_html4_render_move_control_fields(const WenaRootUrl *root,const char *route_path,const char *control_id,const char *operation,const char *session_token,const char *csrf_token,const WenaHtml4MoveFields *fields,const char *label,const char *ascii_control,char *output,size_t capacity)
{
    char base[4096],marker[128],extra[1024],version[32],position[32];const char *at;size_t prefix,suffix;
    if(!fields||!wena_field_name_valid(fields->object_name)||!wena_control_id_valid(fields->object_id)||fields->expected_version==0ul)return 0;
    if(strcmp(operation,"move-card")==0){if(strcmp(fields->object_name,"cardId")!=0||fields->target_name==NULL||strcmp(fields->target_name,"targetListId")!=0||!wena_control_id_valid(fields->target_id)||!wena_control_id_valid(fields->target_swimlane_id))return 0;}
    else if(strcmp(operation,"move-list")==0){if(strcmp(fields->object_name,"listId")!=0||fields->target_position>99999ul)return 0;}
    else if(strcmp(operation,"move-swimlane")==0){if(strcmp(fields->object_name,"swimlaneId")!=0||fields->target_position>99999ul)return 0;}else return 0;
    if(!wena_html4_render_move_control(root,route_path,control_id,operation,session_token,csrf_token,label,ascii_control,base,sizeof(base)))return 0;
    sprintf(marker,"<button type=\"submit\">");at=strstr(base,marker);if(!at)return 0;prefix=(size_t)(at-base);suffix=strlen(at);sprintf(version,"%lu",fields->expected_version);sprintf(position,"%lu",fields->target_position);
    extra[0]='\0';strcat(extra,"<input type=\"hidden\" name=\"");strcat(extra,fields->object_name);strcat(extra,"\" value=\"");strcat(extra,fields->object_id);strcat(extra,"\"><input type=\"hidden\" name=\"expectedVersion\" value=\"");strcat(extra,version);strcat(extra,"\">");
    if(strcmp(operation,"move-card")==0){strcat(extra,"<input type=\"hidden\" name=\"targetListId\" value=\"");strcat(extra,fields->target_id);strcat(extra,"\"><input type=\"hidden\" name=\"targetSwimlaneId\" value=\"");strcat(extra,fields->target_swimlane_id);strcat(extra,"\">");}else{strcat(extra,"<input type=\"hidden\" name=\"targetPosition\" value=\"");strcat(extra,position);strcat(extra,"\">");}
    if(prefix+strlen(extra)+suffix+1u>capacity){if(output&&capacity)output[0]='\0';return 0;}memcpy(output,base,prefix);strcpy(output+prefix,extra);strcpy(output+prefix+strlen(extra),at);return 1;
}
