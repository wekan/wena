#ifndef WENA_SDL_NUKLEAR_H
#define WENA_SDL_NUKLEAR_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "../../third_party/nuklear/nuklear.h"
#include "../../third_party/nuklear/demo/sdl_renderer/nuklear_sdl_renderer.h"

/* Pass the context returned by nk_sdl_init. Text events are validated and
 * accepted atomically, including all their UTF-8 characters. Invalid/control
 * text or frame-capacity overflow returns 0 without changing text input.
 * Desktop builds set NK_INPUT_MAX=256 consistently in every translation unit. */
int wena_sdl_handle_event(struct nk_context *context, SDL_Event *event);

/* Install after nk_sdl_init. Keeps the pinned copy callback and replaces paste
 * with strict UTF-8, selection and fixed-capacity preflight. Invalid/oversized
 * paste preserves the complete draft, selection and undo state. Multiline
 * editors accept LF/CR/TAB; single-line editors reject those controls. */
void wena_sdl_install_clipboard(struct nk_context *context);
/* Same byte-length preflight used by the SDL callback; input must not alias the
 * editor buffer. Only fixed buffers used by native nk_edit_string are supported.
 * Empty input succeeds without deleting selection. No persisted mutation. */
int wena_sdl_paste_text(struct nk_text_edit *edit, const char *text, size_t length);

#endif
