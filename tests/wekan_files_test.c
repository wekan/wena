/* WeKan's files directory: WRITABLE_PATH as WeKan's bundles read it, else
 * wekan-files beside the executable, and its attachments, avatars and
 * db/wekan.sqlite. */
#include "../client/platform/wekan_files.h"
#include "../client/platform/debug_log.h"
#include "../client/platform/files.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *root(const char *writable, const char *executable, const char *home, int system)
{
    static char out[512];
    return wena_wekan_files_root(writable, executable, home, system, out, sizeof(out)) ? out : NULL;
}

int main(int argc, char **argv)
{
    char path[512], file[600];
    const char *got;
    assert(argc == 2);
    /* Default: wekan-files beside the executable (WeKan's Windows .exe). */
    assert(!strcmp(root(NULL, "/opt/wena/wena-linux-amd64", NULL, WENA_SYSTEM_OTHER), "/opt/wena/wekan-files"));
    assert(!strcmp(root("", "C:\\Apps\\wena.exe", NULL, WENA_SYSTEM_WINDOWS), "C:\\Apps\\wekan-files"));
    assert(!strcmp(root(NULL, "/Applications/Wena/wena", NULL, WENA_SYSTEM_MACOS), "/Applications/Wena/wekan-files"));
    assert(!strcmp(root(NULL, NULL, NULL, WENA_SYSTEM_AMIGA), "PROGDIR:wekan-files"));
    assert(!strcmp(root(NULL, "/app/bin/x", "/data/user/0/fi.wekan.wena/files", WENA_SYSTEM_MOBILE),
                   "/data/user/0/fi.wekan.wena/files/wekan-files"));
    /* WRITABLE_PATH: "files" added, as start-wekan.sh does ... */
    assert(!strcmp(root("/home/u/.local/share/wekan", "/x/wena", NULL, WENA_SYSTEM_OTHER),
                   "/home/u/.local/share/wekan/files"));
    assert(!strcmp(root("D:\\data", "C:\\wena.exe", NULL, WENA_SYSTEM_WINDOWS), "D:\\data\\files"));
    /* ... but not again when it already is "files" or "wekan-files". */
    assert(!strcmp(root("/var/snap/wekan/common/files", "/x/wena", NULL, WENA_SYSTEM_OTHER),
                   "/var/snap/wekan/common/files"));
    assert(!strcmp(root("/srv/wekan-files/", "/x/wena", NULL, WENA_SYSTEM_OTHER), "/srv/wekan-files/"));
    assert(!strcmp(root("C:\\Wena\\wekan-files", "C:\\wena.exe", NULL, WENA_SYSTEM_WINDOWS), "C:\\Wena\\wekan-files"));
    /* Negative: relative paths, and no executable to put it beside. */
    assert(root("relative/files", "/x/wena", NULL, WENA_SYSTEM_OTHER) == NULL);
    assert(root(NULL, "wena", NULL, WENA_SYSTEM_OTHER) == NULL);
    assert(root(NULL, NULL, NULL, WENA_SYSTEM_OTHER) == NULL);
    assert(root(NULL, "/x", "relative", WENA_SYSTEM_MOBILE) == NULL);
    /* The database: db/wekan.sqlite, as FerretDB's --sqlite-url=file:<root>/db/. */
    assert(wena_wekan_files_database("/opt/wena/wekan-files", WENA_SYSTEM_OTHER, file, sizeof(file)));
    assert(!strcmp(file, "/opt/wena/wekan-files/db/wekan.sqlite"));
    assert(wena_wekan_files_database("C:\\Apps\\wekan-files", WENA_SYSTEM_WINDOWS, file, sizeof(file)));
    assert(!strcmp(file, "C:\\Apps\\wekan-files\\db\\wekan.sqlite"));
    assert(!wena_wekan_files_database("/opt/wekan-files", WENA_SYSTEM_OTHER, file, 20) && file[0] == '\0');
    /* The folders are made. */
    sprintf(path, "%s/bin/wena", argv[1]);
    got = root(NULL, path, NULL, WENA_SYSTEM_OTHER);
    assert(got != NULL);
    strcpy(path, got);
    assert(wena_wekan_files_prepare(path, WENA_SYSTEM_OTHER));
    sprintf(file, "%s/attachments", path); assert(wena_file_kind(file) == WENA_FILE_OTHER);
    sprintf(file, "%s/avatars", path); assert(wena_file_kind(file) == WENA_FILE_OTHER);
    sprintf(file, "%s/db", path); assert(wena_file_kind(file) == WENA_FILE_OTHER);
    assert(wena_wekan_files_prepare(path, WENA_SYSTEM_OTHER)); /* again: nothing to do */
    /* A database that is no database - 24 stray bytes, as libnix's lseek()
     * left a new wekan.sqlite on AmigaOS - is renamed aside, never removed. */
    {
        char database[1024], moved[1100], text[64];
        FILE *out;
        sprintf(database, "%s/db/wekan.sqlite", path);
        assert(wena_wekan_files_set_aside_broken(database, moved, sizeof(moved)) == 0 && moved[0] == '\0');
        out = fopen(database, "wb"); assert(out != NULL); fclose(out);
        assert(wena_wekan_files_set_aside_broken(database, moved, sizeof(moved)) == 0); /* empty: SQLite's own */
        out = fopen(database, "wb"); assert(out != NULL);
        fputs("ar(0)) = 0 AND card_id N", out); fclose(out);
        assert(wena_wekan_files_set_aside_broken(database, moved, sizeof(moved)) == 1);
        sprintf(text, "%s", moved + strlen(moved) - 15);
        assert(!strcmp(text, ".not-a-database") && wena_file_kind(database) == WENA_FILE_MISSING);
        assert(wena_file_kind(moved) == WENA_FILE_REGULAR);
        /* Again: the first name is taken, the next one is used. */
        out = fopen(database, "wb"); assert(out != NULL); fputs("JJJJ", out); fclose(out);
        assert(wena_wekan_files_set_aside_broken(database, moved, sizeof(moved)) == 1);
        assert(!strcmp(moved + strlen(moved) - 17, ".not-a-database-2"));
        /* Negative: a real SQLite header, and anything 512 bytes or more,
         * stay where they are; so does a missing file; no room for the name. */
        out = fopen(database, "wb"); assert(out != NULL);
        fwrite("SQLite format 3\0\020\000", 1, 18, out); fclose(out);
        assert(wena_wekan_files_set_aside_broken(database, moved, sizeof(moved)) == 0);
        out = fopen(database, "wb"); assert(out != NULL);
        { int i; for (i = 0; i < 600; ++i) fputc('J', out); }
        fclose(out);
        assert(wena_wekan_files_set_aside_broken(database, moved, sizeof(moved)) == 0);
        assert(wena_file_kind(database) == WENA_FILE_REGULAR);
        out = fopen(database, "wb"); assert(out != NULL); fputs("JJ", out); fclose(out);
        assert(wena_wekan_files_set_aside_broken(database, moved, 10) == -1 && wena_file_kind(database) == WENA_FILE_REGULAR);
        assert(wena_wekan_files_set_aside_broken(NULL, moved, sizeof(moved)) == -1);
    }
    puts("wekan-files: WRITABLE_PATH rules, beside the executable, folders and db/wekan.sqlite passed");
    return 0;
}
