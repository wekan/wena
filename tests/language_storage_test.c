#define _POSIX_C_SOURCE 200809L
#include "../imports/i18n/language.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail_flush, fail_rename;
int wena_test_fflush(FILE *file)
{
    int result;
    result = fflush(file);
    if (fail_flush) { errno = ENOSPC; return EOF; }
    return result;
}
int wena_test_rename(const char *source, const char *destination)
{
    if (fail_rename) { errno = EACCES; return -1; }
    return rename(source, destination);
}
static void write_file(const char *path, const char *text)
{
    FILE *file;
    file = fopen(path, "wb"); assert(file);
    assert(fputs(text, file) >= 0); assert(fclose(file) == 0);
}
static void contents(const char *path, const char *text)
{
    char buffer[128];
    FILE *file;
    size_t length;
    file = fopen(path, "rb"); assert(file);
    length = fread(buffer, 1, sizeof(buffer), file);
    assert(length == strlen(text)); assert(memcmp(buffer, text, length) == 0);
    assert(fclose(file) == 0);
}
static void unchanged(WenaLanguageState *state, const WenaLanguageState *before)
{ assert(memcmp(state, before, sizeof(*state)) == 0); }
int main(int argc, char **argv)
{
    const char *const available[] = {"en", "fi"};
    char path[512], temporary[520], victim[512], missing[512];
    WenaLanguageState state, before;
    struct stat info;
    int mode;
    assert(argc == 2);
    sprintf(path, "%s/settings", argv[1]);
    sprintf(temporary, "%s.tmp", path);
    sprintf(victim, "%s/victim", argv[1]);
    sprintf(missing, "%s/missing", argv[1]);
    assert(wena_language_init(&state, path, "en", available, 2));
    assert(wena_language_set(&state, path, "fi", available, 2));
    contents(path, "fi\n"); assert(stat(path, &info) == 0);
    assert((info.st_mode & 0777) == 0600);
    assert(wena_language_init(&state, path, "en", available, 2));
    assert(strcmp(state.current, "fi") == 0); before = state;
    write_file(victim, "unrelated\n");
    for (mode = 0; mode < 3; ++mode) {
        if (mode == 0) write_file(temporary, "stale\n");
        else assert(symlink(mode == 1 ? victim : missing, temporary) == 0);
        assert(!wena_language_set(&state, path, "en", available, 2));
        unchanged(&state, &before); contents(path, "fi\n"); contents(victim, "unrelated\n");
        if (mode == 0) contents(temporary, "stale\n");
        else { assert(lstat(temporary, &info) == 0); assert(S_ISLNK(info.st_mode)); }
        assert(lstat(missing, &info) != 0 && errno == ENOENT);
        assert(unlink(temporary) == 0);
    }
    for (mode = 0; mode < 2; ++mode) {
        fail_flush = mode == 0; fail_rename = mode == 1;
        assert(!wena_language_set(&state, path, "en", available, 2));
        unchanged(&state, &before); contents(path, "fi\n");
        assert(lstat(temporary, &info) != 0 && errno == ENOENT);
    }
    fail_flush = fail_rename = 0;
    assert(unlink(path) == 0);
    for (mode = 0; mode < 2; ++mode) {
        assert(symlink(mode == 0 ? victim : missing, path) == 0);
        assert(!wena_language_set(&state, path, "en", available, 2));
        assert(!wena_language_clear(&state, path, "en", available, 2));
        unchanged(&state, &before); contents(victim, "unrelated\n");
        assert(lstat(path, &info) == 0 && S_ISLNK(info.st_mode));
        assert(wena_language_init(&state, path, "en", available, 2));
        assert(strcmp(state.current, "en") == 0 && !state.explicit_override);
        state = before; assert(unlink(path) == 0);
    }
    assert(mkdir(path, 0700) == 0);
    assert(!wena_language_set(&state, path, "en", available, 2));
    assert(!wena_language_clear(&state, path, "en", available, 2));
    unchanged(&state, &before); assert(rmdir(path) == 0);
    assert(wena_language_set(&state, path, "en", available, 2));
    contents(path, "en\n"); contents(victim, "unrelated\n");
    assert(wena_language_clear(&state, path, "fi", available, 2));
    assert(wena_language_clear(&state, path, "fi", available, 2));
    assert(strcmp(state.current, "fi") == 0 && !state.explicit_override);
    puts("language settings exclusive staging and failure tests passed");
    return 0;
}
