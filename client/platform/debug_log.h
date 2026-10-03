#ifndef WENA_DEBUG_LOG_H
#define WENA_DEBUG_LOG_H

#include <stddef.h>

/* Desktop debug log: one directory per run, YYYY-MM-DD_HH-MM-SS, holding
 * desktop.log. The directory is WENA_LOG_DIR when set (build.sh run sets it),
 * otherwise <...>/.tools/log/wena/<time> when the executable lives under
 * <...>/.tools/wena/, otherwise none and lines go to stderr only.
 * Opening also installs handlers that record a fatal signal before the
 * default action runs, so a crash leaves its signal in the log. */
int wena_debug_log_open(const char *executable);
/* The run's directory, or "" when there is no log file. */
const char *wena_debug_log_directory(void);
/* One timestamped line; printf-style. Never fails the caller. */
void wena_debug_log(const char *format, ...);
void wena_debug_log_close(void);
/* mkdir -p of the directory holding an absolute file path. */
int wena_make_parent_directories(const char *file);

/* Pure path rules, exposed for tests. 0 when the result does not fit or the
 * input gives no directory. */
int wena_debug_log_directory_for(const char *log_dir_env, const char *executable,
                                 const char *stamp, char *out, size_t capacity);
/* The default board file when the desktop starts without arguments:
 * WENA_DATABASE, else the per-user data folder. 0 when it cannot be named. */
int wena_desktop_default_database(const char *database_env, const char *home,
                                  const char *xdg_data_home, int apple,
                                  char *out, size_t capacity);

#endif
