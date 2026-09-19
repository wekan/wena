#include "../client/features/checklists/badges.h"
#include "../client/components/cards/card_body.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    WenaChecklistBoardSummary *summary;
    WenaCard card;
    struct nk_context context;
    summary = wena_checklist_summary_create();
    assert(summary != NULL);
    strcpy(summary->board_id, "board");
    summary->card_count = 1;
    strcpy(summary->cards[0].card_id, "card");
    assert(wena_card_init(&card, "card", "board", "lane", "list", "Card", 0, 0));
    memset(&context, 0, sizeof(context));
    summary->cards[0].checklist_count = 1;
    assert(wena_checklist_badges_render(&context, summary, &card) == 0);
    assert(context.label_count == 0 && context.button_count == 0);
    summary->enabled = 1;
    context.button_to_press = "0/0";
    assert(wena_checklist_badges_render(&context, summary, &card) == WENA_CARD_BODY_OPEN_CHECKLISTS);
    assert(context.label_count == 1 && !strcmp(context.labels[0], "Checklists"));
    summary->cards[0].progress.total = 1024;
    summary->cards[0].progress.finished = 1024;
    memset(&context, 0, sizeof(context));
    context.button_to_press = "1024/1024";
    assert(wena_checklist_badges_render(&context, summary, &card) == WENA_CARD_BODY_OPEN_CHECKLISTS);
    summary->cards[0].progress.finished = 3;
    context.button_to_press = "3/1024";
    assert(wena_checklist_badges_render(&context, summary, &card) == WENA_CARD_BODY_OPEN_CHECKLISTS);
    memset(&context, 0, sizeof(context));
    summary->cards[0].checklist_count = 0;
    assert(wena_checklist_badges_render(&context, summary, &card) == 0);
    assert(context.button_count == 0);
    summary->cards[0].checklist_count = 1;
    strcpy(card.board_id, "foreign");
    assert(wena_checklist_badges_render(&context, summary, &card) == 0);
    strcpy(card.board_id, "board");
    strcpy(card.id, "absent");
    assert(wena_checklist_badges_render(&context, summary, &card) == 0);
    strcpy(card.id, "card");
    card.archived = 1;
    assert(wena_checklist_badges_render(&context, summary, &card) == 0);
    card.archived = 0;
    summary->cards[0].archived = 1;
    assert(wena_checklist_badges_render(&context, summary, &card) == 0);
    assert(context.button_count == 0 && context.label_count == 0);
    assert(wena_checklist_badges_render(NULL, summary, &card) == 0);
    assert(wena_checklist_badges_render(&context, NULL, &card) == 0);
    assert(wena_checklist_badges_render(&context, summary, NULL) == 0);
    wena_checklist_summary_free(summary);
    puts("checklist badges: default off, exact count/click, empty checklist, scope and archive guards passed");
    return 0;
}
