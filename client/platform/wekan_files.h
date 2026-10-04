#ifndef WENA_PLATFORM_WEKAN_FILES_H
#define WENA_PLATFORM_WEKAN_FILES_H

#include <stddef.h>

/* WeKan's files directory, as WeKan's FerretDB bundles lay it out, so Wena
 * opens and writes the same data a WeKan AppImage does:
 *
 *   wekan-files/attachments
 *   wekan-files/avatars
 *   wekan-files/db/wekan.sqlite      FerretDB's SQLite file of database "wekan"
 *
 * The root is WRITABLE_PATH when it is set - as-is when it already ends in
 * "files" or "wekan-files" (releases/ferretdb/start-wekan.sh and .bat), with
 * "files" added otherwise - and else "wekan-files" beside the executable, as
 * WeKan's Windows single executable has it. `system` is a WENA_SYSTEM_* of
 * debug_log.h; Amiga and AROS use "PROGDIR:wekan-files", phones (whose
 * program folder is read-only) the app's data folder in `home`. */
int wena_wekan_files_root(const char *writable_path, const char *executable, const char *home,
                          int system, char *out, size_t capacity);
/* root + "db" + "wekan.sqlite" with the platform's separator. */
int wena_wekan_files_database(const char *root, int system, char *out, size_t capacity);
/* Creates the root and its attachments, avatars and db folders. */
int wena_wekan_files_prepare(const char *root, int system);

/* A database file that is no database: not empty, smaller than the 512
 * bytes of the smallest SQLite file, and without SQLite's header - what
 * libnix's lseek() made of a new wekan.sqlite on AmigaOS before Wena's
 * SQLite stopped seeking past the end. It is renamed, never removed, to
 * "<database>.not-a-database" (then .not-a-database-2 ... -9), and the
 * new name goes in `moved`. 1 when it was set aside, 0 when there was
 * nothing to do (missing, empty, a database, or too large to be this),
 * -1 when it could not be renamed. */
int wena_wekan_files_set_aside_broken(const char *database, char *moved, size_t capacity);

#endif
