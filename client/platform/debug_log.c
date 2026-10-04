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
#if defined(__ANDROID__)
/* liblog's logcat writer. <android/log.h> needs C99 (static inline), so its
 * one function used here is declared from the NDK's stable ABI instead. */
int __android_log_vprint(int priority, const char *tag, const char *format, va_list arguments);
#define WENA_ANDROID_LOG_INFO 4
#endif

#define LOG_PATH_CAPACITY 4096

static FILE *log_file;
static char log_directory[LOG_PATH_CAPACITY];
static char log_path[LOG_PATH_CAPACITY + 32];
static int log_fd = -1;
static int append(char *out, size_t capacity, size_t *used, const char *text, size_t length);
static int append_text(char *out, size_t capacity, size_t *used, const char *text);
static char recent[WENA_DEBUG_LOG_RECENT][WENA_DEBUG_LOG_RECENT_WIDTH];
static size_t recent_next, recent_count;

/* The bounded formatter behind wena_debug_log_format. */
typedef struct Formatted { char *out; size_t capacity, used; } Formatted;

static void put_char(Formatted *f, char c)
{
    if (f->used + 1 < f->capacity) f->out[f->used++] = c;
}

static void put_text(Formatted *f, const char *text)
{
    if (text == NULL) text = "(null)";
    while (*text != '\0') put_char(f, *text++);
}

static void put_unsigned(Formatted *f, unsigned long value, unsigned base)
{
    char digits[32];
    int count = 0;
    do { digits[count++] = "0123456789abcdef"[value % base]; value /= base; } while (value != 0 && count < 31);
    while (count > 0) put_char(f, digits[--count]);
}

static void put_signed(Formatted *f, long value)
{
    if (value < 0) { put_char(f, '-'); put_unsigned(f, 0UL - (unsigned long)value, 10); }
    else put_unsigned(f, (unsigned long)value, 10);
}

static void put_double(Formatted *f, double value, int precision)
{
    double scale = 1.0, whole;
    int i;
    if (value != value) { put_text(f, "nan"); return; }
    if (value < 0.0) { put_char(f, '-'); value = -value; }
    if (value > 4.0e9) { put_text(f, "big"); return; }
    if (precision > 9) precision = 9;
    for (i = 0; i < precision; ++i) scale *= 10.0;
    value = (double)(unsigned long)(value * scale + 0.5) / scale;
    whole = (double)(unsigned long)value;
    put_unsigned(f, (unsigned long)whole, 10);
    if (precision <= 0) return;
    put_char(f, '.');
    value -= whole;
    for (i = 0; i < precision; ++i) {
        value *= 10.0;
        put_char(f, (char)('0' + (int)value % 10));
        value -= (double)(int)value;
    }
}

static void format_into(char *out, size_t capacity, const char *format, va_list arguments)
{
    Formatted f;
    if (out == NULL || capacity == 0) return;
    f.out = out; f.capacity = capacity; f.used = 0;
    while (format != NULL && *format != '\0') {
        int is_long = 0, precision = 6;
        if (*format != '%') { put_char(&f, *format++); continue; }
        ++format;
        if (*format == '.') {
            precision = 0;
            for (++format; *format >= '0' && *format <= '9'; ++format) precision = precision * 10 + (*format - '0');
        }
        if (*format == 'l') { is_long = 1; ++format; }
        switch (*format) {
        case 's': put_text(&f, va_arg(arguments, const char *)); break;
        case 'c': put_char(&f, (char)va_arg(arguments, int)); break;
        case 'd': case 'i':
            put_signed(&f, is_long ? va_arg(arguments, long) : (long)va_arg(arguments, int)); break;
        case 'u':
            put_unsigned(&f, is_long ? va_arg(arguments, unsigned long) : (unsigned long)va_arg(arguments, unsigned), 10);
            break;
        case 'x':
            put_unsigned(&f, is_long ? va_arg(arguments, unsigned long) : (unsigned long)va_arg(arguments, unsigned), 16);
            break;
        case 'f': put_double(&f, va_arg(arguments, double), precision); break;
        case 'p': {
            void *pointer = va_arg(arguments, void *);
            unsigned long bits = 0;
            memcpy(&bits, &pointer, sizeof(pointer) < sizeof(bits) ? sizeof(pointer) : sizeof(bits));
            put_text(&f, "0x");
            put_unsigned(&f, bits, 16);
            break;
        }
        case '%': put_char(&f, '%'); break;
        case '\0': --format; break;
        default: put_char(&f, '%'); put_char(&f, *format); break;
        }
        ++format;
    }
    out[f.used] = '\0';
}

void wena_debug_log_format(char *out, size_t capacity, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    format_into(out, capacity, format, arguments);
    va_end(arguments);
}

size_t wena_debug_log_recent_count(void)
{
    return recent_count;
}

const char *wena_debug_log_recent_line(size_t index)
{
    if (index >= recent_count) return "";
    return recent[(recent_next + WENA_DEBUG_LOG_RECENT - recent_count + index) % WENA_DEBUG_LOG_RECENT];
}

