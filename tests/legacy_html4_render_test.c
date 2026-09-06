#include "../server/legacy_html4.h"
#include "../server/root_url.h"
#include "../imports/ui/page_contract.h"

#include <assert.h>
#include <string.h>

static int occurrences(const char *value, const char *needle)
{
    int count;
    size_t length;
    count = 0;
    length = strlen(needle);
    while ((value = strstr(value, needle)) != NULL) {
        ++count;
        value += length;
    }
    return count;
}

int main(void)
{
    WenaRootUrl root;
    WenaHtml4Row rows[2];
    WenaHtml4Page page;
    char output[4096];
    char url[256];
    const WenaUiControlContract *add_card;

    assert(wena_server_root_url_parse("https://kanban.example:8443/team", &root));
    assert(wena_root_url_join(&root, "/b/board-1/demo", url, sizeof(url)));
    assert(strcmp(url, "https://kanban.example:8443/team/b/board-1/demo") == 0);
    assert(!wena_root_url_join(&root, "//evil.example/x", url, sizeof(url)));
    assert(!wena_root_url_join(&root, "/b/../admin", url, sizeof(url)));
    assert(!wena_root_url_join(&root, "/b/%2e%2e/admin", url, sizeof(url)));
    assert(!wena_root_url_join(&root, "/b/x?next=https://evil.example", url, sizeof(url)));
    assert(!wena_root_url_join(&root, "/b/x#evil", url, sizeof(url)));

    rows[0].heading = "List <one>";
    rows[0].content = "Card & task";
    rows[1].heading = "Owner";
    rows[1].content = "\"Alice\" <script>alert(1)</script>";
    page.language = "fi\" onload=\"evil";
    page.title = "Board <demo>";
    page.heading = "Kanban & work";
    page.route_path = "/b/board-1/demo";
    page.theme_name = "belize";
    page.rows = rows;
    page.row_count = 2;
    assert(wena_html4_render_page(&root, &page, output, sizeof(output)));
    assert(strstr(output, "HTML 4.01") != NULL);
    assert(occurrences(output, "<table ") == 1);
    assert(strstr(output, "background-color:#2980b9") != NULL);
    assert(strstr(output, "src=\"https://kanban.example:8443/team/legacy-html4-capabilities.js\"") != NULL);
    assert(strstr(output, "<script>") == NULL);
    assert(strstr(output, "&lt;script&gt;alert(1)&lt;/script&gt;") != NULL);
    assert(strstr(output, "href=\"https://kanban.example:8443/team/b/board-1/demo\"") != NULL);

    add_card = wena_ui_control(WENA_UI_ADD_CARD);
    assert(add_card != NULL && strcmp(add_card->http_method, "POST") == 0);
    assert(wena_html4_render_post_form(&root, "/b/board-1/demo", add_card->domain_operation,
                                       "token&amp;forged", "Add <card>", add_card->ascii_control,
                                       output, sizeof(output)));
    assert(strstr(output, "method=\"post\"") != NULL);
    assert(strstr(output, "action=\"https://kanban.example:8443/team/b/board-1/demo\"") != NULL);
    assert(strstr(output, "token&amp;amp;forged") != NULL);
    assert(strstr(output, "Add &lt;card&gt;") != NULL);
    assert(!wena_html4_render_post_form(&root, "/b/board-1/demo", "create-card",
                                        "", "Add", "[+]", output, sizeof(output)));
    assert(output[0] == '\0');
    assert(!wena_html4_render_page(&root, &page, output, 32));
    assert(output[0] == '\0');
    return 0;
}
