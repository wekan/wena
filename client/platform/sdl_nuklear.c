#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "sdl_nuklear.h"

int wena_sdl_handle_event(struct nk_context *context, SDL_Event *event)
{
    nk_rune runes[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    const unsigned char *text;
    size_t length, index, count, current;
    unsigned long first, code, need, minimum;
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
        first = text[index++];
        if (first < 32 || first == 127) return 0;
        if (first < 128) {
            code = first;
            need = 0;
            minimum = 0;
        } else if (first >= 194 && first <= 223) {
            code = first & 31; need = 1; minimum = 128;
        } else if (first >= 224 && first <= 239) {
            code = first & 15; need = 2; minimum = 2048;
        } else if (first >= 240 && first <= 244) {
            code = first & 7; need = 3; minimum = 65536;
        } else return 0;
        while (need) {
            if (index >= length || (text[index] & 192) != 128) return 0;
            code = (code << 6) | (text[index++] & 63);
            --need;
        }
        if (code < minimum || code > 1114111 ||
            (code >= 55296 && code <= 57343) ||
            (code >= 128 && code <= 159)) return 0;
        runes[count++] = (nk_rune)code;
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