int wena_debug_log_beside_for(int system, const char *executable, char *out, size_t capacity)
{
    static const char name[] = "wena-debug-log.txt";
    size_t used = 0, folder;
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    if (system == WENA_SYSTEM_AMIGA)
        return append_text(out, capacity, &used, "PROGDIR:") && append_text(out, capacity, &used, name);
    if (executable == NULL) return 0;
    /* Up to and including the last separator; Windows takes either. */
    for (folder = strlen(executable); folder > 0; --folder) {
        char c = executable[folder - 1];
        if (c == '/' || (system == WENA_SYSTEM_WINDOWS && c == '\\')) break;
    }
    if (folder == 0 || !append(out, capacity, &used, executable, folder) ||
        !append_text(out, capacity, &used, name)) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

int wena_debug_log_file_for(const char *directory, char *out, size_t capacity)
{
    size_t used = 0, length;
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    if (directory == NULL || (length = strlen(directory)) == 0) return 0;
    return append_text(out, capacity, &used, directory) &&
           (directory[length - 1] == '/' || directory[length - 1] == ':' ||
            directory[length - 1] == '\\' || append(out, capacity, &used, "/", 1)) &&
           append_text(out, capacity, &used, "desktop.log");
}

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

int wena_debug_log_data_directory_for(const char *log_dir_env, const char *data_directory,
                                      char *out, size_t capacity)
{
    size_t used;
    if (out == NULL || capacity == 0) return 0;
    out[0] = '\0';
    used = 0;
    if (log_dir_env != NULL && log_dir_env[0] != '\0')
        return wena_path_absolute_for(log_dir_env, 0) && append_text(out, capacity, &used, log_dir_env);
    return wena_path_absolute_for(data_directory, 0) &&
           append_text(out, capacity, &used, data_directory) &&
           (data_directory[strlen(data_directory) - 1] == '/' ||
            append(out, capacity, &used, "/", 1)) &&
           append_text(out, capacity, &used, "log");
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
    if (system == WENA_SYSTEM_MOBILE)
        return wena_path_absolute_for(home, 0) &&
               append_text(out, capacity, &used, home) &&
               (home[strlen(home) - 1] == '/' || append(out, capacity, &used, "/", 1)) &&
               append_text(out, capacity, &used, "wena.sqlite");
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

static FILE *open_log(const char *path, int truncate)
{
#if defined(_WIN32)
    wchar_t name[LOG_PATH_CAPACITY + 16];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, name, LOG_PATH_CAPACITY + 16) <= 0)
        return NULL;
    return _wfopen(name, truncate ? L"w" : L"a");
#else
    return fopen(path, truncate ? "w" : "a");
#endif
}

/* The log at log_path; the folders above it are made first. */
static int open_file(int truncate, int handlers)
{
    if (!wena_make_parent_directories(log_path) ||
        (log_file = open_log(log_path, truncate)) == NULL) {
        log_directory[0] = '\0';
        log_path[0] = '\0';
        return 0;
    }
    setvbuf(log_file, NULL, _IOLBF, 0);
#if defined(_WIN32)
    log_fd = _fileno(log_file);
#else
    log_fd = fileno(log_file);
#endif
    if (!handlers) return 1;
    signal(SIGSEGV, fatal_signal);
    signal(SIGABRT, fatal_signal);
    signal(SIGFPE, fatal_signal);
    signal(SIGILL, fatal_signal);
#ifdef SIGBUS
    signal(SIGBUS, fatal_signal);
#endif
    return 1;
}

/* desktop.log in log_directory, which the caller has named. */
static int open_in_directory(int truncate, int handlers)
{
    if (!wena_debug_log_file_for(log_directory, log_path, sizeof(log_path))) {
        log_directory[0] = '\0';
        log_path[0] = '\0';
        return 0;
    }
    return open_file(truncate, handlers);
}

int wena_debug_log_open(const char *executable)
{
    char stamp[32], environment[LOG_PATH_CAPACITY];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    log_directory[0] = '\0';
    if (local == NULL || strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", local) == 0) return 0;
    if (!wena_debug_log_directory_for(wena_environment("WENA_LOG_DIR", environment, sizeof(environment)) ?
                                      environment : NULL, executable, stamp,
                                      log_directory, sizeof(log_directory))) {
        /* Outside a checkout: wena-debug-log.txt beside the program, the
         * last run only, so it does not grow. */
        size_t folder;
        log_directory[0] = '\0';
        if (!wena_debug_log_beside_for(
#if defined(__amigaos__) || defined(__AROS__)
                WENA_SYSTEM_AMIGA,
#elif defined(_WIN32)
                WENA_SYSTEM_WINDOWS,
#else
                WENA_SYSTEM_OTHER,
#endif
                executable, log_path, sizeof(log_path)))
            return 0;
        folder = strlen(log_path) - strlen("wena-debug-log.txt");
        if (folder < sizeof(log_directory)) {
            memcpy(log_directory, log_path, folder);
            log_directory[folder] = '\0';
        }
        return open_file(1, 1);
    }
    return open_in_directory(0, 1);
}

int wena_debug_log_open_data(const char *data_directory)
{
    char environment[LOG_PATH_CAPACITY];
    log_directory[0] = '\0';
    if (!wena_debug_log_data_directory_for(wena_environment("WENA_LOG_DIR", environment, sizeof(environment)) ?
                                           environment : NULL, data_directory,
                                           log_directory, sizeof(log_directory))) {
        log_directory[0] = '\0';
        return 0;
    }
#if defined(__ANDROID__)
    return open_in_directory(1, 0);
#else
    return open_in_directory(1, 1);
#endif
}

const char *wena_debug_log_directory(void)
{
    return log_directory;
}

const char *wena_debug_log_path(void)
{
    return log_path;
}

void wena_debug_log(const char *format, ...)
{
    char stamp[32];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    va_list arguments;
#if defined(__ANDROID__)
    /* Android discards stdout and stderr: every line also goes to logcat. */
    va_start(arguments, format);
    __android_log_vprint(WENA_ANDROID_LOG_INFO, "Wena", format, arguments);
    va_end(arguments);
#endif
    va_start(arguments, format);
    format_into(recent[recent_next], sizeof(recent[recent_next]), format, arguments);
    va_end(arguments);
    recent_next = (recent_next + 1) % WENA_DEBUG_LOG_RECENT;
    if (recent_count < WENA_DEBUG_LOG_RECENT) ++recent_count;
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
