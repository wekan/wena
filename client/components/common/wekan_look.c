#include "wekan_look.h"

#include "../../platform/nuklear_options.h"
#include <nuklear.h>
#include <stdio.h>
#include <string.h>

/* Measured from WeKan's board at 1024x720 (tests/fixtures/wekan-ui and
 * the screenshots they came with); tests/test_wekan_ui_parity.py compares. */
static const char *const colors[WENA_WEKAN_COLOR_COUNT] = {
    "#dedede", /* BODY: body, list */
    "#2980b9", /* HEADER: #header-quick-access */
    "#ffffff", /* HEADER_TEXT */
    "#f2f2f2", /* HEADER_LINK: .board-header-btn */
    "#cccccc", /* SWIMLANE_HEADER: .swimlane-header-wrap */
    "#dedede", /* LIST: .js-list */
    "#e4e4e4", /* LIST_HEADER: .list-header */
    "#cccccc", /* LIST_BORDER: between lists */
    "#ffffff", /* MINICARD: .minicard */
    "#f7f7f7", /* MINICARD_OPEN: .minicard while its details are open */
    "#4d4d4d", /* MINICARD_TEXT: .minicard-title */
    "#d0d0d0", /* MINICARD_SHADOW */
    "#000000", /* TEXT: list and swimlane titles, menu items */
    "#a6a6a6", /* ICON: Font Awesome icons */
    "#666666", /* ICON_ACTIVE */
    "#8c8c8c", /* ADD_CARD: .open-minicard-composer */
    "#ffffff", /* POPUP: .js-pop-over */
    "#dbdbdb", /* POPUP_BORDER */
    "#f7f7f7", /* POPUP_HEADER */
    "#666666", /* POPUP_HEADER_TEXT */
    "#2e90d0", /* POPUP_HOVER */
    "#f7f7f7", /* PANEL: .js-card-details, .sidebar */
    "#808080", /* SECTION_TITLE: .card-details-item-title */
    "#2980b9", /* BUTTON: .button.primary */
    "#216694", /* BUTTON_ADD: the composer's Add */
    "#ffffff", /* BUTTON_TEXT */
    "#222222", /* INPUT_BORDER */
    "#ce1414"  /* WIP_EXCEEDED: .list-header .highlight */
};

const char *wena_wekan_color_hex(WenaWekanColor color)
{
    return (int)color >= 0 && color < WENA_WEKAN_COLOR_COUNT ? colors[color] : NULL;
}

static int hex(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0;
}

int wena_wekan_rgb(WenaWekanColor color)
{
    const char *value = wena_wekan_color_hex(color);
    if (value == NULL) return 0;
    return (hex(value[1]) << 20) | (hex(value[2]) << 16) | (hex(value[3]) << 12) |
           (hex(value[4]) << 8) | (hex(value[5]) << 4) | hex(value[6]);
}

