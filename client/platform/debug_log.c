#if defined(__unix__) || defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "debug_log.h"
#include "files.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#define WENA_WRITE _write
#define WENA_STDERR 2
#else
#include <unistd.h>
#define WENA_WRITE write
#define WENA_STDERR STDERR_FILENO
#endif

#define LOG_PATH_CAPACITY 4096

static FILE *log_file;
static char log_directory[LOG_PATH_CAPACITY];
static int log_fd = -1;

static int append(char *out, size_t capacity, size_t *used, const char *text, size_t length)
{
    if (text == NULL) return 0;
    if (*used + length >= capacity) return 0;
    memcpy(out + *used, text, length);
    *used += length;
    out[*used] = '\0';
    return 1;
}

static int append_text(char *out, size_t capacity, size_t *used, const char *text)
{
    return text != NULL && append(out, capacity, used, text, strlen(text));
}

int wena_debug_log_directory_for(const char *log_dir_env, const char *executable,
                                 const char *stamp, char *out, size_t capacity)
{
    static const char posix_marker[] = "/.tools/wena/", windows_marker[] = "\\.tools\\wena\\";
    const char *found;
    size_t used;
    int windows;
    if (out == NULL || capacity == 0 || stamp == NULL || stamp[0] == '\0') return 0;
    out[0] = '\0';
    used = 0;
    if (log_dir_env != NULL && log_dir_env[0] != '\0')
        return (wena_path_absolute_for(log_dir_env, 0) || wena_path_absolute_for(log_dir_env, 1)
#if defined(__amigaos__) || defined(__AROS__)
                || wena_path_absolute_amiga(log_dir_env)
#endif
               ) && append_text(out, capacity, &used, log_dir_env);
    if (executable == NULL) return 0;
    found = strstr(executable, posix_marker);
    windows = found == NULL;
    if (windows) found = strstr(executable, windows_marker);
    if (found == NULL) return 0;
    /* <prefix><sep>.tools<sep> then log<sep>wena<sep><stamp> */
    return append(out, capacity, &used, executable, (size_t)(found - executable) + strlen("/.tools/")) &&
           append_text(out, capacity, &used, windows ? "log\\wena\\" : "log/wena/") &&
           append_text(out, capacity, &used, stamp);
}

int wena_desktop_default_database(const char *database_env, const char *home,
                                  const char *xdg_data_home, int system,
                                  char *out, size_t capacity)
{
    size_t used;
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    used = 0;
    if (database_env != NULL && database_env[0] != '\0')
        return (system == WENA_SYSTEM_AMIGA ? wena_path_absolute_amiga(database_env) :
                wena_path_absolute_for(database_env, system == WENA_SYSTEM_WINDOWS)) &&
               append_text(out, capacity, &used, database_env);
    /* The drawer Wena was started from, as Amiga programs keep their data:
     * ENV: is a RAM disk and ENVARC: is copied into it at every boot, so
     * neither is a place for a board that grows. */
    if (system == WENA_SYSTEM_AMIGA)
        return append_text(out, capacity, &used, "PROGDIR:wena.sqlite");
    if (system == WENA_SYSTEM_WINDOWS)
        return wena_path_absolute_for(home, 1) &&
               append_text(out, capacity, &used, home) &&
               append_text(out, capacity, &used, "\\Wena\\wena.sqlite");
    if (system == WENA_SYSTEM_MACOS)
        return wena_path_absolute_for(home, 0) &&
               append_text(out, capacity, &used, home) &&
               append_text(out, capacity, &used, "/Library/Application Support/Wena/wena.sqlite");
    if (wena_path_absolute_for(xdg_data_home, 0))
        return append_text(out, capacity, &used, xdg_data_home) &&
               append_text(out, capacity, &used, "/wena/wena.sqlite");
    return wena_path_absolute_for(home, 0) &&
           append_text(out, capacity, &used, home) &&
           append_text(out, capacity, &used, "/.local/share/wena/wena.sqlite");
}

static void write_text(const char *text)
{
    unsigned int length = (unsigned int)strlen(text);
    if (log_fd >= 0 && WENA_WRITE(log_fd, text, length) < 0) { /* nothing more can be done */ }
    if (WENA_WRITE(WENA_STDERR, text, length) < 0) { /* nothing more can be done */ }
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

static FILE *open_append(const char *path)
{
#if defined(_WIN32)
    wchar_t name[LOG_PATH_CAPACITY + 16];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, name, LOG_PATH_CAPACITY + 16) <= 0)
        return NULL;
    return _wfopen(name, L"a");
#else
    return fopen(path, "a");
#endif
}

int wena_debug_log_open(const char *executable)
{
    char stamp[32], environment[LOG_PATH_CAPACITY], path[LOG_PATH_CAPACITY + 16];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    size_t used = 0;
    log_directory[0] = '\0';
    if (local == NULL || strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", local) == 0) return 0;
    if (!wena_debug_log_directory_for(wena_environment("WENA_LOG_DIR", environment, sizeof(environment)) ?
                                      environment : NULL, executable, stamp,
                                      log_directory, sizeof(log_directory))) {
        log_directory[0] = '\0';
        return 0;
    }
    path[0] = '\0';
    if (!append_text(path, sizeof(path), &used, log_directory) ||
        !append(path, sizeof(path), &used, "/", 1) ||
        !append_text(path, sizeof(path), &used, "desktop.log") ||
        !wena_make_parent_directories(path) ||
        (log_file = open_append(path)) == NULL) {
        log_directory[0] = '\0';
        return 0;
    }
    setvbuf(log_file, NULL, _IOLBF, 0);
#if defined(_WIN32)
    log_fd = _fileno(log_file);
#else
    log_fd = fileno(log_file);
#endif
    signal(SIGSEGV, fatal_signal);
    signal(SIGABRT, fatal_signal);
    signal(SIGFPE, fatal_signal);
    signal(SIGILL, fatal_signal);
#ifdef SIGBUS
    signal(SIGBUS, fatal_signal);
#endif
    return 1;
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
    log_fd = -1;
}
