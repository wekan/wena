/* Desktop debug log: where a run's log goes, the default board file, and that
 * a log line and a fatal signal reach desktop.log. */
#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../client/platform/debug_log.h"
#include "../client/platform/files.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void read_file(const char *path, char *out, size_t capacity)
{
    FILE *file = fopen(path, "r");
    size_t length;
    assert(file != NULL);
    length = fread(out, 1, capacity - 1, file);
    out[length] = '\0';
    fclose(file);
}

int main(int argc, char **argv)
{
    char out[256], small[24], path[1024], text[4096], expected[1024];
    pid_t child;
    int status;
    const char *dir = argc > 1 ? argv[1] : NULL;

    /* Under a checkout's .tools/wena: that .tools/log/wena/<stamp>. */
    assert(wena_debug_log_directory_for(NULL, "/r/.tools/wena/dist/desktop/wena-desktop",
                                        "2026-10-03_15-04-05", out, sizeof(out)));
    assert(!strcmp(out, "/r/.tools/log/wena/2026-10-03_15-04-05"));
    /* WENA_LOG_DIR wins. */
    assert(wena_debug_log_directory_for("/logs/x", "/r/.tools/wena/dist/desktop/wena-desktop",
                                        "s", out, sizeof(out)));
    assert(!strcmp(out, "/logs/x"));
    /* Negative: no marker, a relative WENA_LOG_DIR, no stamp, and too small. */
    assert(!wena_debug_log_directory_for(NULL, "/usr/local/bin/wena-desktop", "s", out, sizeof(out)));
    assert(!wena_debug_log_directory_for("relative", NULL, "s", out, sizeof(out)));
    assert(!wena_debug_log_directory_for(NULL, "/r/.tools/wena/x", "", out, sizeof(out)));
    assert(!wena_debug_log_directory_for(NULL, "/r/.tools/wena/x", "2026-10-03_15-04-05", small, sizeof(small)));

    /* The default board file. */
    assert(wena_desktop_default_database(NULL, "/Users/u", NULL, WENA_SYSTEM_MACOS, out, sizeof(out)));
    assert(!strcmp(out, "/Users/u/Library/Application Support/Wena/wena.sqlite"));
    assert(wena_desktop_default_database(NULL, "/home/u", NULL, WENA_SYSTEM_OTHER, out, sizeof(out)));
    assert(!strcmp(out, "/home/u/.local/share/wena/wena.sqlite"));
    assert(wena_desktop_default_database(NULL, "/home/u", "/data", WENA_SYSTEM_OTHER, out, sizeof(out)));
    assert(!strcmp(out, "/data/wena/wena.sqlite"));
    assert(wena_desktop_default_database("/x/b.sqlite", "/home/u", "/data", WENA_SYSTEM_MACOS, out, sizeof(out)));
    assert(!strcmp(out, "/x/b.sqlite"));
    /* Windows: %APPDATA%\Wena, a drive or UNC WENA_DATABASE, and its log folder. */
    assert(wena_desktop_default_database(NULL, "C:\\Users\\u\\AppData\\Roaming", NULL, WENA_SYSTEM_WINDOWS, out, sizeof(out)));
    assert(!strcmp(out, "C:\\Users\\u\\AppData\\Roaming\\Wena\\wena.sqlite"));
    assert(wena_desktop_default_database("D:/boards/w.sqlite", NULL, NULL, WENA_SYSTEM_WINDOWS, out, sizeof(out)));
    assert(wena_desktop_default_database("\\\\server\\share\\w.sqlite", NULL, NULL, WENA_SYSTEM_WINDOWS, out, sizeof(out)));
    assert(!wena_desktop_default_database("/x/b.sqlite", NULL, NULL, WENA_SYSTEM_WINDOWS, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, "relative", NULL, WENA_SYSTEM_WINDOWS, out, sizeof(out)));
    assert(wena_debug_log_directory_for(NULL, "C:\\r\\.tools\\wena\\dist\\desktop\\wena-desktop.exe",
                                        "2026-10-03_15-04-05", out, sizeof(out)));
    assert(!strcmp(out, "C:\\r\\.tools\\log\\wena\\2026-10-03_15-04-05"));
    assert(wena_debug_log_directory_for("C:\\logs", NULL, "s", out, sizeof(out)) && !strcmp(out, "C:\\logs"));
    /* Path rules per platform. */
    assert(wena_path_absolute_for("/x", 0) && !wena_path_absolute_for("C:\\x", 0));
    assert(wena_path_absolute_for("C:\\x", 1) && wena_path_absolute_for("c:/x", 1));
    assert(wena_path_absolute_for("\\\\server\\share", 1));
    assert(!wena_path_absolute_for("/x", 1) && !wena_path_absolute_for("C:x", 1));
    assert(!wena_path_absolute_for("\\\\", 1) && !wena_path_absolute_for("", 0) && !wena_path_absolute_for(NULL, 1));
    /* AmigaOS and AROS: the program's drawer, and "Volume:" or "ASSIGN:" names. */
    assert(wena_desktop_default_database(NULL, NULL, NULL, WENA_SYSTEM_AMIGA, out, sizeof(out)));
    assert(!strcmp(out, "PROGDIR:wena.sqlite"));
    assert(wena_desktop_default_database("Work:Wena/b.sqlite", "/home/u", "/data", WENA_SYSTEM_AMIGA, out, sizeof(out)));
    assert(!strcmp(out, "Work:Wena/b.sqlite"));
    assert(!wena_desktop_default_database("/x/b.sqlite", NULL, NULL, WENA_SYSTEM_AMIGA, out, sizeof(out)));
    assert(!wena_desktop_default_database("b.sqlite", NULL, NULL, WENA_SYSTEM_AMIGA, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, NULL, NULL, WENA_SYSTEM_AMIGA, small, 19));
    assert(!wena_desktop_default_database("Work:b.sqlite", NULL, NULL, WENA_SYSTEM_OTHER, out, sizeof(out)));
    assert(wena_path_absolute_amiga("PROGDIR:wena.sqlite") && wena_path_absolute_amiga("DH0:a/b"));
    assert(wena_path_absolute_amiga("Work:") && wena_path_absolute_amiga("RAM Disk:x"));
    assert(!wena_path_absolute_amiga(":x") && !wena_path_absolute_amiga("x") && !wena_path_absolute_amiga("/x"));
    assert(!wena_path_absolute_amiga("a/b:c") && !wena_path_absolute_amiga("a:b:c"));
    assert(!wena_path_absolute_amiga("") && !wena_path_absolute_amiga(NULL));
#if !defined(__amigaos__) && !defined(__AROS__)
    /* Elsewhere a colon is part of a name: the host rules are unchanged. */
    assert(!wena_path_absolute("Work:x") && !wena_path_absolute("PROGDIR:wena.sqlite"));
    assert(!wena_debug_log_directory_for("RAM:log", NULL, "s", out, sizeof(out)));
#else
    assert(wena_path_absolute("PROGDIR:wena.sqlite") && !wena_path_absolute("/x"));
    assert(wena_debug_log_directory_for("RAM:log", NULL, "s", out, sizeof(out)) && !strcmp(out, "RAM:log"));
#endif
    /* Android and iOS: the app's own data folder (SDL_GetPrefPath), with or
     * without its trailing separator; WENA_DATABASE still wins. */
    assert(wena_desktop_default_database(NULL, "/data/user/0/fi.wekan.wena/files/", NULL,
                                         WENA_SYSTEM_MOBILE, out, sizeof(out)));
    assert(!strcmp(out, "/data/user/0/fi.wekan.wena/files/wena.sqlite"));
    assert(wena_desktop_default_database(NULL, "/var/mobile/Library/Application Support/wekan/wena",
                                         NULL, WENA_SYSTEM_MOBILE, out, sizeof(out)));
    assert(!strcmp(out, "/var/mobile/Library/Application Support/wekan/wena/wena.sqlite"));
    assert(wena_desktop_default_database("/x/b.sqlite", "/data/files/", NULL, WENA_SYSTEM_MOBILE, out, sizeof(out)));
    assert(!strcmp(out, "/x/b.sqlite"));
    assert(wena_debug_log_data_directory_for(NULL, "/data/files/", out, sizeof(out)));
    assert(!strcmp(out, "/data/files/log"));
    assert(wena_debug_log_data_directory_for(NULL, "/data/files", out, sizeof(out)));
    assert(!strcmp(out, "/data/files/log"));
    assert(wena_debug_log_data_directory_for("/logs/x", "/data/files/", out, sizeof(out)));
    assert(!strcmp(out, "/logs/x"));
    /* Negative: no data folder (SDL_GetPrefPath failed), a relative one, and too small. */
    assert(!wena_desktop_default_database(NULL, NULL, NULL, WENA_SYSTEM_MOBILE, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, "", NULL, WENA_SYSTEM_MOBILE, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, "files/", NULL, WENA_SYSTEM_MOBILE, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, "/data/user/0/fi.wekan.wena/files/", NULL,
                                          WENA_SYSTEM_MOBILE, small, sizeof(small)));
    assert(!wena_debug_log_data_directory_for(NULL, NULL, out, sizeof(out)));
    assert(!wena_debug_log_data_directory_for(NULL, "files/", out, sizeof(out)));
    assert(!wena_debug_log_data_directory_for("relative", "/data/files/", out, sizeof(out)));
    assert(!wena_debug_log_data_directory_for(NULL, "/data/user/0/fi.wekan.wena/files/", small, sizeof(small)));
    /* Negative: relative WENA_DATABASE, no home, and too small. */
    assert(!wena_desktop_default_database("b.sqlite", "/home/u", NULL, WENA_SYSTEM_OTHER, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, NULL, NULL, WENA_SYSTEM_MACOS, out, sizeof(out)));
    assert(!wena_desktop_default_database(NULL, "/home/u", NULL, WENA_SYSTEM_MACOS, small, sizeof(small)));

    /* The bounded formatter: what the log's calls use, cut and terminated. */
    {
        char line[64], tiny[6];
        size_t n;
        wena_debug_log_format(line, sizeof(line), "%s %d %u %ld %lu %x %c %%", "a", -12, 7u, -300000L, 4000000000UL, 255u, 'z');
        assert(!strcmp(line, "a -12 7 -300000 4000000000 ff z %"));
        wena_debug_log_format(line, sizeof(line), "%.2f %.0f %f", 1.25, 2.5, -0.25);
        assert(!strcmp(line, "1.25 3 -0.250000"));
        wena_debug_log_format(line, sizeof(line), "%s|%q|%", (const char *)NULL);
        assert(!strcmp(line, "(null)|%q|"));
        wena_debug_log_format(tiny, sizeof(tiny), "%s", "abcdefgh");
        assert(!strcmp(tiny, "abcde"));
        /* Every line is kept in memory, oldest first, the last 48 only -
         * with or without a log file. */
        for (n = 0; n < WENA_DEBUG_LOG_RECENT + 5; ++n) wena_debug_log("step %d", (int)n);
        assert(wena_debug_log_recent_count() == WENA_DEBUG_LOG_RECENT);
        assert(!strcmp(wena_debug_log_recent_line(0), "step 5"));
        wena_debug_log_format(line, sizeof(line), "step %d", WENA_DEBUG_LOG_RECENT + 4);
        assert(!strcmp(wena_debug_log_recent_line(WENA_DEBUG_LOG_RECENT - 1), line));
        assert(!strcmp(wena_debug_log_recent_line(WENA_DEBUG_LOG_RECENT), ""));
        /* A long line is cut, not overflowed. */
        {
            char longer[600];
            memset(longer, 'x', sizeof(longer) - 1);
            longer[sizeof(longer) - 1] = '\0';
            wena_debug_log("%s", longer);
            assert(strlen(wena_debug_log_recent_line(WENA_DEBUG_LOG_RECENT - 1)) == WENA_DEBUG_LOG_RECENT_WIDTH - 1);
        }
    }
    /* Outside a checkout the log is wena-debug-log.txt beside the program:
     * PROGDIR: on AmigaOS and AROS, with or without an executable name. */
    assert(wena_debug_log_beside_for(WENA_SYSTEM_AMIGA, NULL, out, sizeof(out)) &&
           !strcmp(out, "PROGDIR:wena-debug-log.txt"));
    assert(wena_debug_log_beside_for(WENA_SYSTEM_AMIGA, "Work:Wena/wena-amigaos-m68k", out, sizeof(out)) &&
           !strcmp(out, "PROGDIR:wena-debug-log.txt"));
    assert(wena_debug_log_beside_for(WENA_SYSTEM_OTHER, "/opt/wena/wena-linux-amd64", out, sizeof(out)) &&
           !strcmp(out, "/opt/wena/wena-debug-log.txt"));
    assert(wena_debug_log_beside_for(WENA_SYSTEM_MACOS, "/Applications/Wena.app/Contents/MacOS/wena", out, sizeof(out)) &&
           !strcmp(out, "/Applications/Wena.app/Contents/MacOS/wena-debug-log.txt"));
    assert(wena_debug_log_beside_for(WENA_SYSTEM_WINDOWS, "C:\\Wena\\wena.exe", out, sizeof(out)) &&
           !strcmp(out, "C:\\Wena\\wena-debug-log.txt"));
    assert(wena_debug_log_beside_for(WENA_SYSTEM_WINDOWS, "C:/Wena/wena.exe", out, sizeof(out)) &&
           !strcmp(out, "C:/Wena/wena-debug-log.txt"));
    /* Negative: no executable, no folder in it, a backslash outside
     * Windows, and too small. */
    assert(!wena_debug_log_beside_for(WENA_SYSTEM_OTHER, NULL, out, sizeof(out)) && out[0] == '\0');
    assert(!wena_debug_log_beside_for(WENA_SYSTEM_OTHER, "wena", out, sizeof(out)) && out[0] == '\0');
    assert(!wena_debug_log_beside_for(WENA_SYSTEM_OTHER, "C:\\Wena\\wena.exe", out, sizeof(out)));
    assert(!wena_debug_log_beside_for(WENA_SYSTEM_OTHER, "/opt/wena/wena", small, sizeof(small)) && small[0] == '\0');
    assert(!wena_debug_log_beside_for(WENA_SYSTEM_AMIGA, NULL, small, sizeof(small)));
    /* desktop.log joined without a "/" after a device or a separator. */
    assert(wena_debug_log_file_for("PROGDIR:wena-log", out, sizeof(out)) && !strcmp(out, "PROGDIR:wena-log/desktop.log"));
    assert(wena_debug_log_file_for("RAM:", out, sizeof(out)) && !strcmp(out, "RAM:desktop.log"));
    assert(wena_debug_log_file_for("/logs/", out, sizeof(out)) && !strcmp(out, "/logs/desktop.log"));
    assert(wena_debug_log_file_for("/logs", out, sizeof(out)) && !strcmp(out, "/logs/desktop.log"));
    assert(wena_debug_log_file_for("C:\\logs\\", out, sizeof(out)) && !strcmp(out, "C:\\logs\\desktop.log"));
    assert(!wena_debug_log_file_for("", out, sizeof(out)) && !wena_debug_log_file_for(NULL, out, sizeof(out)));
    assert(!wena_debug_log_file_for("/a/long/folder", small, 12));

    if (dir == NULL) return 0;
    /* Folders are created, a line is written, and a crash leaves its signal. */
    sprintf(path, "%s/a/b/board.sqlite", dir);
    assert(wena_make_parent_directories(path));
    assert(!wena_make_parent_directories("relative/board.sqlite"));
    sprintf(path, "%s/run", dir);
    assert(setenv("WENA_LOG_DIR", path, 1) == 0);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        if (!wena_debug_log_open(NULL)) _exit(3);
        wena_debug_log("hello %d", 42);
        raise(SIGSEGV);
        _exit(4);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
    sprintf(path, "%s/run/desktop.log", dir);
    read_file(path, text, sizeof(text));
    assert(strstr(text, " hello 42\n") != NULL);
    sprintf(expected, "CRASH: fatal signal %d\n", SIGSEGV);
    assert(strstr(text, expected) != NULL);
    /* The data-folder log keeps only the last run: opening truncates it. */
    sprintf(path, "%s/data/", dir);
    assert(unsetenv("WENA_LOG_DIR") == 0);
    assert(wena_debug_log_open_data(path));
    wena_debug_log("first run");
    wena_debug_log_close();
    assert(wena_debug_log_open_data(path));
    sprintf(expected, "%s/data/log", dir);
    assert(!strcmp(wena_debug_log_directory(), expected));
    wena_debug_log("second run");
    wena_debug_log_close();
    sprintf(path, "%s/data/log/desktop.log", dir);
    read_file(path, text, sizeof(text));
    assert(strstr(text, " second run\n") != NULL && strstr(text, "first run") == NULL);
    /* Negative: no data folder, no log. */
    assert(!wena_debug_log_open_data(NULL) && wena_debug_log_directory()[0] == '\0');
    /* Outside a checkout, with no WENA_LOG_DIR: wena-debug-log.txt beside
     * the executable, the last run only. */
    sprintf(path, "%s/bin/x", dir);
    assert(wena_make_parent_directories(path));
    sprintf(path, "%s/bin/wena-linux-amd64", dir);
    assert(wena_debug_log_open(path));
    wena_debug_log("first start");
    wena_debug_log_close();
    assert(wena_debug_log_open(path));
    sprintf(expected, "%s/bin/wena-debug-log.txt", dir);
    assert(!strcmp(wena_debug_log_path(), expected));
    sprintf(expected, "%s/bin/", dir);
    assert(!strcmp(wena_debug_log_directory(), expected));
    wena_debug_log("second start");
    wena_debug_log_close();
    sprintf(path, "%s/bin/wena-debug-log.txt", dir);
    read_file(path, text, sizeof(text));
    assert(strstr(text, " second start\n") != NULL && strstr(text, "first start") == NULL);
    /* WENA_LOG_DIR still wins over the executable's folder. */
    sprintf(path, "%s/chosen", dir);
    assert(setenv("WENA_LOG_DIR", path, 1) == 0);
    sprintf(path, "%s/bin/wena-linux-amd64", dir);
    assert(wena_debug_log_open(path));
    sprintf(expected, "%s/chosen/desktop.log", dir);
    assert(!strcmp(wena_debug_log_path(), expected));
    wena_debug_log_close();
    assert(unsetenv("WENA_LOG_DIR") == 0);
    /* Negative: an executable with no folder, a folder that cannot be
     * written - no log, and nothing named. */
    assert(!wena_debug_log_open("wena") && wena_debug_log_path()[0] == '\0');
    sprintf(path, "%s/bin/wena-debug-log.txt/wena", dir);
    assert(!wena_debug_log_open(path) && wena_debug_log_path()[0] == '\0');
    puts("debug log tests passed");
    return 0;
}