static struct nk_color color_of(int color)
{
    /* A WenaWekanColor, or a raw 0xRRGGBB marked by 0x1000000 (a list's or
     * label's own color). */
    int rgb = color >= 0x1000000 ? (color & 0xffffff) :
              color >= 0 && color < WENA_WEKAN_COLOR_COUNT ? wena_wekan_rgb((WenaWekanColor)color) : 0;
    return nk_rgb((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

static const struct nk_user_font *fonts[WENA_WEKAN_FONT_COUNT];

void wena_wekan_font_set(WenaWekanFont which, const struct nk_user_font *font)
{
    if ((int)which >= 0 && which < WENA_WEKAN_FONT_COUNT) fonts[which] = font;
}

const struct nk_user_font *wena_wekan_font(struct nk_context *context, WenaWekanFont which)
{
    if ((int)which >= 0 && which < WENA_WEKAN_FONT_COUNT && fonts[which] != NULL) return fonts[which];
    return context != NULL ? context->style.font : NULL;
}

/* Controls ------------------------------------------------------------------ */

static WenaUiControl controls[WENA_UI_CONTROL_CAPACITY];
static size_t control_count;
static char current_region[24];

void wena_ui_controls_begin(void)
{
    control_count = 0;
    current_region[0] = '\0';
}

void wena_ui_region(const char *region)
{
    current_region[0] = '\0';
    if (region != NULL) strncat(current_region, region, sizeof(current_region) - 1);
}

void wena_ui_control_record(const char *region, const char *name,
                            float x, float y, float w, float h)
{
    WenaUiControl *control;
    if (name == NULL || name[0] == '\0' || control_count == WENA_UI_CONTROL_CAPACITY) return;
    control = &controls[control_count++];
    control->name[0] = '\0';
    strncat(control->name, name, sizeof(control->name) - 1);
    {
        const char *source = region != NULL ? region : current_region;
        size_t at = 0;
        while (source[at] != '\0' && at + 1 < sizeof(control->region)) {
            control->region[at] = source[at];
            at++;
        }
        control->region[at] = '\0';
    }
    control->x = x; control->y = y; control->w = w; control->h = h;
}

const WenaUiControl *wena_ui_controls(size_t *count)
{
    if (count != NULL) *count = control_count;
    return controls;
}

/* Icons --------------------------------------------------------------------- */

static void line(struct nk_command_buffer *out, float x0, float y0, float x1, float y1,
                 float width, struct nk_color color)
{
    nk_stroke_line(out, x0, y0, x1, y1, width, color);
}

/* (x, y) is the box's corner and s its side; coordinates below are in 16ths. */
#define P(v) ((v) * s / 16.0f)
void wena_wekan_icon_draw(struct nk_context *context, WenaIcon icon,
                          float x, float y, float s, int color)
{
    struct nk_command_buffer *out;
    struct nk_color c = color_of(color);
    float t = s / 9.0f, points[20];
    int i;
    if (context == NULL || s <= 0.0f || (out = nk_window_get_canvas(context)) == NULL) return;
    if (t < 1.2f) t = 1.2f;
    switch (icon) {
    case WENA_ICON_HOME:
        nk_fill_triangle(out, x + P(1), y + P(8), x + P(8), y + P(1), x + P(15), y + P(8), c);
        nk_fill_rect(out, nk_rect(x + P(3.5f), y + P(8), P(9), P(7)), 0, c);
        break;
    case WENA_ICON_ANGLES_LEFT:
        for (i = 0; i < 2; ++i) {
            float o = (float)i * P(6);
            line(out, x + P(8) + o - P(3), y + P(3), x + P(3) + o - P(1), y + P(8), t, c);
            line(out, x + P(3) + o - P(1), y + P(8), x + P(8) + o - P(3), y + P(13), t, c);
        }
        break;
    case WENA_ICON_CARET_DOWN:
        nk_fill_triangle(out, x + P(3), y + P(6), x + P(13), y + P(6), x + P(8), y + P(11), c);
        break;
    case WENA_ICON_CARET_RIGHT:
        nk_fill_triangle(out, x + P(6), y + P(3), x + P(6), y + P(13), x + P(11), y + P(8), c);
        break;
    case WENA_ICON_BARS:
        for (i = 0; i < 3; ++i)
            nk_fill_rect(out, nk_rect(x + P(1), y + P(3) + (float)i * P(4.5f), P(14), t), t / 2.0f, c);
        break;
    case WENA_ICON_PLUS:
        line(out, x + P(8), y + P(2), x + P(8), y + P(14), t * 0.8f, c);
        line(out, x + P(2), y + P(8), x + P(14), y + P(8), t * 0.8f, c);
        break;
    case WENA_ICON_PLUS_SQUARE:
        nk_stroke_rect(out, nk_rect(x + P(2), y + P(2), P(12), P(12)), P(2), t * 0.7f, c);
        line(out, x + P(8), y + P(5), x + P(8), y + P(11), t * 0.7f, c);
        line(out, x + P(5), y + P(8), x + P(11), y + P(8), t * 0.7f, c);
        break;
    case WENA_ICON_TIMES:
        line(out, x + P(3), y + P(3), x + P(13), y + P(13), t, c);
        line(out, x + P(13), y + P(3), x + P(3), y + P(13), t, c);
        break;
    case WENA_ICON_FILTER:
        nk_fill_triangle(out, x + P(1), y + P(2), x + P(15), y + P(2), x + P(8), y + P(9), c);
        nk_fill_rect(out, nk_rect(x + P(6.5f), y + P(8), P(3), P(6)), 0, c);
        break;
    case WENA_ICON_SEARCH:
        nk_stroke_circle(out, nk_rect(x + P(1.5f), y + P(1.5f), P(9), P(9)), t, c);
        line(out, x + P(9), y + P(9), x + P(14.5f), y + P(14.5f), t * 1.3f, c);
        break;
    case WENA_ICON_CHECK_SQUARE:
    case WENA_ICON_SQUARE:
        nk_stroke_rect(out, nk_rect(x + P(2), y + P(2), P(12), P(12)), P(2), t * 0.8f, c);
        if (icon == WENA_ICON_CHECK_SQUARE) {
            points[0] = x + P(4.5f); points[1] = y + P(8);
            points[2] = x + P(7); points[3] = y + P(10.5f);
            points[4] = x + P(11.5f); points[5] = y + P(5.5f);
            nk_stroke_polyline(out, points, 3, t * 0.8f, c);
        }
        break;
    case WENA_ICON_CHECK:
        points[0] = x + P(2.5f); points[1] = y + P(8.5f);
        points[2] = x + P(6); points[3] = y + P(12);
        points[4] = x + P(13.5f); points[5] = y + P(4);
        nk_stroke_polyline(out, points, 3, t, c);
        break;
    case WENA_ICON_GRID:
        nk_stroke_rect(out, nk_rect(x + P(1.5f), y + P(2.5f), P(13), P(11)), P(1), t * 0.7f, c);
        line(out, x + P(8), y + P(2.5f), x + P(8), y + P(13.5f), t * 0.7f, c);
        line(out, x + P(1.5f), y + P(8), x + P(14.5f), y + P(8), t * 0.7f, c);
        break;
    case WENA_ICON_GEAR:
        {
            /* Four spokes through the center make the eight teeth. */
            static const float spoke[4][2] = {{1.0f, 0.0f}, {0.7071f, 0.7071f}, {0.0f, 1.0f}, {-0.7071f, 0.7071f}};
            for (i = 0; i < 4; ++i)
                line(out, x + P(8) - spoke[i][0] * P(7), y + P(8) - spoke[i][1] * P(7),
                     x + P(8) + spoke[i][0] * P(7), y + P(8) + spoke[i][1] * P(7), t * 1.4f, c);
        }
        nk_fill_circle(out, nk_rect(x + P(3), y + P(3), P(10), P(10)), c);
        nk_fill_circle(out, nk_rect(x + P(6), y + P(6), P(4), P(4)), color_of(WENA_WEKAN_PANEL));
        break;
    case WENA_ICON_TAG:
        points[0] = x + P(1); points[1] = y + P(1);
        points[2] = x + P(8); points[3] = y + P(1);
        points[4] = x + P(15); points[5] = y + P(8);
        points[6] = x + P(8); points[7] = y + P(15);
        points[8] = x + P(1); points[9] = y + P(8);
        nk_fill_polygon(out, points, 5, c);
        nk_fill_circle(out, nk_rect(x + P(3.5f), y + P(3.5f), P(3), P(3)), color_of(WENA_WEKAN_PANEL));
        break;
    case WENA_ICON_USER:
    case WENA_ICON_USERS:
        nk_fill_circle(out, nk_rect(x + P(5), y + P(1), P(6), P(6)), c);
        nk_fill_arc(out, x + P(8), y + P(15), P(6.5f), 3.14159265f, 6.2831853f, c);
        if (icon == WENA_ICON_USERS) {
            nk_fill_circle(out, nk_rect(x + P(0), y + P(3), P(4), P(4)), c);
            nk_fill_circle(out, nk_rect(x + P(12), y + P(3), P(4), P(4)), c);
        }
        break;
    case WENA_ICON_ARCHIVE:
        nk_fill_rect(out, nk_rect(x + P(1), y + P(2), P(14), P(4)), P(1), c);
        nk_fill_rect(out, nk_rect(x + P(2), y + P(7), P(12), P(8)), P(1), c);
        nk_fill_rect(out, nk_rect(x + P(6), y + P(9), P(4), P(1.5f)), 0, color_of(WENA_WEKAN_POPUP));
        break;
    case WENA_ICON_ARROW_RIGHT:
        line(out, x + P(2), y + P(8), x + P(14), y + P(8), t, c);
        line(out, x + P(9), y + P(3), x + P(14), y + P(8), t, c);
        line(out, x + P(9), y + P(13), x + P(14), y + P(8), t, c);
        break;
    case WENA_ICON_ARROW_UP:
    case WENA_ICON_ARROW_DOWN: {
        float d = icon == WENA_ICON_ARROW_UP ? 1.0f : -1.0f, tip = icon == WENA_ICON_ARROW_UP ? P(2) : P(14);
        line(out, x + P(8), y + P(2), x + P(8), y + P(14), t, c);
        line(out, x + P(3), y + tip + d * P(5), x + P(8), y + tip, t, c);
        line(out, x + P(13), y + tip + d * P(5), x + P(8), y + tip, t, c);
        break;
    }
    case WENA_ICON_BRUSH:
        line(out, x + P(14), y + P(2), x + P(7), y + P(9), t * 1.5f, c);
        nk_fill_circle(out, nk_rect(x + P(2), y + P(8), P(6), P(6)), c);
        break;
    case WENA_ICON_LIST:
        for (i = 0; i < 3; ++i) {
            nk_fill_rect(out, nk_rect(x + P(1), y + P(2.5f) + (float)i * P(4.5f), P(2.5f), P(2.5f)), 0, c);
            nk_fill_rect(out, nk_rect(x + P(5), y + P(3) + (float)i * P(4.5f), P(10), t * 0.8f), 0, c);
        }
        break;
    case WENA_ICON_ALIGN_LEFT:
        for (i = 0; i < 4; ++i)
            nk_fill_rect(out, nk_rect(x + P(1), y + P(2) + (float)i * P(3.6f),
                                      i % 2 ? P(9) : P(14), t * 0.8f), 0, c);
        break;
    case WENA_ICON_BAN:
        nk_stroke_circle(out, nk_rect(x + P(1.5f), y + P(1.5f), P(13), P(13)), t, c);
        line(out, x + P(3.5f), y + P(3.5f), x + P(12.5f), y + P(12.5f), t, c);
        break;
    case WENA_ICON_GLOBE:
        nk_stroke_circle(out, nk_rect(x + P(1.5f), y + P(1.5f), P(13), P(13)), t * 0.8f, c);
        nk_stroke_circle(out, nk_rect(x + P(5), y + P(1.5f), P(6), P(13)), t * 0.7f, c);
        line(out, x + P(1.5f), y + P(8), x + P(14.5f), y + P(8), t * 0.7f, c);
        break;
    case WENA_ICON_PENCIL:
        line(out, x + P(3), y + P(13), x + P(13), y + P(3), t * 1.6f, c);
        nk_fill_triangle(out, x + P(1.5f), y + P(14.5f), x + P(2), y + P(11), x + P(5), y + P(14), c);
        break;
    case WENA_ICON_REFRESH:
    case WENA_ICON_HISTORY:
        nk_stroke_arc(out, x + P(8), y + P(8), P(6), 0.6f, 5.6f, t, c);
        nk_fill_triangle(out, x + P(12), y + P(1.5f), x + P(15.5f), y + P(6), x + P(10.5f), y + P(6), c);
        if (icon == WENA_ICON_HISTORY) {
            line(out, x + P(8), y + P(4.5f), x + P(8), y + P(8), t * 0.8f, c);
            line(out, x + P(8), y + P(8), x + P(10.5f), y + P(10), t * 0.8f, c);
        }
        break;
    case WENA_ICON_WINDOW_MAXIMIZE:
        /* fa-window-maximize: a window with a thick title bar. */
        nk_stroke_rect(out, nk_rect(x + P(1.5f), y + P(2.5f), P(13), P(11)), P(1), t * 0.8f, c);
        nk_fill_rect(out, nk_rect(x + P(1.5f), y + P(2.5f), P(13), P(3)), P(1), c);
        break;
    case WENA_ICON_WINDOW_MINIMIZE:
        /* fa-window-minimize: a bar at the bottom. */
        nk_fill_rect(out, nk_rect(x + P(2), y + P(11), P(12), P(2.5f)), P(1), c);
        break;
    case WENA_ICON_STAR:
    case WENA_ICON_STAR_O: {
        /* fa-star / fa-star-o: five points, filled or drawn. */
        /* The unit star's ten points, outer then inner (radius 3.2/7.5). */
        static const float unit[10][2] = {{0.0f, -1.0f}, {0.2508f, -0.3452f}, {0.9511f, -0.309f}, {0.4058f, 0.1318f}, {0.5878f, 0.809f}, {0.0f, 0.4267f}, {-0.5878f, 0.809f}, {-0.4058f, 0.1318f}, {-0.9511f, -0.309f}, {-0.2508f, -0.3452f}};
        float cx = x + P(8), cy = y + P(8.5f), r = P(7.5f);
        int k;
        for (k = 0; k < 10; ++k) {
            points[k * 2] = cx + r * unit[k][0];
            points[k * 2 + 1] = cy + r * unit[k][1];
        }
        if (icon == WENA_ICON_STAR) nk_fill_polygon(out, points, 10, c);
        else {
            for (k = 0; k < 10; ++k)
                line(out, points[k * 2], points[k * 2 + 1], points[((k + 1) % 10) * 2], points[((k + 1) % 10) * 2 + 1],
                     t * 0.7f, c);
        }
        break;
    }
    case WENA_ICON_FOLDER:
        nk_fill_rect(out, nk_rect(x + P(1), y + P(3), P(6), P(3)), P(1), c);
        nk_fill_rect(out, nk_rect(x + P(1), y + P(5), P(14), P(9)), P(1), c);
        break;
    case WENA_ICON_CLIPBOARD:
        nk_stroke_rect(out, nk_rect(x + P(3), y + P(3), P(10), P(12)), P(1), t * 0.8f, c);
        nk_fill_rect(out, nk_rect(x + P(5.5f), y + P(1.5f), P(5), P(3)), P(1), c);
        break;
    case WENA_ICON_FILE_TEXT_O:
        /* fa-file-text-o: a page with a folded corner and three lines. */
        nk_stroke_rect(out, nk_rect(x + P(3), y + P(1), P(10), P(14)), P(0.5f), t * 0.8f, c);
        for (i = 0; i < 3; ++i)
            nk_stroke_line(out, x + P(5.5f), y + P(6 + i * 2.5f), x + P(10.5f), y + P(6 + i * 2.5f), t * 0.7f, c);
        break;
    case WENA_ICON_LOCK:
        /* fa-lock: the shackle over a filled body. */
        nk_stroke_arc(out, x + P(8), y + P(6.5f), P(3.5f), 3.14159265f, 6.2831853f, t * 1.2f, c);
        line(out, x + P(4.5f), y + P(6.5f), x + P(4.5f), y + P(8), t * 1.2f, c);
        line(out, x + P(11.5f), y + P(6.5f), x + P(11.5f), y + P(8), t * 1.2f, c);
        nk_fill_rect(out, nk_rect(x + P(2.5f), y + P(8), P(11), P(7)), P(1), c);
        break;
    case WENA_ICON_EYE:
        /* fa-eye: the lid's outline and the pupil. */
        nk_stroke_arc(out, x + P(8), y + P(14), P(8.5f), 3.14159265f * 1.22f, 3.14159265f * 1.78f, t, c);
        nk_stroke_arc(out, x + P(8), y + P(2), P(8.5f), 3.14159265f * 0.22f, 3.14159265f * 0.78f, t, c);
        nk_fill_circle(out, nk_rect(x + P(5.5f), y + P(5.5f), P(5), P(5)), c);
        break;
    case WENA_ICON_BELL:
    case WENA_ICON_BELL_SLASH:
        /* fa-bell: the dome, its rim and the clapper; slashed when muted. */
        nk_fill_arc(out, x + P(8), y + P(7), P(5), 3.14159265f, 6.2831853f, c);
        nk_fill_rect(out, nk_rect(x + P(3), y + P(7), P(10), P(5)), 0.0f, c);
        nk_fill_rect(out, nk_rect(x + P(1.5f), y + P(11.5f), P(13), P(1.5f)), 0.0f, c);
        nk_fill_circle(out, nk_rect(x + P(6.5f), y + P(13), P(3), P(3)), c);
        if (icon == WENA_ICON_BELL_SLASH) line(out, x + P(1), y + P(1.5f), x + P(15), y + P(15), t * 1.2f, c);
        break;
    case WENA_ICON_SORT:
        /* fa-sort: a triangle up over a triangle down. */
        nk_fill_triangle(out, x + P(8), y + P(1), x + P(3), y + P(7), x + P(13), y + P(7), c);
        nk_fill_triangle(out, x + P(3), y + P(9), x + P(13), y + P(9), x + P(8), y + P(15), c);
        break;
    case WENA_ICON_CALENDAR:
        nk_stroke_rect(out, nk_rect(x + P(1.5f), y + P(3), P(13), P(12)), P(1), t * 0.8f, c);
        nk_fill_rect(out, nk_rect(x + P(1.5f), y + P(3), P(13), P(3.5f)), P(1), c);
        line(out, x + P(5), y + P(1), x + P(5), y + P(4), t, c);
        line(out, x + P(11), y + P(1), x + P(11), y + P(4), t, c);
        break;
    case WENA_ICON_SORT_ALPHA:
        /* fa-sort-alpha-asc: an arrow down beside the letters' lines. */
        line(out, x + P(3.5f), y + P(1.5f), x + P(3.5f), y + P(14), t, c);
        nk_fill_triangle(out, x + P(1), y + P(11), x + P(6), y + P(11), x + P(3.5f), y + P(15), c);
        line(out, x + P(8), y + P(4), x + P(14), y + P(4), t, c);
        line(out, x + P(8), y + P(8.5f), x + P(13), y + P(8.5f), t, c);
        line(out, x + P(8), y + P(13), x + P(11.5f), y + P(13), t, c);
        break;
    case WENA_ICON_THUMBS_UP:
        /* fa-thumbs-o-up: the cuff and the hand with its raised thumb. */
        nk_fill_rect(out, nk_rect(x + P(1), y + P(7), P(3.5f), P(8)), 0.0f, c);
        nk_stroke_rect(out, nk_rect(x + P(6), y + P(6.5f), P(8.5f), P(8.5f)), P(2), t * 0.8f, c);
        line(out, x + P(6.5f), y + P(7), x + P(9.5f), y + P(1.5f), t, c);
        break;
    case WENA_ICON_TABLE:
        nk_stroke_rect(out, nk_rect(x + P(1), y + P(2), P(14), P(12)), P(1), t * 0.8f, c);
        nk_fill_rect(out, nk_rect(x + P(1), y + P(2), P(14), P(3)), P(1), c);
        line(out, x + P(1), y + P(9), x + P(15), y + P(9), t * 0.7f, c);
        line(out, x + P(6), y + P(5), x + P(6), y + P(14), t * 0.7f, c);
        break;
    case WENA_ICON_CLOCK:
        nk_stroke_circle(out, nk_rect(x + P(1.5f), y + P(1.5f), P(13), P(13)), t, c);
        line(out, x + P(8), y + P(4), x + P(8), y + P(8), t, c);
        line(out, x + P(8), y + P(8), x + P(11), y + P(10), t, c);
        break;
    case WENA_ICON_PIE_CHART:
        nk_fill_arc(out, x + P(8), y + P(8), P(6.5f), 0.0f, 4.712389f, c);
        nk_fill_arc(out, x + P(9), y + P(7), P(6), 4.712389f, 6.2831853f, c);
        break;
    case WENA_ICON_BAR_CHART:
        line(out, x + P(1.5f), y + P(14.5f), x + P(15), y + P(14.5f), t, c);
        nk_fill_rect(out, nk_rect(x + P(3), y + P(8), P(2.5f), P(6)), 0.0f, c);
        nk_fill_rect(out, nk_rect(x + P(7), y + P(4), P(2.5f), P(10)), 0.0f, c);
        nk_fill_rect(out, nk_rect(x + P(11), y + P(6), P(2.5f), P(8)), 0.0f, c);
        break;
    case WENA_ICON_TASKS:
        for (i = 0; i < 3; ++i) {
            nk_stroke_rect(out, nk_rect(x + P(1.5f), y + P(2 + i * 4.5f), P(3), P(3)), 0.0f, t * 0.7f, c);
            line(out, x + P(6.5f), y + P(3.5f + i * 4.5f), x + P(14.5f), y + P(3.5f + i * 4.5f), t, c);
        }
        break;
    case WENA_ICON_LINE_CHART:
        line(out, x + P(1.5f), y + P(14.5f), x + P(15), y + P(14.5f), t, c);
        points[0] = x + P(2); points[1] = y + P(11);
        points[2] = x + P(6); points[3] = y + P(6);
        points[4] = x + P(9.5f); points[5] = y + P(9);
        points[6] = x + P(14); points[7] = y + P(3);
        nk_stroke_polyline(out, points, 4, t, c);
        break;
    case WENA_ICON_AREA_CHART:
        line(out, x + P(1.5f), y + P(14.5f), x + P(15), y + P(14.5f), t, c);
        nk_fill_triangle(out, x + P(2), y + P(14), x + P(7), y + P(5), x + P(10), y + P(14), c);
        nk_fill_triangle(out, x + P(7), y + P(14), x + P(11), y + P(8), x + P(14.5f), y + P(14), c);
        break;
    case WENA_ICON_ROAD:
        line(out, x + P(4), y + P(15), x + P(6.5f), y + P(1), t, c);
        line(out, x + P(12), y + P(15), x + P(9.5f), y + P(1), t, c);
        line(out, x + P(8), y + P(3), x + P(8), y + P(5.5f), t, c);
        line(out, x + P(8), y + P(8), x + P(8), y + P(10.5f), t, c);
        line(out, x + P(8), y + P(13), x + P(8), y + P(15), t, c);
        break;
    case WENA_ICON_TACHOMETER:
        nk_stroke_arc(out, x + P(8), y + P(11), P(6.5f), 3.14159265f, 6.2831853f, t, c);
        line(out, x + P(8), y + P(11), x + P(11.5f), y + P(6), t, c);
        nk_fill_circle(out, nk_rect(x + P(6.5f), y + P(9.5f), P(3), P(3)), c);
        break;
    case WENA_ICON_HEARTBEAT:
        points[0] = x + P(1); points[1] = y + P(9);
        points[2] = x + P(5); points[3] = y + P(9);
        points[4] = x + P(7); points[5] = y + P(3);
        points[6] = x + P(9.5f); points[7] = y + P(14);
        points[8] = x + P(11.5f); points[9] = y + P(9);
        points[10] = x + P(15); points[11] = y + P(9);
        nk_stroke_polyline(out, points, 6, t, c);
        break;
    case WENA_ICON_MAP_MARKER:
        nk_fill_circle(out, nk_rect(x + P(3.5f), y + P(1), P(9), P(9)), c);
        nk_fill_triangle(out, x + P(4.2f), y + P(7.5f), x + P(11.8f), y + P(7.5f), x + P(8), y + P(15), c);
        nk_fill_circle(out, nk_rect(x + P(6.5f), y + P(3.5f), P(3), P(3)), color_of(WENA_WEKAN_PANEL));
        break;
    case WENA_ICON_ANGLES_RIGHT:
        /* fa-angle-double-right: two chevrons. */
        line(out, x + P(3), y + P(3), x + P(8), y + P(8), t * 1.2f, c);
        line(out, x + P(8), y + P(8), x + P(3), y + P(13), t * 1.2f, c);
        line(out, x + P(8), y + P(3), x + P(13), y + P(8), t * 1.2f, c);
        line(out, x + P(13), y + P(8), x + P(8), y + P(13), t * 1.2f, c);
        break;
    case WENA_ICON_DESKTOP:
        /* fa-desktop: the screen on its stand. */
        nk_stroke_rect(out, nk_rect(x + P(1), y + P(2), P(14), P(9)), P(1), t * 1.1f, c);
        nk_fill_rect(out, nk_rect(x + P(7), y + P(11), P(2), P(2.5f)), 0.0f, c);
        nk_fill_rect(out, nk_rect(x + P(4.5f), y + P(13.5f), P(7), P(1.5f)), 0.0f, c);
        break;
    case WENA_ICON_ARROWS:
        /* fa-arrows: the four ways a card moves. */
        line(out, x + P(8), y + P(1.5f), x + P(8), y + P(14.5f), t, c);
        line(out, x + P(1.5f), y + P(8), x + P(14.5f), y + P(8), t, c);
        nk_fill_triangle(out, x + P(8), y + P(0.5f), x + P(5.5f), y + P(3.5f), x + P(10.5f), y + P(3.5f), c);
        nk_fill_triangle(out, x + P(8), y + P(15.5f), x + P(5.5f), y + P(12.5f), x + P(10.5f), y + P(12.5f), c);
        nk_fill_triangle(out, x + P(0.5f), y + P(8), x + P(3.5f), y + P(5.5f), x + P(3.5f), y + P(10.5f), c);
        nk_fill_triangle(out, x + P(15.5f), y + P(8), x + P(12.5f), y + P(5.5f), x + P(12.5f), y + P(10.5f), c);
        break;
    case WENA_ICON_MOBILE:
        /* fa-mobile: the phone, its screen and its button. */
        nk_stroke_rect(out, nk_rect(x + P(4), y + P(0.5f), P(8), P(15)), P(1.5f), t * 1.1f, c);
        nk_fill_rect(out, nk_rect(x + P(5.5f), y + P(2.5f), P(5), P(9.5f)), 0.0f, c);
        nk_fill_circle(out, nk_rect(x + P(7.2f), y + P(12.8f), P(1.6f), P(1.6f)), c);
        break;
    case WENA_ICON_NONE:
    case WENA_ICON_COUNT:
        break;
    }
}
#undef P

/* Widgets ------------------------------------------------------------------- */

void wena_wekan_space_area(struct nk_context *context, float height, float *x, float *y, float *w)
{
    struct nk_vec2 origin;
    struct nk_rect clip;
    (void)height;
    origin = nk_layout_space_to_screen(context, nk_vec2(0.0f, 0.0f));
    clip = nk_layout_space_bounds(context);
    *x = origin.x; *y = origin.y;
    *w = clip.w - (origin.x - clip.x);
}

void wena_wekan_fill(struct nk_context *context, float x, float y, float w, float h,
                     int color, float rounding)
{
    struct nk_command_buffer *out;
    if (context == NULL || w <= 0.0f || h <= 0.0f || (out = nk_window_get_canvas(context)) == NULL) return;
    nk_fill_rect(out, nk_rect(x, y, w, h), rounding, color_of(color));
}

/* A button with no background of its own: the click behavior of Nuklear's
 * button over a region the caller draws. */
static int invisible_button(struct nk_context *context)
{
    struct nk_style_button style;
    style = context->style.button;
    style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    style.hover = style.normal;
    style.active = style.normal;
    style.border = 0.0f;
    style.rounding = 0.0f;
    style.padding = nk_vec2(0.0f, 0.0f);
    return nk_button_text_styled(context, &style, "", 0);
}

/* The tooltip an icon asked for this frame, and where the mouse was. Drawn by
 * wena_wekan_tooltip_flush at window level: nk_tooltip opens a popup, and one
 * opened inside a layout-space row or a group corrupted the window's command
 * list, so nothing after the hovered icon was drawn. */
static char pending_tooltip[WENA_UI_CONTROL_NAME];
static float pending_x, pending_y;

void wena_wekan_tooltip_flush(struct nk_context *context)
{
    if (pending_tooltip[0] == '\0') return;
    if (context != NULL && context->current != NULL &&
        context->input.mouse.pos.x == pending_x && context->input.mouse.pos.y == pending_y &&
        !nk_input_is_mouse_down(&context->input, NK_BUTTON_LEFT)) {
        /* Nuklear sizes a tooltip by window.padding but lays it out with
         * popup_padding; the board's zero padding left no room for the text. */
        nk_style_push_vec2(context, &context->style.window.padding, context->style.window.popup_padding);
        nk_tooltip(context, pending_tooltip);
        nk_style_pop_vec2(context);
    }
    pending_tooltip[0] = '\0';
}

int wena_wekan_icon_button(struct nk_context *context, WenaIcon icon,
                           const char *name, float size, int color)
{
    struct nk_rect bounds;
    int hovered, clicked;
    if (context == NULL || context->current == NULL) return 0;
    bounds = nk_widget_bounds(context);
    hovered = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    /* Named before it is drawn: what tests and the parity check click by. */
    wena_ui_control_record(NULL, name, bounds.x, bounds.y, bounds.w, bounds.h);
    clicked = invisible_button(context);
    if (size <= 0.0f || size > bounds.h) size = bounds.h < bounds.w ? bounds.h : bounds.w;
    wena_wekan_icon_draw(context, icon, bounds.x + (bounds.w - size) / 2.0f,
                         bounds.y + (bounds.h - size) / 2.0f, size,
                         hovered && color == WENA_WEKAN_ICON ? WENA_WEKAN_ICON_ACTIVE : color);
    if (hovered && name != NULL && name[0] != '\0') {
        size_t length = strlen(name);
        if (length >= sizeof(pending_tooltip)) length = sizeof(pending_tooltip) - 1;
        memcpy(pending_tooltip, name, length);
        pending_tooltip[length] = '\0';
        pending_x = context->input.mouse.pos.x;
        pending_y = context->input.mouse.pos.y;
    }
    return clicked;
}

int wena_wekan_link(struct nk_context *context, WenaIcon icon, const char *text,
                    WenaWekanFont font, int color)
{
    struct nk_rect bounds;
    const struct nk_user_font *face;
    struct nk_command_buffer *out;
    float icon_size, x, width;
    int clicked, hovered;
    if (context == NULL || context->current == NULL || text == NULL) return 0;
    bounds = nk_widget_bounds(context);
    hovered = nk_input_is_mouse_hovering_rect(&context->input, bounds);
    wena_ui_control_record(NULL, text, bounds.x, bounds.y, bounds.w, bounds.h);
    clicked = invisible_button(context);
    face = wena_wekan_font(context, font);
    out = nk_window_get_canvas(context);
    icon_size = face != NULL ? face->height : 13.0f;
    x = bounds.x;
    if (icon != WENA_ICON_NONE) {
        wena_wekan_icon_draw(context, icon, x, bounds.y + (bounds.h - icon_size) / 2.0f, icon_size, color);
        x += icon_size + 4.0f;
    }
    if (face != NULL && out != NULL) {
        width = face->width(face->userdata, face->height, text, (int)strlen(text));
        nk_draw_text(out, nk_rect(x, bounds.y + (bounds.h - face->height) / 2.0f, width + 1.0f, face->height),
                     text, (int)strlen(text), face, nk_rgba(0, 0, 0, 0), color_of(color));
        if (hovered)
            nk_stroke_line(out, x, bounds.y + (bounds.h + face->height) / 2.0f + 1.0f,
                           x + width, bounds.y + (bounds.h + face->height) / 2.0f + 1.0f, 1.0f, color_of(color));
    }
    return clicked;
}

void wena_wekan_text(struct nk_context *context, const char *text,
                     WenaWekanFont font, int color, int align)
{
    struct nk_rect bounds;
    const struct nk_user_font *face;
    if (context == NULL || context->current == NULL || text == NULL) return;
    bounds = nk_widget_bounds(context);
    /* Without a baked face the context's own font draws it: the text stays. */
    face = wena_wekan_font(context, font);
    if (face != NULL) nk_style_push_font(context, face);
    nk_label_colored(context, text, (nk_flags)align, color_of(color));
    if (face != NULL) nk_style_pop_font(context);
    wena_ui_control_record(NULL, text, bounds.x, bounds.y, bounds.w, bounds.h);
}

void wena_wekan_initials(const char *name, char *out, size_t capacity)
{
    size_t used = 0, length;
    int start = 1;
    if (out == NULL || capacity == 0) return;
    out[0] = '\0';
    if (name == NULL) return;
    for (; *name != '\0'; ++name) {
        if (*name == ' ' || *name == '\t' || *name == '\n' || *name == '\r') { start = 1; continue; }
        if (!start) continue;
        start = 0;
        /* The word's first letter, all of its UTF-8 bytes. */
        length = 1;
        while (((unsigned char)name[length] & 0xC0u) == 0x80u) ++length;
        if (used + length >= capacity) break;
        memcpy(out + used, name, length);
        if (length == 1 && out[used] >= 'a' && out[used] <= 'z') out[used] = (char)(out[used] - 'a' + 'A');
        /* Latin-1 letters too, as toUpperCase: a with ring to A with ring ... */
        else if (length == 2 && (unsigned char)out[used] == 0xC3u && (unsigned char)out[used + 1] >= 0xA0u &&
                 (unsigned char)out[used + 1] <= 0xBEu && (unsigned char)out[used + 1] != 0xB7u)
            out[used + 1] = (char)((unsigned char)out[used + 1] - 0x20u);
        used += length;
        name += length - 1;
    }
    out[used] = '\0';
}

void wena_wekan_avatar(struct nk_context *context, const char *name)
{
    struct nk_rect bounds;
    char initials[32];
    if (context == NULL || context->current == NULL || name == NULL) return;
    bounds = nk_widget_bounds(context);
    wena_wekan_initials(name, initials, sizeof(initials));
    wena_wekan_fill(context, bounds.x, bounds.y, bounds.w, bounds.h, 0x1DBDBDB, bounds.w * 0.5f);
    wena_wekan_text(context, initials, WENA_WEKAN_FONT_SMALL, 0x1444444, NK_TEXT_CENTERED);
}

void wena_wekan_text_wrap(struct nk_context *context, const char *text,
                          WenaWekanFont font, int color)
{
    struct nk_rect bounds;
    const struct nk_user_font *face;
    if (context == NULL || context->current == NULL || text == NULL) return;
    bounds = nk_widget_bounds(context);
    face = wena_wekan_font(context, font);
    if (face != NULL) nk_style_push_font(context, face);
    nk_label_colored_wrap(context, text, color_of(color));
    if (face != NULL) nk_style_pop_font(context);
    wena_ui_control_record(NULL, text, bounds.x, bounds.y, bounds.w, bounds.h);
}

/* The length of the next line of `text` that fits `width`: whole words, or a
 * word cut where a single one is wider than the line. */
static int wrap_line(const struct nk_user_font *face, const char *text, float width)
{
    int length, best, end;
    best = 0;
    for (end = 0; text[end] != '\0' && text[end] != '\n'; ++end) {
        if (text[end + 1] == ' ' || text[end + 1] == '\0' || text[end + 1] == '\n') {
            if (face->width(face->userdata, face->height, text, end + 1) > width && best > 0) break;
            best = end + 1;
        }
    }
    if (best == 0) {
        /* One word wider than the line: as much of it as fits. */
        for (length = 1; text[length] != '\0' && text[length] != '\n' &&
             face->width(face->userdata, face->height, text, length + 1) <= width; ++length) {}
        best = text[0] == '\0' ? 0 : length;
    }
    return best;
}

int wena_wekan_wrapped_lines(struct nk_context *context, const char *text,
                             WenaWekanFont font, float width)
{
    const struct nk_user_font *face;
    int lines, length;
    if (context == NULL || text == NULL) return 0;
    face = wena_wekan_font(context, font);
    if (face == NULL || width <= 0.0f) return 1;
    for (lines = 0; *text != '\0'; ++lines) {
        length = wrap_line(face, text, width);
        text += length;
        while (*text == ' ' || *text == '\n') ++text;
    }
    return lines > 0 ? lines : 1;
}

int wena_wekan_text_button(struct nk_context *context, const char *text,
                           WenaWekanFont font, int color)
{
    struct nk_rect bounds;
    const struct nk_user_font *face;
    struct nk_command_buffer *out;
    float y;
    int clicked, length;
    if (context == NULL || context->current == NULL || text == NULL) return 0;
    bounds = nk_widget_bounds(context);
    wena_ui_control_record(NULL, text, bounds.x, bounds.y, bounds.w, bounds.h);
    clicked = invisible_button(context);
    face = wena_wekan_font(context, font);
    out = nk_window_get_canvas(context);
    if (face == NULL || out == NULL) return clicked;
    for (y = bounds.y; *text != '\0' && y + face->height <= bounds.y + bounds.h + 0.5f;
         y += face->height + 4.0f) {
        length = wrap_line(face, text, bounds.w);
        nk_draw_text(out, nk_rect(bounds.x, y, bounds.w, face->height), text, length, face,
                     nk_rgba(0, 0, 0, 0), color_of(color));
        text += length;
        while (*text == ' ' || *text == '\n') ++text;
    }
    return clicked;
}

int wena_wekan_section_header(struct nk_context *context, int open, WenaIcon icon,
                              const char *label)
{
    struct nk_rect bounds;
    const struct nk_user_font *face;
    struct nk_command_buffer *out;
    float size, x;
    int clicked;
    if (context == NULL || context->current == NULL || label == NULL) return 0;
    bounds = nk_widget_bounds(context);
    wena_ui_control_record(NULL, label, bounds.x, bounds.y, bounds.w, bounds.h);
    clicked = invisible_button(context);
    face = wena_wekan_font(context, WENA_WEKAN_FONT_SECTION);
    out = nk_window_get_canvas(context);
    size = face != NULL ? face->height : 16.0f;
    x = bounds.x;
    wena_wekan_icon_draw(context, open ? WENA_ICON_CARET_DOWN : WENA_ICON_CARET_RIGHT,
                         x, bounds.y + (bounds.h - size) / 2.0f, size, WENA_WEKAN_SECTION_TITLE);
    x += size + 2.0f;
    if (icon != WENA_ICON_NONE) {
        wena_wekan_icon_draw(context, icon, x, bounds.y + (bounds.h - size) / 2.0f, size,
                             WENA_WEKAN_SECTION_TITLE);
        x += size + 6.0f;
    }
    if (face != NULL && out != NULL)
        nk_draw_text(out, nk_rect(x, bounds.y + (bounds.h - face->height) / 2.0f,
                                  bounds.x + bounds.w - x, face->height),
                     label, (int)strlen(label), face, nk_rgba(0, 0, 0, 0),
                     color_of(WENA_WEKAN_SECTION_TITLE));
    return clicked;
}

int wena_wekan_checkbox(struct nk_context *context, int checked, const char *text,
                        int interactive)
{
    struct nk_rect bounds;
    const struct nk_user_font *face;
    struct nk_command_buffer *out;
    float size;
    int clicked;
    if (context == NULL || context->current == NULL || text == NULL) return 0;
    bounds = nk_widget_bounds(context);
    wena_ui_control_record(checked ? "checklist-checked" : "checklist-item", text,
                           bounds.x, bounds.y, bounds.w, bounds.h);
    if (interactive) clicked = invisible_button(context);
    else { clicked = 0; nk_spacer(context); /* one slot: nk_spacing wraps to a new row */ }
    face = wena_wekan_font(context, WENA_WEKAN_FONT_BODY);
    out = nk_window_get_canvas(context);
    size = face != NULL ? face->height + 2.0f : 16.0f;
    wena_wekan_icon_draw(context, checked ? WENA_ICON_CHECK_SQUARE : WENA_ICON_SQUARE, bounds.x,
                         bounds.y + (bounds.h - size) / 2.0f, size,
                         checked ? WENA_WEKAN_BUTTON : WENA_WEKAN_ICON);
    if (face != NULL && out != NULL)
        nk_draw_text(out, nk_rect(bounds.x + size + 6.0f, bounds.y + (bounds.h - face->height) / 2.0f,
                                  bounds.w - size - 6.0f, face->height),
                     text, (int)strlen(text), face, nk_rgba(0, 0, 0, 0),
                     color_of(checked ? WENA_WEKAN_ICON_ACTIVE : WENA_WEKAN_TEXT));
    return clicked;
}

int wena_wekan_area(struct nk_context *context, const char *name)
{
    struct nk_rect bounds;
    if (context == NULL || context->current == NULL) return 0;
    bounds = nk_widget_bounds(context);
    if (name != NULL) wena_ui_control_record(NULL, name, bounds.x, bounds.y, bounds.w, bounds.h);
    return invisible_button(context);
}

void wena_wekan_draw_wrapped(struct nk_context *context, const char *text, WenaWekanFont font, int color,
                             float x, float y, float w, float h)
{
    const struct nk_user_font *face;
    struct nk_command_buffer *out;
    int length;
    float line_y;
    if (context == NULL || text == NULL || (face = wena_wekan_font(context, font)) == NULL ||
        (out = nk_window_get_canvas(context)) == NULL) return;
    for (line_y = y; *text != '\0' && line_y + face->height <= y + h + 0.5f; line_y += face->height + 4.0f) {
        length = wrap_line(face, text, w);
        nk_draw_text(out, nk_rect(x, line_y, w, face->height), text, length, face, nk_rgba(0, 0, 0, 0), color_of(color));
        text += length;
        while (*text == ' ' || *text == '\n') ++text;
    }
}

int wena_wekan_button(struct nk_context *context, const char *text, int color)
{
    struct nk_style_button style;
    struct nk_rect bounds;
    int clicked;
    if (context == NULL || context->current == NULL || text == NULL) return 0;
    bounds = nk_widget_bounds(context);
    style = context->style.button;
    style.normal = nk_style_item_color(color_of(color));
    style.hover = style.normal;
    style.active = style.normal;
    style.border = 0.0f;
    style.rounding = 4.0f;
    style.text_normal = color_of(WENA_WEKAN_BUTTON_TEXT);
    style.text_hover = style.text_normal;
    style.text_active = style.text_normal;
    wena_ui_control_record(NULL, text, bounds.x, bounds.y, bounds.w, bounds.h);
    nk_style_push_font(context, wena_wekan_font(context, WENA_WEKAN_FONT_BOLD));
    clicked = nk_button_label_styled(context, &style, text);
    nk_style_pop_font(context);
    return clicked;
}

/* Menus --------------------------------------------------------------------- */

#define MENU_HEADER 42.0f
#define MENU_ROW 36.0f
#define MENU_SEPARATOR 11.0f

int wena_wekan_menu(struct nk_context *context, const char *title,
                    float x, float y, float width, int columns,
                    const WenaWekanMenuItem *items, size_t count)
{
    struct nk_rect area, cell;
    struct nk_command_buffer *out;
    const struct nk_user_font *bold, *heading;
    size_t per_column, column, row;
    float column_width, column_height, height, tallest;
    int chosen = WENA_WEKAN_MENU_NONE, hovered;
    char id[96];
    if (context == NULL || title == NULL || (items == NULL && count > 0) || columns < 1) return WENA_WEKAN_MENU_NONE;
    per_column = (count + (size_t)columns - 1) / (size_t)columns;
    if (per_column == 0) per_column = 1;
    tallest = 0.0f;
    for (column = 0; column < (size_t)columns; ++column) {
        column_height = 0.0f;
        for (row = 0; row < per_column && column * per_column + row < count; ++row)
            column_height += MENU_ROW + (items[column * per_column + row].separator_before && row ? MENU_SEPARATOR : 0.0f);
        if (column_height > tallest) tallest = column_height;
    }
    height = MENU_HEADER + 18.0f + tallest + 18.0f;
    area = nk_rect(x, y, width, height);
    (void)sprintf(id, "wekan-menu:%.80s", title);
    bold = wena_wekan_font(context, WENA_WEKAN_FONT_BOLD);
    heading = bold;
    nk_style_push_style_item(context, &context->style.window.fixed_background,
                             nk_style_item_color(color_of(WENA_WEKAN_POPUP)));
    nk_style_push_color(context, &context->style.window.border_color, color_of(WENA_WEKAN_POPUP_BORDER));
    nk_style_push_vec2(context, &context->style.window.padding, nk_vec2(0.0f, 0.0f));
    if (nk_begin(context, id, area, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
        wena_ui_region("popup");
        out = nk_window_get_canvas(context);
        nk_fill_rect(out, nk_rect(x + 1.0f, y + 1.0f, width - 2.0f, MENU_HEADER), 0.0f,
                     color_of(WENA_WEKAN_POPUP_HEADER));
        nk_stroke_line(out, x + 1.0f, y + MENU_HEADER, x + width - 1.0f, y + MENU_HEADER, 1.0f,
                       color_of(WENA_WEKAN_POPUP_BORDER));
        if (heading != NULL)
            nk_draw_text(out, nk_rect(x + 18.0f, y + (MENU_HEADER - heading->height) / 2.0f, width - 60.0f, heading->height),
                         title, (int)strlen(title), heading, nk_rgba(0, 0, 0, 0), color_of(WENA_WEKAN_POPUP_HEADER_TEXT));
        nk_layout_space_begin(context, NK_STATIC, height, (int)count + 1);
        nk_layout_space_push(context, nk_rect(width - 36.0f, 10.0f, 22.0f, 22.0f));
        if (wena_wekan_icon_button(context, WENA_ICON_TIMES, "Close", 12.0f, WENA_WEKAN_ICON))
            chosen = WENA_WEKAN_MENU_CLOSED;
        column_width = (width - 36.0f - (float)(columns - 1) * 24.0f) / (float)columns;
        for (column = 0; column < (size_t)columns; ++column) {
            float top = MENU_HEADER + 18.0f;
            for (row = 0; row < per_column && column * per_column + row < count; ++row) {
                const WenaWekanMenuItem *item = &items[column * per_column + row];
                float left = 18.0f + (float)column * (column_width + 24.0f);
                if (item->separator_before && row) {
                    nk_stroke_line(out, x + left, y + top + MENU_SEPARATOR / 2.0f,
                                   x + left + column_width, y + top + MENU_SEPARATOR / 2.0f, 1.0f,
                                   color_of(WENA_WEKAN_POPUP_BORDER));
                    top += MENU_SEPARATOR;
                }
                cell = nk_rect(x + left, y + top, column_width, MENU_ROW);
                hovered = item->enabled && nk_input_is_mouse_hovering_rect(&context->input, cell);
                if (hovered) nk_fill_rect(out, cell, 0.0f, color_of(WENA_WEKAN_POPUP_HOVER));
                nk_layout_space_push(context, nk_rect(left, top, column_width, MENU_ROW));
                wena_ui_control_record("popup", item->text, cell.x, cell.y, cell.w, cell.h);
                if (invisible_button(context) && item->enabled) chosen = (int)(column * per_column + row);
                if (item->icon != WENA_ICON_NONE)
                    wena_wekan_icon_draw(context, item->icon, cell.x + 10.0f, cell.y + 11.0f, 14.0f,
                                         hovered ? WENA_WEKAN_BUTTON_TEXT : WENA_WEKAN_ICON);
                if (item->checked)
                    wena_wekan_icon_draw(context, WENA_ICON_CHECK, cell.x + cell.w - 24.0f, cell.y + 11.0f, 14.0f,
                                         hovered ? WENA_WEKAN_BUTTON_TEXT : WENA_WEKAN_ICON);
                if (bold != NULL && item->text != NULL)
                    nk_draw_text(out, nk_rect(cell.x + 30.0f, cell.y + (MENU_ROW - bold->height) / 2.0f,
                                              column_width - 34.0f, bold->height),
                                 item->text, (int)strlen(item->text), bold, nk_rgba(0, 0, 0, 0),
                                 hovered ? color_of(WENA_WEKAN_BUTTON_TEXT) :
                                 item->enabled ? color_of(WENA_WEKAN_TEXT) : color_of(WENA_WEKAN_ICON));
                top += MENU_ROW;
            }
        }
        nk_layout_space_end(context);
        /* A click anywhere else closes it, as in WeKan. */
        if (chosen == WENA_WEKAN_MENU_NONE &&
            nk_input_is_mouse_pressed(&context->input, NK_BUTTON_LEFT) &&
            !nk_input_is_mouse_hovering_rect(&context->input, area))
            chosen = WENA_WEKAN_MENU_CLOSED;
    }
    nk_end(context);
    nk_style_pop_vec2(context);
    nk_style_pop_color(context);
    nk_style_pop_style_item(context);
    if (chosen == WENA_WEKAN_MENU_NONE) nk_window_set_focus(context, id);
    return chosen;
}
