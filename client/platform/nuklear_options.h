#ifndef WENA_NUKLEAR_OPTIONS_H
#define WENA_NUKLEAR_OPTIONS_H
/* The one set of Nuklear options. Every translation unit must see the same
 * options before <nuklear.h>: they add fields to struct nk_context, and a file
 * compiled without them reads context->current, input and layout at other
 * offsets than the Nuklear implementation wrote them - a drag handle read a
 * NULL window and crashed the desktop on macOS. Include this first.
 * tests/test_nuklear_options.sh fails when a source includes <nuklear.h>
 * without it, or when two units disagree on the layout. */
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#ifndef NK_INPUT_MAX
#define NK_INPUT_MAX 256
#endif
/* Newer clang reports the pinned Nuklear's portable alignof (a struct defined
 * inside offsetof) as a C23 extension, which -Werror and -pedantic-errors turn
 * into a build failure wherever the implementation is compiled. Only that
 * warning, only on a clang that knows it; gcc builds keep it. */
#if defined(__clang__) && defined(__has_warning)
#if __has_warning("-Wc23-extensions")
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
#endif
#endif
