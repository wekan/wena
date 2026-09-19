#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "sdl_nuklear.h"
#include <limits.h>

static int text_scalar(const unsigned char *text, size_t length, size_t *offset,
                        nk_rune *rune)
{
    unsigned long first, code, need, minimum;
    size_t index;
    if (*offset >= length) return 0;
    index = *offset; first = text[index++];
    if (first > 0 && first < 128) {
        code = first; need = 0; minimum = 0;
    } else if (first >= 194 && first <= 223) {
        code = first & 31; need = 1; minimum = 128;
    } else if (first >= 224 && first <= 239) {
        code = first & 15; need = 2; minimum = 2048;
    } else if (first >= 240 && first <= 244) {
        code = first & 7; need = 3; minimum = 65536;
    } else return 0;
    while (need) {
        if (index >= length || (text[index] & 192) != 128) return 0;
        code = (code << 6) | (text[index++] & 63); --need;
    }
    if (code < minimum || code > 1114111 ||
        (code >= 55296 && code <= 57343)) return 0;
    *offset = index; *rune = (nk_rune)code; return 1;
}

int wena_sdl_paste_text(struct nk_text_edit *edit, const char *text, size_t length)
{
    size_t offset, selected_start, selected_end, amount, expected;
    int count, start, end;
    unsigned char mode;
    nk_rune rune;
    nk_plugin_filter filter;
    if (!edit || (!text && length) || edit->mode == NK_TEXT_EDIT_MODE_VIEW ||
        edit->string.buffer.type != NK_BUFFER_FIXED ||
        !edit->string.buffer.memory.ptr || !edit->string.buffer.memory.size ||
        edit->string.buffer.memory.size > (nk_size)INT_MAX ||
        edit->string.buffer.allocated >= edit->string.buffer.memory.size ||
        edit->string.len < 0 || edit->cursor < 0 || edit->cursor > edit->string.len ||
        edit->select_start < 0 || edit->select_start > edit->string.len ||
        edit->select_end < 0 || edit->select_end > edit->string.len ||
        length >= (size_t)edit->string.buffer.memory.size) return 0;
    if (!length) return 1;
    start = NK_MIN(edit->select_start, edit->select_end);
    end = NK_MAX(edit->select_start, edit->select_end);
    offset = 0; count = 0; selected_start = selected_end = 0;
    amount = (size_t)edit->string.buffer.allocated;
    while (offset < amount) {
        if (count == start) selected_start = offset;
        if (count == end) selected_end = offset;
        if (!text_scalar((const unsigned char *)edit->string.buffer.memory.ptr,
            amount, &offset, &rune)) return 0;
        ++count;
    }
    if (count != edit->string.len) return 0;
    if (start == count) selected_start = offset;
    if (end == count) selected_end = offset;
    expected = amount - (selected_end - selected_start);
    if (length >= (size_t)edit->string.buffer.memory.size - expected) return 0;
    expected += length;
    offset = 0;
    while (offset < length) {
        if (!text_scalar((const unsigned char *)text, length, &offset, &rune) ||
            rune == 127 || (rune >= 128 && rune <= 159) ||
            (rune < 32 && (edit->single_line ||
                (rune != 9 && rune != 10 && rune != 13))) ||
            (edit->filter && !edit->filter(edit, rune))) return 0;
    }
    /* The pinned nk_textedit_paste confuses bytes with rune counts and can
     * over-read multibyte clipboard strings. Its ordinary text-input API takes
     * an explicit byte length. Preflight guarantees all runes fit before that
     * API deletes selection or changes undo state. Paste always inserts. */
    mode = edit->mode; filter = edit->filter;
    edit->mode = NK_TEXT_EDIT_MODE_INSERT; edit->filter = NULL;
    nk_textedit_text(edit, text, (int)length);
    edit->mode = mode; edit->filter = filter;
    return edit->string.buffer.allocated == (nk_size)expected;
}

static void clipboard_paste(nk_handle user, struct nk_text_edit *edit)
{
    char *text;
    size_t length, capacity;
    (void)user;
    if (!edit || edit->string.buffer.type != NK_BUFFER_FIXED ||
        edit->string.buffer.memory.size > (nk_size)INT_MAX) return;
    text = SDL_GetClipboardText();
    if (!text) return;
    capacity = (size_t)edit->string.buffer.memory.size;
    for (length = 0; length < capacity && text[length]; ++length) {}
    if (length < capacity) (void)wena_sdl_paste_text(edit, text, length);
    SDL_free(text);
}

void wena_sdl_install_clipboard(struct nk_context *context)
{
    if (context) context->clip.paste = clipboard_paste;
}

int wena_sdl_handle_event(struct nk_context *context, SDL_Event *event)
{
    nk_rune runes[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    const unsigned char *text;
    size_t length, index, count, current;
    nk_rune rune;
    if (!context || !event) return 0;
    if (event->type != SDL_TEXTINPUT) return nk_sdl_handle_event(event);
    text = (const unsigned char *)event->text.text;
    for (length = 0; length < sizeof(event->text.text) && text[length]; ++length) {}
    if (length == sizeof(event->text.text)) return 0;
    /* SDL text events are complete UTF-8 strings, not single glyphs. Validate
     * the entire event and available frame capacity before publishing any rune.
     * Keyboard control actions arrive separately through SDL_KEYDOWN/KEYUP. */
    index = 0;
    count = 0;
    while (index < length) {
        if (!text_scalar(text, length, &index, &rune) || rune < 32 || rune == 127 ||
            (rune >= 128 && rune <= 159)) return 0;
        runes[count++] = rune;
    }
    if (context->input.keyboard.text_len < 0 ||
        context->input.keyboard.text_len >= NK_INPUT_MAX) return 0;
    current = (size_t)context->input.keyboard.text_len;
    /* Nuklear reserves the last byte of its input buffer. */
    if (length >= (size_t)NK_INPUT_MAX - current) return 0;
    for (index = 0; index < count; ++index)
        nk_input_unicode(context, runes[index]);
    return 1;
}
