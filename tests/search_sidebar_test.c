/* WeKan's Search sidebar (client/components/sidebar/search_sidebar.c). */
#include "../client/components/sidebar/search_sidebar.h"
#include "../client/components/common/wekan_look.h"
#include "../client/platform/nuklear_options.h"
#include <nuklear.h>
#include <assert.h>
#include <string.h>

static struct nk_context context;

static int control(const char *name)
{
    size_t count, i;
    const WenaUiControl *controls = wena_ui_controls(&count);
    for (i = 0; i < count; ++i) if (!strcmp(controls[i].name, name)) return 1;
    return 0;
}

int main(void)
{
    WenaSearchSidebar search;
    WenaList lists[2];
    WenaCard cards[2];
    char card[WENA_ID_CAPACITY];
    unsigned int action;
    memset(&search, 0, sizeof(search));
    assert(wena_list_init(&lists[0], "l1", "b1", "", "Release plan", 0.0, 0));
    assert(wena_list_init(&lists[1], "l2", "b1", "", "Backlog", 1.0, 0));
    assert(wena_card_init(&cards[0], "c1", "b1", "s1", "l1", "Write the notes", 0.0, 0));
    assert(wena_card_init(&cards[1], "c2", "b1", "s1", "l1", "Fix the build", 1.0, 0));

    /* Negative: closed, nothing is drawn. */
    wena_ui_controls_begin();
    assert(wena_search_sidebar_render(&context, &search, lists, 2, cards, 2, 1024.0f, 720.0f, card, sizeof(card)) ==
           WENA_SEARCH_NO_ACTION && !control("Search"));
    /* Open: WeKan's title, the field with its placeholder, Lists and Cards. */
    wena_search_sidebar_open(&search);
    assert(search.visible && search.focus);
    wena_ui_controls_begin();
    assert(wena_search_sidebar_render(&context, &search, lists, 2, cards, 2, 1024.0f, 720.0f, card, sizeof(card)) ==
           WENA_SEARCH_NO_ACTION);
    assert(control("Search") && control("Write text you search and press Enter") && control("Lists") &&
           control("Cards") && !search.focus);
    /* Enter searches for what was written. */
    context.edit_text = "notes";
    context.edit_commit = 1;
    action = wena_search_sidebar_render(&context, &search, lists, 2, cards, 2, 1024.0f, 720.0f, card, sizeof(card));
    assert(action == WENA_SEARCH_SUBMIT && !strcmp(search.term, "notes"));
    /* The results by their titles; a found card opens. An id that is not
     * on the board (deleted since) is left out. */
    search.searched = 1;
    strcpy(search.lists[0], "l1");
    search.list_count = 1;
    strcpy(search.cards[0], "gone");
    strcpy(search.cards[1], "c2");
    search.card_count = 2;
    wena_ui_controls_begin();
    context.button_to_press = "Fix the build";
    action = wena_search_sidebar_render(&context, &search, lists, 2, cards, 2, 1024.0f, 720.0f, card, sizeof(card));
    assert(action == WENA_SEARCH_OPEN_CARD && !strcmp(card, "c2"));
    assert(control("Release plan") && !control("Backlog") && !control("Write the notes"));
    /* The cross closes it. */
    context.button_to_press = "Close";
    assert(wena_search_sidebar_render(&context, &search, lists, 2, cards, 2, 1024.0f, 720.0f, card, sizeof(card)) ==
           WENA_SEARCH_CLOSE && !search.visible);
    /* Negative: no context or no room. */
    search.visible = 1;
    assert(wena_search_sidebar_render(NULL, &search, lists, 2, cards, 2, 1024.0f, 720.0f, card, sizeof(card)) ==
           WENA_SEARCH_NO_ACTION);
    assert(wena_search_sidebar_render(&context, &search, lists, 2, cards, 2, 0.0f, 720.0f, card, sizeof(card)) ==
           WENA_SEARCH_NO_ACTION);
    return 0;
}
