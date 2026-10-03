#include "badges.h"
#include "component.h"
#include "../../components/cards/card_body.h"
#include "../../components/common/wekan_look.h"
#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <string.h>

/* WeKan's .card-label: bold 13px text, 3px 8px padding, at most 210px, 4px
 * apart, flowing over as many rows as they need. */
#define CHIP_HEIGHT 22.0f
#define CHIP_PADDING 24.0f   /* 8px each side, the border and the button's own text padding */
#define CHIP_MAX 210.0f

static float chip_width(struct nk_context *context, const char *name, float available)
{
    const struct nk_user_font *face = wena_wekan_font(context, WENA_WEKAN_FONT_BOLD);
    float width = face != NULL ? face->width(face->userdata, face->height, name, (int)strlen(name)) : 0.0f;
    width += CHIP_PADDING;
    if (width < 24.0f) width = 24.0f;
    if (width > CHIP_MAX) width = CHIP_MAX;
    return width > available ? available : width;
}

static int assigned(const unsigned char *assignments, size_t index)
{
    return (assignments[index / 8u] & (1u << (index % 8u))) != 0u;
}

unsigned int wena_label_badges_render(struct nk_context *context,
    const WenaLabelBoardSnapshot *snapshot, const WenaCard *card)
{
    const unsigned char *assignments;
    const WenaLabel *labels;
    size_t index, next, in_row;
    unsigned int action;
    struct nk_rect row;
    float available, used, width, spacing;
    if (context == NULL || card == NULL || card->archived || snapshot == NULL)
        return WENA_CARD_BODY_NO_ACTION;
    assignments = wena_label_board_assignments(snapshot, card->board_id, card->id);
    if (assignments == NULL) return WENA_CARD_BODY_NO_ACTION;
    labels = snapshot->catalogue.labels;
    for (index = 0; index < snapshot->catalogue.label_count && !assigned(assignments, index); ++index) {}
    if (index == snapshot->catalogue.label_count) return WENA_CARD_BODY_NO_ACTION;
    action = WENA_CARD_BODY_NO_ACTION;
    nk_layout_row_dynamic(context, 1.0f, 1);
    row = nk_widget_bounds(context);
    nk_spacer(context); /* one slot: nk_spacing wraps to a new row */
    available = row.w > 1.0f ? row.w : 1.0f;
    spacing = context->style.window.spacing.x;
    while (index < snapshot->catalogue.label_count) {
        /* How many of the next chips fit this row (always at least one). */
        used = 0.0f; in_row = 0;
        for (next = index; next < snapshot->catalogue.label_count; ++next) {
            if (!assigned(assignments, next)) continue;
            width = chip_width(context, labels[next].name, available);
            if (in_row > 0 && used + spacing + width > available) break;
            used += (in_row > 0 ? spacing : 0.0f) + width;
            ++in_row;
        }
        nk_layout_row_begin(context, NK_STATIC, CHIP_HEIGHT, (int)in_row);
        for (; index < next; ++index) {
            if (!assigned(assignments, index)) continue;
            nk_layout_row_push(context, chip_width(context, labels[index].name, available));
            if (wena_label_badge_render(context, labels[index].name, labels[index].color))
                action |= WENA_CARD_BODY_OPEN_LABELS;
        }
        nk_layout_row_end(context);
        while (index < snapshot->catalogue.label_count && !assigned(assignments, index)) ++index;
    }
    return action;
}
