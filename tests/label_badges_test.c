#include "../client/features/labels/badges.h"
#include "../client/components/cards/card_body.h"
#include <nuklear.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    WenaLabelBoardSnapshot *snapshot;
    WenaCard card;
    struct nk_context context;
    size_t index;
    char id[20];
    snapshot = wena_label_board_snapshot_create();
    assert(snapshot != NULL);
    strcpy(snapshot->catalogue.board_id, "board");
    snapshot->catalogue.board_version = 1;
    snapshot->card_count = 2;
    strcpy(snapshot->card_ids[0], "card1");
    strcpy(snapshot->card_ids[1], "card2");
    snapshot->card_versions[0] = 1;
    snapshot->card_versions[1] = 1;
    snapshot->catalogue.label_count = WENA_BOARD_LABEL_CAPACITY;
    for (index = 0; index < WENA_BOARD_LABEL_CAPACITY; ++index) {
        sprintf(id, "label%03lu", (unsigned long)index);
        assert(wena_label_init(&snapshot->catalogue.labels[index], id,
            "board", id, index == 0 ? "" : "blue", (unsigned long)index));
        snapshot->catalogue.label_versions[index] = 1;
    }
    snapshot->assignments[0][0] = 1;
    snapshot->assignments[1][WENA_LABEL_ASSIGNMENT_BYTES - 1] = 128;
    snapshot->catalogue.assigned_card_counts[0] = 1;
    snapshot->catalogue.assigned_card_counts[127] = 1;
    assert(wena_label_board_snapshot_valid(snapshot, "board"));
    assert(wena_card_init(&card, "card1", "board", "lane", "list", "Card", 0, 0));
    memset(&context, 0, sizeof(context));
    assert(wena_label_badges_render(&context, snapshot, &card) == 0);
    assert(context.label_count == 1 && !strcmp(context.labels[0], "label000"));
    memset(&context, 0, sizeof(context));
    context.button_to_press = "label000";
    assert(wena_label_badges_render(&context, snapshot, &card) == WENA_CARD_BODY_OPEN_LABELS);
    strcpy(card.id, "card2");
    memset(&context, 0, sizeof(context));
    assert(wena_label_badges_render(&context, snapshot, &card) == 0);
    assert(context.label_count == 1 && !strcmp(context.labels[0], "label127"));
    card.archived = 1;
    memset(&context, 0, sizeof(context));
    assert(wena_label_badges_render(&context, snapshot, &card) == 0);
    assert(context.label_count == 0);
    card.archived = 0;
    strcpy(card.board_id, "foreign");
    assert(wena_label_badges_render(&context, snapshot, &card) == 0);
    assert(context.label_count == 0);
    strcpy(card.board_id, "board");
    strcpy(card.id, "missing");
    assert(wena_label_badges_render(&context, snapshot, &card) == 0);
    assert(context.label_count == 0);
    assert(wena_label_badges_render(NULL, snapshot, &card) == 0);
    assert(wena_label_badges_render(&context, NULL, &card) == 0);
    assert(wena_label_badges_render(&context, snapshot, NULL) == 0);
    strcpy(card.id, "card1");
    memset(snapshot->assignments[0], 255, WENA_LABEL_ASSIGNMENT_BYTES);
    memset(&context, 0, sizeof(context));
    assert(wena_label_badges_render(&context, snapshot, &card) == 0);
    assert(context.label_count == WENA_BOARD_LABEL_CAPACITY);
    assert(!strcmp(context.labels[0], "label000"));
    assert(!strcmp(context.labels[127], "label127"));
    wena_label_board_snapshot_free(snapshot);
    puts("label badges: scoped cached drawing, empty/default color, high bit, full capacity and click intent passed");
    return 0;
}
