#ifndef WENA_SQLITE_AMIGA_IO_H
#define WENA_SQLITE_AMIGA_IO_H
/* Forced into the SQLite amalgamation of the Amiga builds (-include, with
 * -DUSE_PREAD -Dpread=wena_sqlite_pread -Dpwrite=wena_sqlite_pwrite) so the
 * unix VFS reads and writes at an offset through these, which never seek
 * past the end of a file. Defined in server/sqlite_amiga_vfs.c. */
#include <sys/types.h>

ssize_t wena_sqlite_pread(int descriptor, void *buffer, size_t count, off_t offset);
ssize_t wena_sqlite_pwrite(int descriptor, const void *buffer, size_t count, off_t offset);

#endif
