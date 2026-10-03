#ifndef WENA_WEKAN_LOOK_H
#define WENA_WEKAN_LOOK_H

/* WeKan's board look in Nuklear: its colors, fonts, icons and controls, as
 * WeKan itself draws them. The values are measured from a running WeKan by
 * tools/wekan-ui/capture.e2e.js (pinned in tests/fixtures/wekan-ui and checked
 * by tests/test_wekan_ui_parity.py), not chosen by eye. */

#include <stddef.h>

struct nk_context;
struct nk_user_font;

typedef enum WenaWekanColor {
    WENA_WEKAN_BODY,             /* the board canvas, between lists */
    WENA_WEKAN_HEADER,           /* the blue header bar */
    WENA_WEKAN_HEADER_TEXT,      /* header titles and the active icon */
    WENA_WEKAN_HEADER_LINK,      /* header links: Filter, Multi-Selection ... */
    WENA_WEKAN_SWIMLANE_HEADER,
    WENA_WEKAN_LIST,
    WENA_WEKAN_LIST_HEADER,
    WENA_WEKAN_LIST_BORDER,
    WENA_WEKAN_MINICARD,
    WENA_WEKAN_MINICARD_OPEN,    /* the minicard whose details are open */
    WENA_WEKAN_MINICARD_TEXT,
    WENA_WEKAN_MINICARD_SHADOW,
    WENA_WEKAN_TEXT,             /* titles: lists, swimlanes, menu items */
    WENA_WEKAN_ICON,             /* gray Font Awesome icons */
    WENA_WEKAN_ICON_ACTIVE,
    WENA_WEKAN_ADD_CARD,         /* "+ Add Card" under a list */
    WENA_WEKAN_POPUP,
    WENA_WEKAN_POPUP_BORDER,
    WENA_WEKAN_POPUP_HEADER,
    WENA_WEKAN_POPUP_HEADER_TEXT,
    WENA_WEKAN_POPUP_HOVER,      /* the highlighted menu item */
    WENA_WEKAN_PANEL,            /* card details and sidebar */
    WENA_WEKAN_SECTION_TITLE,    /* "Labels", "Members" ... in card details */
    WENA_WEKAN_BUTTON,           /* primary buttons */
    WENA_WEKAN_BUTTON_ADD,       /* the composer's "Add" */
    WENA_WEKAN_BUTTON_TEXT,
    WENA_WEKAN_INPUT_BORDER,
    WENA_WEKAN_WIP_EXCEEDED,     /* a list's count over its WIP limit */
    WENA_WEKAN_COLOR_COUNT
} WenaWekanColor;

/* "#rrggbb" of a color: the pinned value tests compare with WeKan's. */
const char *wena_wekan_color_hex(WenaWekanColor color);

typedef enum WenaWekanFont {
    WENA_WEKAN_FONT_BODY,        /* 14px Roboto */
    WENA_WEKAN_FONT_BOLD,        /* 14px Roboto Bold: titles, menu items */
    WENA_WEKAN_FONT_SMALL,       /* 12px: minicards, the user's name */
    WENA_WEKAN_FONT_LINK,        /* 13px: header links, "+ Add Card" */
    WENA_WEKAN_FONT_SECTION,     /* 16px bold: card details sections */
    WENA_WEKAN_FONT_TITLE,       /* 19px bold: the card details title */
    WENA_WEKAN_FONT_COUNT
} WenaWekanFont;

/* The desktop bakes these once; unset ones fall back to the context's font. */
void wena_wekan_font_set(WenaWekanFont which, const struct nk_user_font *font);
const struct nk_user_font *wena_wekan_font(struct nk_context *context, WenaWekanFont which);

/* WeKan's Font Awesome icons, drawn as vectors. */
typedef enum WenaIcon {
    WENA_ICON_NONE,
    WENA_ICON_HOME, WENA_ICON_ANGLES_LEFT, WENA_ICON_CARET_DOWN, WENA_ICON_CARET_RIGHT,
    WENA_ICON_BARS, WENA_ICON_PLUS, WENA_ICON_PLUS_SQUARE, WENA_ICON_TIMES,
    WENA_ICON_FILTER, WENA_ICON_SEARCH, WENA_ICON_CHECK_SQUARE, WENA_ICON_SQUARE,
    WENA_ICON_GRID, WENA_ICON_GEAR, WENA_ICON_TAG, WENA_ICON_USERS, WENA_ICON_USER,
    WENA_ICON_ARCHIVE, WENA_ICON_ARROW_RIGHT, WENA_ICON_ARROW_UP, WENA_ICON_ARROW_DOWN,
    WENA_ICON_BRUSH, WENA_ICON_LIST, WENA_ICON_ALIGN_LEFT, WENA_ICON_CHECK,
    WENA_ICON_BAN, WENA_ICON_GLOBE, WENA_ICON_PENCIL, WENA_ICON_REFRESH, WENA_ICON_HISTORY,
    WENA_ICON_WINDOW_MAXIMIZE, WENA_ICON_WINDOW_MINIMIZE,
    WENA_ICON_STAR, WENA_ICON_STAR_O, WENA_ICON_FOLDER, WENA_ICON_CLIPBOARD,
    WENA_ICON_FILE_TEXT_O, WENA_ICON_LOCK, WENA_ICON_EYE, WENA_ICON_BELL, WENA_ICON_BELL_SLASH,
    WENA_ICON_SORT, WENA_ICON_CALENDAR, WENA_ICON_SORT_ALPHA,
    WENA_ICON_COUNT
} WenaIcon;

