#include "dependencies.h"

#include <SDL.h>
#include <sqlite3.h>

int wena_sqlite_wal_reset_fix_known(int version_number)
{
    return version_number >= 3051003 ||
        (version_number >= 3050007 && version_number < 3051000) ||
        (version_number >= 3044006 && version_number < 3045000);
}

static int entry(FILE *output, const char *key, const char *value)
{
    const unsigned char *cursor;
    if (fprintf(output, "%s=", key) < 0) return 0;
    cursor = (const unsigned char *)value;
    while (*cursor) {
        if (*cursor < 32 || *cursor >= 127 || *cursor == '%') {
            if (fprintf(output, "%%%02X", (unsigned int)*cursor) < 0) return 0;
        } else if (fputc(*cursor, output) == EOF) return 0;
        ++cursor;
    }
    return fputc('\n', output) != EOF;
}

int wena_desktop_dependency_report(FILE *output)
{
    SDL_version runtime;
    int version, known;
    char text[64];
    if (output == NULL) return 0;
    SDL_GetVersion(&runtime);
    version = sqlite3_libversion_number();
    known = wena_sqlite_wal_reset_fix_known(version);
    if (!entry(output, "format", "wena-dependencies-v1")) return 0;
    sprintf(text, "%d.%d.%d", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    if (!entry(output, "sdl_header", text)) return 0;
    sprintf(text, "%d.%d.%d", runtime.major, runtime.minor, runtime.patch);
    if (!entry(output, "sdl_runtime", text) ||
        !entry(output, "sdl_backend_minimum", "2.0.22") ||
        !entry(output, "sqlite_header", SQLITE_VERSION) ||
        !entry(output, "sqlite_runtime", sqlite3_libversion()) ||
        !entry(output, "sqlite_source_id", sqlite3_sourceid())) return 0;
    sprintf(text, "%d", version);
    if (!entry(output, "sqlite_runtime_number", text)) return 0;
    sprintf(text, "%d", sqlite3_threadsafe());
    if (!entry(output, "sqlite_threadsafe", text) ||
        !entry(output, "sqlite_wal_reset_known_fixed_upstream_version", known ? "1" : "0") ||
        !entry(output, "sqlite_wal_reset_status", known ?
            "known_fixed_upstream_version" : "unknown_distribution_backport_status") ||
        !entry(output, "sqlite_distribution_backports", "not_inspected") ||
        !entry(output, "scope", "libraries_loaded_by_this_process")) return 0;
    return fflush(output) == 0 && !ferror(output);
}
