#include "../client/platform/dependencies.h"
#include <SDL.h>
#include <sqlite3.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void require_value(const char *report, const char *key, const char *value)
{
    char expected[1024];
    assert(strlen(key) + strlen(value) + 3 < sizeof(expected));
    sprintf(expected, "%s=%s\n", key, value);
    assert(strstr(report, expected) != NULL);
}
int main(void)
{
    static const int fixed[] = {3044006, 3044099, 3050007, 3050099, 3051003, 3053004};
    static const int unknown[] = {-1, 0, 3044005, 3045000, 3045001, 3050006, 3051000, 3051002};
    size_t i, size;
    char report[4096], expected[64];
    FILE *file;
    SDL_version runtime;
    for (i = 0; i < sizeof(fixed) / sizeof(fixed[0]); ++i)
        assert(wena_sqlite_wal_reset_fix_known(fixed[i]));
    for (i = 0; i < sizeof(unknown) / sizeof(unknown[0]); ++i)
        assert(!wena_sqlite_wal_reset_fix_known(unknown[i]));
    assert(!wena_desktop_dependency_report(NULL));
    assert(SDL_WasInit(0) == 0);
    file = tmpfile(); assert(file != NULL);
    assert(wena_desktop_dependency_report(file));
    assert(SDL_WasInit(0) == 0);
    rewind(file); size = fread(report, 1, sizeof(report)-1, file); report[size] = 0;
    assert(!ferror(file)); fclose(file);
    require_value(report, "format", "wena-dependencies-v1");
    require_value(report, "sqlite_header", SQLITE_VERSION);
    require_value(report, "sqlite_runtime", sqlite3_libversion());
    require_value(report, "sqlite_source_id", sqlite3_sourceid());
    require_value(report, "sqlite_distribution_backports", "not_inspected");
    require_value(report, "scope", "libraries_loaded_by_this_process");
    sprintf(expected, "%d", sqlite3_libversion_number());
    require_value(report, "sqlite_runtime_number", expected);
    require_value(report, "sqlite_wal_reset_status",
        wena_sqlite_wal_reset_fix_known(sqlite3_libversion_number()) ?
        "known_fixed_upstream_version" : "unknown_distribution_backport_status");
    SDL_GetVersion(&runtime);
    sprintf(expected, "%d.%d.%d", runtime.major, runtime.minor, runtime.patch);
    require_value(report, "sdl_runtime", expected);
    /* /dev/full provides a deterministic write-error sink on Unix hosts. */
    file = fopen("/dev/full", "wb");
    if (file != NULL) {
        assert(!wena_desktop_dependency_report(file));
        fclose(file);
    }
    puts("process dependency report and upstream fix boundaries passed");
    return 0;
}
