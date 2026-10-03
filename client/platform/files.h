#ifndef WENA_PLATFORM_FILES_H
#define WENA_PLATFORM_FILES_H

#include <stddef.h>

/* Paths are UTF-8 everywhere. On Windows they are converted to UTF-16 for the
 * wide-character file APIs, so names outside the ANSI code page work. */

#define WENA_FILE_MISSING 0
#define WENA_FILE_REGULAR 1
#define WENA_FILE_OTHER 2
#define WENA_FILE_ERROR (-1)

/* POSIX: starts with '/'. Windows: a drive ("C:\" or "C:/") or UNC ("\\server"). */
int wena_path_absolute(const char *path);
/* Rules for one platform, for tests: windows != 0 applies the Windows rules. */
int wena_path_absolute_for(const char *path, int windows);
/* What is at a path; a symbolic link counts as what it points to, as stat(). */
int wena_file_kind(const char *path);
/* mkdir -p of the directory holding an absolute file path. */
int wena_make_parent_directories(const char *file);
/* An environment variable as UTF-8; 0 when unset, empty or too long. */
int wena_environment(const char *name, char *out, size_t capacity);
/* The path separator this platform writes. */
char wena_path_separator(void);

#endif
