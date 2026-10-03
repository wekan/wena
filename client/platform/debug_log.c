#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "debug_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define WENA_DEBUG_LOG_POSIX 1
#endif

#define LOG_PATH_CAPACITY 4096

static FILE *log_file;
static char log_directory[LOG_PATH_CAPACITY];
#ifdef WENA_DEBUG_LOG_POSIX
static int log_fd = -1;
#endif

static int append(char *out, size_t capacity, size_t *used, const char *text, size_t length)
{
    if (text == NULL) return 0;
    if (*used + length >= capacity) return 0;
    memcpy(out + *used, text, length);
    *used += length;
    out[*used] = '\0';
    return 1;
}

int wena_debug_log_directory_for(const char *log_dir_env, const char *executable,
                                 const char *stamp, char *out, size_t capacity)
{
    static const char marker[] = "/.tools/wena/";
    const char *found;
    size_t used;
    if (out == NULL || capacity == 0 || stamp == NULL || stamp[0] == '\0') return 0;
    out[0] = '\0';
    used = 0;
    if (log_dir_env != NULL && log_dir_env[0] == '/')
        return append(out, capacity, &used, log_dir_env, strlen(log_dir_env));
    if (executable == NULL || (found = strstr(executable, marker)) == NULL) return 0;
    /* <prefix>/.tools/ then log/wena/<stamp> */
    return append(out, capacity, &used, executable, (size_t)(found - executable) + strlen("/.tools/")) &&
           append(out, capacity, &used, "log/wena/", strlen("log/wena/")) &&
           append(out, capacity, &used, stamp, strlen(stamp));
}

int wena_desktop_default_database(const char *database_env, const char *home,
                                  const char *xdg_data_home, int apple,
                                  char *out, size_t capacity)
{
    size_t used;
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    used = 0;
    if (database_env != NULL && database_env[0] != '\0')
        return database_env[0] == '/' && append(out, capacity, &used, database_env, strlen(database_env));
    if (apple)
        return home != NULL && home[0] == '/' &&
               append(out, capacity, &used, home, strlen(home)) &&
               append(out, capacity, &used, "/Library/Application Support/Wena/wena.sqlite", strlen("/Library/Application Support/Wena/wena.sqlite"));
    if (xdg_data_home != NULL && xdg_data_home[0] == '/')
        return append(out, capacity, &used, xdg_data_home, strlen(xdg_data_home)) &&
               append(out, capacity, &used, "/wena/wena.sqlite", strlen("/wena/wena.sqlite"));
    return home != NULL && home[0] == '/' &&
           append(out, capacity, &used, home, strlen(home)) &&
           append(out, capacity, &used, "/.local/share/wena/wena.sqlite", strlen("/.local/share/wena/wena.sqlite"));
}

#ifdef WENA_DEBUG_LOG_POSIX
/* mkdir -p for an absolute path. */
static int make_directories(const char *path);
#endif

int wena_make_parent_directories(const char *file)
{
#ifdef WENA_DEBUG_LOG_POSIX
    char parent[LOG_PATH_CAPACITY];
    const char *slash = file == NULL ? NULL : strrchr(file, '/');
    if (slash == NULL || file[0] != '/' || (size_t)(slash - file) >= sizeof(parent)) return 0;
    if (slash == file) return 1;
    memcpy(parent, file, (size_t)(slash - file));
    parent[slash - file] = '\0';
    return make_directories(parent);
#else
    (void)file;
    return 0;
#endif
}

#ifdef WENA_DEBUG_LOG_POSIX
static int make_directories(const char *path)
{
    char partial[LOG_PATH_CAPACITY];
    size_t i, length = strlen(path);
    if (length == 0 || length >= sizeof(partial)) return 0;
    memcpy(partial, path, length + 1);
    for (i = 1; i <= length; ++i) {
        if (partial[i] == '/' || partial[i] == '\0') {
            char saved = partial[i];
            partial[i] = '\0';
            if (mkdir(partial, 0700) != 0 && errno != EEXIST) return 0;
            partial[i] = saved;
        }
    }
    return 1;
}

static void write_text(const char *text)
{
    size_t length = strlen(text);
    if (log_fd >= 0 && write(log_fd, text, length) < 0) { /* nothing more can be done */ }
    if (write(STDERR_FILENO, text, length) < 0) { /* nothing more can be done */ }
}

/* Async-signal-safe: only write() and fixed text, then the default action. */
static void fatal_signal(int number)
{
    char line[64] = "CRASH: fatal signal ";
    char digits[8];
    int i = 0, n = number, j;
    do { digits[i++] = (char)('0' + n % 10); n /= 10; } while (n > 0 && i < 7);
    j = (int)strlen(line);
    while (i > 0) line[j++] = digits[--i];
    line[j++] = '\n'; line[j] = '\0';
    write_text(line);
    signal(number, SIG_DFL);
    raise(number);
}
#endif

int wena_debug_log_open(const char *executable)
{
    char stamp[32], path[LOG_PATH_CAPACITY + 16];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    log_directory[0] = '\0';
    if (local == NULL || strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", local) == 0) return 0;
#ifdef WENA_DEBUG_LOG_POSIX
    if (!wena_debug_log_directory_for(getenv("WENA_LOG_DIR"), executable, stamp,
                                      log_directory, sizeof(log_directory)) ||
        !make_directories(log_directory)) {
        log_directory[0] = '\0';
        return 0;
    }
    sprintf(path, "%s/desktop.log", log_directory);
    log_file = fopen(path, "a");
    if (log_file == NULL) { log_directory[0] = '\0'; return 0; }
    setvbuf(log_file, NULL, _IOLBF, 0);
    log_fd = fileno(log_file);
    signal(SIGSEGV, fatal_signal);
    signal(SIGBUS, fatal_signal);
    signal(SIGABRT, fatal_signal);
    signal(SIGFPE, fatal_signal);
    signal(SIGILL, fatal_signal);
    return 1;
#else
    (void)executable; (void)path;
    return 0;
#endif
}

const char *wena_debug_log_directory(void)
{
    return log_directory;
}

void wena_debug_log(const char *format, ...)
{
    char stamp[32];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    va_list arguments;
    if (log_file == NULL) return;
    if (local == NULL || strftime(stamp, sizeof(stamp), "%H:%M:%S", local) == 0) strcpy(stamp, "--:--:--");
    fprintf(log_file, "%s ", stamp);
    va_start(arguments, format);
    vfprintf(log_file, format, arguments);
    va_end(arguments);
    fputc('\n', log_file);
    fflush(log_file);
}

void wena_debug_log_close(void)
{
    if (log_file != NULL) fclose(log_file);
    log_file = NULL;
#ifdef WENA_DEBUG_LOG_POSIX
    log_fd = -1;
#endif
}