void wena_wekan_icon_draw(struct nk_context *context, WenaIcon icon,
                          float x, float y, float size, int active_rgb);

/* Every control drawn this frame, by the name WeKan gives it (its title or
 * text: "Collapse", "Swimlane Actions", "Add Card"), and where: what tests
 * click and what `wena --ui-inventory` compares with WeKan's capture. */
#define WENA_UI_CONTROL_NAME 64
#define WENA_UI_CONTROL_CAPACITY 768
typedef struct WenaUiControl {
    char name[WENA_UI_CONTROL_NAME];
    char region[24];
    float x, y, w, h;
} WenaUiControl;
void wena_ui_controls_begin(void);
void wena_ui_control_record(const char *region, const char *name,
                            float x, float y, float w, float h);
const WenaUiControl *wena_ui_controls(size_t *count);
/* The region the next controls belong to: "header", "list-header" ... */
void wena_ui_region(const char *region);

/* An icon-only control in the next layout slot; returns 1 when clicked. */
int wena_wekan_icon_button(struct nk_context *context, WenaIcon icon,
                           const char *name, float size, int color);
/* Shows the hovered icon's name as a tooltip. Call at window level (never
 * inside a group or layout-space row), before the window's nk_end. */
void wena_wekan_tooltip_flush(struct nk_context *context);
/* Icon and text without a background, as WeKan's header links and
 * "+ Add Card"; returns 1 when clicked. */
int wena_wekan_link(struct nk_context *context, WenaIcon icon, const char *text,
                    WenaWekanFont font, int color);
/* Text in the next slot, in one of WeKan's fonts and colors. */
void wena_wekan_text(struct nk_context *context, const char *text,
                     WenaWekanFont font, int color, int align);
/* WeKan's avatar initials (users.getInitials) of a full name: the first
 * letter of each word, upper-cased - whole UTF-8 letters - into `out`. */
void wena_wekan_initials(const char *name, char *out, size_t capacity);
/* WeKan's initials avatar in the next slot: the gray circle, the initials. */
void wena_wekan_avatar(struct nk_context *context, const char *name);
/* The same, wrapped over as many lines as the slot holds. */
void wena_wekan_text_wrap(struct nk_context *context, const char *text,
                          WenaWekanFont font, int color);
/* Lines `text` takes at `width` in `font`, word-wrapped as a browser does. */
int wena_wekan_wrapped_lines(struct nk_context *context, const char *text,
                             WenaWekanFont font, float width);
/* Text wrapped over the next slot that is itself one control, named by the
 * text: WeKan's card title, whose click edits it. Returns 1 when clicked. */
int wena_wekan_text_button(struct nk_context *context, const char *text,
                           WenaWekanFont font, int color);
/* WeKan's card section heading (cardSectionHeader): the caret - down when
 * open -, the section's icon and its name in 16px bold gray, one control
 * named by the name. Returns 1 when clicked. */
int wena_wekan_section_header(struct nk_context *context, int open, WenaIcon icon,
                              const char *label);
/* WeKan's checklist item (.materialCheckBox and its text): the box, ticked
 * when `checked`, then the text. Recorded by the text in the region
 * "checklist-checked" or "checklist-item", so its state can be read back.
 * Returns 1 when clicked and `interactive`. */
int wena_wekan_checkbox(struct nk_context *context, int checked, const char *text,
                        int interactive);
/* The next slot as one invisible control named `name`: a tile, a menu row.
 * Draw it yourself; returns 1 when clicked. */
int wena_wekan_area(struct nk_context *context, const char *name);
/* Text word-wrapped into a rectangle of the current window, drawn only. */
void wena_wekan_draw_wrapped(struct nk_context *context, const char *text, WenaWekanFont font, int color,
                             float x, float y, float w, float h);
/* WeKan's filled button: "Add", "Save". */
int wena_wekan_button(struct nk_context *context, const char *text, int color);
/* The screen rectangle of the row an nk_layout_space_begin just started:
 * nk_layout_space_bounds gives the panel's clip, not the row. */
struct nk_rect;
void wena_wekan_space_area(struct nk_context *context, float height, float *x, float *y, float *w);
/* Fill a rectangle of the current window, below whatever is drawn next. */
void wena_wekan_fill(struct nk_context *context, float x, float y, float w, float h,
                     int color, float rounding);
/* An nk_color for a WenaWekanColor, or a raw 0xRRGGBB at or above 0x1000000. */
int wena_wekan_rgb(WenaWekanColor color);

/* WeKan's popup menus ("List Actions", "Swimlane Actions"): a white panel with
 * a gray title bar, a close cross and bold items with gray icons, in columns. */
typedef struct WenaWekanMenuItem {
    WenaIcon icon;
    const char *text;
    int enabled;
    int separator_before;
    int checked;          /* WeKan's check after the current choice */
} WenaWekanMenuItem;
#define WENA_WEKAN_MENU_CLOSED (-2)
#define WENA_WEKAN_MENU_NONE (-1)
/* The chosen item's index, WENA_WEKAN_MENU_NONE, or WENA_WEKAN_MENU_CLOSED
 * when the cross or a click outside closed it. */
int wena_wekan_menu(struct nk_context *context, const char *title,
                    float x, float y, float width, int columns,
                    const WenaWekanMenuItem *items, size_t count);

#endif
