#ifndef WENA_SDL_NUKLEAR_H
#define WENA_SDL_NUKLEAR_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

/* Pinned third-party code: newer clang reports Nuklear's portable alignof
 * (a struct defined inside offsetof) as a C23 extension, which -Werror and
 * -pedantic-errors turn into a build failure. Only that warning, only for
 * these two headers, and only on a clang that knows it. */
#if defined(__clang__) && defined(__has_warning)
#if __has_warning("-Wc23-extensions")
#define WENA_NUKLEAR_C23_OFFSETOF
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
#endif
#include "../../third_party/nuklear/nuklear.h"
#include "../../third_party/nuklear/demo/sdl_renderer/nuklear_sdl_renderer.h"
#ifdef WENA_NUKLEAR_C23_OFFSETOF
#pragma clang diagnostic pop
#undef WENA_NUKLEAR_C23_OFFSETOF
#endif

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
