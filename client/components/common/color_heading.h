#ifndef WENA_COLOR_HEADING_H
#define WENA_COLOR_HEADING_H
struct nk_context;
/* Caller owns the row. Empty/invalid colors preserve theme styling; stored
 * item colors paint the whole cell with readable foreground. No style leaks. */
void wena_color_heading(struct nk_context *context,const char *title,
    const char *color,int wrap);
/* Selected headings use the active selectable theme without changing input,
 * layout or persistent styles. Titles may wrap and retain their full width. */
void wena_selection_heading(struct nk_context *context,const char *title,int selected,int wrap);
#endif
