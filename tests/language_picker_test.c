#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include <nuklear.h>
#include "../client/features/language_picker.h"
#include "../imports/i18n/ui_catalog.h"
#include "../imports/i18n/locale.h"
#include "../imports/ui/page_contract.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static float text_width(nk_handle handle, float height,
                        const char *text, int length)
{
    (void)handle; (void)text;
    return height * (float)length * 0.5f;
}
static size_t language_index(const WenaLanguagePicker *picker, const char *tag)
{
    size_t i;
    for (i = 0; i < picker->count; ++i)
        if (strcmp(picker->items[i], tag) == 0) return i;
    assert(0);
    return 0;
}
static void render(struct nk_context *ctx, WenaLanguagePicker *picker)
{
    const struct nk_command *command;
    if (nk_begin(ctx, "Language test", nk_rect(0, 0, 500, 450), 0))
        wena_language_picker_render(ctx, picker);
    nk_end(ctx);
    /* Match the SDL renderer: finalize command links every rendered frame. */
    nk_foreach(command, ctx) { (void)command; }
}
static struct nk_vec2 label_center(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, ctx) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0)
            return nk_vec2((float)text->x + (float)text->w * 0.5f,
                           (float)text->y + (float)text->h * 0.5f);
    }
    fprintf(stderr, "missing rendered control: %s\n", label);
    assert(0);
    return nk_vec2(0, 0);
}
static int has_label(struct nk_context *ctx, const char *label)
{
    const struct nk_command *command;
    const struct nk_command_text *text;
    nk_foreach(command, ctx) {
        if (command->type != NK_COMMAND_TEXT) continue;
        text = (const struct nk_command_text *)command;
        if ((size_t)text->length == strlen(label) &&
            memcmp(text->string, label, (size_t)text->length) == 0) return 1;
    }
    return 0;
}

static void click(struct nk_context *ctx, WenaLanguagePicker *picker,
                  struct nk_vec2 point)
{
    int down;
    for (down = 1; down >= 0; --down) {
        nk_clear(ctx);
        nk_input_begin(ctx);
        nk_input_motion(ctx, (int)point.x, (int)point.y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)point.x, (int)point.y, down);
        nk_input_end(ctx);
        render(ctx, picker);
    }
}
int main(int argc, char **argv)
{
    WenaLanguageState language, before, loaded;
    WenaLanguagePicker picker, readonly, failed;
    struct nk_context ctx;
    struct nk_user_font font;
    const char *const *available;
    const char *const spellings[] = {"en", "en_US", "en-US"};
    size_t count, selected, fi;
    char path[512], bad_path[512], oversized[600], resolved[64];
    assert(argc == 2);
    sprintf(path, "%s/language.conf", argv[1]);
    sprintf(bad_path, "%s/absent/language.conf", argv[1]);
    available = wena_ui_catalog_languages(&count);
    assert(wena_language_init(&language, NULL, "en", available, count));
    assert(!wena_language_picker_init(NULL, &language, path, 0));
    assert(!wena_language_picker_init(&failed, NULL, path, 0));
    assert(!wena_language_picker_init(&failed, &language, NULL, 0));
    memset(oversized, 'x', sizeof(oversized)); oversized[sizeof(oversized)-1] = 0;
    assert(!wena_language_picker_init(&failed, &language, oversized, 0));
    assert(wena_language_picker_init(&picker, &language, path, 0));
    assert(picker.count == count && picker.writable && !picker.error);
    assert(strcmp(picker.settings_path, path) == 0);
    fi = language_index(&picker, "fi");
    before = language; selected = picker.selected;
    assert(!wena_language_picker_select(NULL, 0));
    assert(!wena_language_picker_select(&picker, count));
    assert(picker.error && picker.selected == selected);
    assert(memcmp(&language, &before, sizeof(language)) == 0);
    assert(!wena_language_picker_select(&picker, (size_t)-1));
    assert(wena_language_picker_select(&picker, fi));
    assert(!picker.error && picker.selected == fi && strcmp(language.current, "fi") == 0);
    assert(wena_language_init(&loaded, path, "en", available, count));
    assert(strcmp(loaded.current, "fi") == 0 && loaded.explicit_override);
    assert(wena_language_picker_init(&readonly, &language, NULL, 1));
    assert(!wena_language_picker_select(&readonly, language_index(&readonly, "en")));
    assert(strcmp(language.current, "fi") == 0);
    assert(wena_language_picker_init(&failed, &language, bad_path, 0));
    before = language; selected = failed.selected;
    assert(!wena_language_picker_select(&failed, language_index(&failed, "en")));
    assert(failed.error && failed.selected == selected);
    assert(memcmp(&language, &before, sizeof(language)) == 0);
    assert(wena_language_picker_select(&failed, fi));
    assert(!failed.error && memcmp(&language, &before, sizeof(language)) == 0);
    /* Null state cannot rewrite or remove an existing language setting. */
    assert(!wena_language_set(NULL, path, "ar", available, count));
    assert(!wena_language_clear(NULL, path, "ar", available, count));
    assert(wena_language_init(&loaded, path, "en", available, count));
    assert(strcmp(loaded.current, "fi") == 0);
    assert(wena_locale_resolve("en_US.UTF-8", spellings, 2, resolved, sizeof(resolved)));
    assert(strcmp(resolved, "en_US") == 0);
    assert(wena_locale_resolve("en_US.UTF-8", spellings, 3, resolved, sizeof(resolved)));
    assert(strcmp(resolved, "en-US") == 0);
    assert(wena_locale_resolve("en_US", spellings, 3, resolved, sizeof(resolved)));
    assert(strcmp(resolved, "en_US") == 0);
    memset(&font, 0, sizeof(font)); font.height = 14.0f; font.width = text_width;
    assert(nk_init_default(&ctx, &font));
    wena_ui_set_translator(wena_ui_catalog_translate, &language);
    render(&ctx, &picker); label_center(&ctx, "Kieli"); label_center(&ctx, "fi");
    /* Failed persistence must report a visible, localized generic error while
     * retaining both the current choice and the last durable preference. */
    assert(!wena_language_picker_select(&failed, language_index(&failed, "en")));
    nk_clear(&ctx); render(&ctx, &failed);
    assert(has_label(&ctx, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED)));
    label_center(&ctx, "fi");
    assert(wena_language_init(&loaded, path, "en", available, count));
    assert(strcmp(loaded.current, "fi") == 0);
    assert(wena_language_picker_select(&failed, language_index(&failed, "fi")));
    nk_clear(&ctx); render(&ctx, &failed);
    assert(!has_label(&ctx, wena_ui_text(WENA_UI_TEXT_OPERATION_FAILED)));
    assert(wena_language_picker_select(&picker, language_index(&picker, "ace")));
    nk_clear(&ctx); render(&ctx, &picker);
    click(&ctx, &picker, label_center(&ctx, "ace"));
    click(&ctx, &picker, label_center(&ctx, "af"));
    assert(strcmp(language.current, "af") == 0);
    assert(wena_language_init(&loaded, path, "en", available, count));
    assert(strcmp(loaded.current, "af") == 0);
    nk_clear(&ctx); render(&ctx, &readonly);
    label_center(&ctx, "af");
    click(&ctx, &readonly, label_center(&ctx, "af"));
    assert(strcmp(language.current, "af") == 0);
    assert(readonly.selected == language_index(&readonly, "af"));
    wena_language_picker_render(NULL, &picker);
    wena_language_picker_render(&ctx, NULL);
    wena_ui_set_translator(NULL, NULL);
    nk_free(&ctx);
    remove(path);
    puts("real Nuklear language picker checks passed");
    return 0;
}
