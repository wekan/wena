#include "../imports/ui/page_contract.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    const WenaUiPageContract *pages;
    const WenaUiColorContract *colors;
    const WenaUiControlContract *control;
    size_t page_count;
    size_t color_count;
    size_t index;
    unsigned int previous;

    assert(WENA_UI_BASELINE_CONTENT_TABLES == 1u);
    assert(WENA_UI_NATURAL_TAB_ORDER == 1u);

    pages = wena_ui_pages(&page_count);
    colors = wena_ui_colors(&color_count);
    assert(pages != NULL && page_count == 15);
    assert(strcmp(pages[0].route_family, "/") == 0);
    assert(strcmp(pages[14].route_family, "/b/:boardId/:slug") == 0);
    assert(colors != NULL && color_count == 50);
    assert(strcmp(colors[0].name, "belize") == 0);
    assert(strcmp(colors[0].rgb, "#2980b9") == 0);

    previous = 0u;
    for (index = WENA_UI_BOARD_MENU; index <= WENA_UI_CLOSE; ++index) {
        control = wena_ui_control((WenaUiControlId)index);
        assert(control != NULL);
        assert(control->i18n_key[0] != '\0');
        assert(control->ascii_control[0] == '[');
        assert(control->tab_order > previous);
        previous = control->tab_order;
    }
    assert(strcmp(wena_ui_control(WENA_UI_ADD_CARD)->http_method, "POST") == 0);
    assert(strcmp(wena_ui_control(WENA_UI_ADD_CARD)->domain_operation, "create-card") == 0);
    assert(strcmp(wena_ui_control(WENA_UI_ARCHIVE_CARD)->http_method, "POST") == 0);
    assert(strcmp(wena_ui_control(WENA_UI_ARCHIVE_CARD)->domain_operation, "archive-card") == 0);
    assert(wena_ui_control((WenaUiControlId)999) == NULL);
    return 0;
}
