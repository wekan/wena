/* AmigaOS 3.x, AmigaOS 4 and AROS: SQLite's unix VFS without file locks and
 * with AmigaDOS file names. Linked into the SQLite object of those builds only
 * (scripts/build_desktop_amiga_container.sh), which compiles the amalgamation
 * with -DSQLITE_EXTRA_INIT=wena_sqlite_amiga_init so this runs inside
 * sqlite3_initialize(), before any database is opened. */
#include <sqlite3.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "sqlite_amiga_io.h"

static sqlite3_vfs amiga_vfs;

/* "Work:Wena/wena.sqlite" and "PROGDIR:wena.sqlite" are already complete:
 * the unix VFS would put getcwd() and a '/' in front of anything that does
 * not start with '/', and on AmigaDOS a leading '/' means the parent. A
 * relative name is left for AmigaDOS to resolve against the current drawer. */
static int amiga_full_pathname(sqlite3_vfs *vfs, const char *name, int capacity, char *out)
{
    size_t length;
    (void)vfs;
    if (name == NULL || out == NULL || capacity <= 0) return SQLITE_CANTOPEN;
    length = strlen(name);
    if (length >= (size_t)capacity) return SQLITE_CANTOPEN;
    memcpy(out, name, length + 1);
    return SQLITE_OK;
}

/* The amalgamation is compiled with -Dfchmod=wena_sqlite_fchmod and
 * -Dfchown=wena_sqlite_fchown: libnix declares both and has neither, and on
 * every one of these systems a Unix owner or mode means nothing to AmigaDOS.
 * SQLite uses them only to give a journal its database's owner and mode. */
int wena_sqlite_fchmod(int descriptor, mode_t mode)
{
    (void)descriptor; (void)mode;
    return 0;
}

int wena_sqlite_fchown(int descriptor, uid_t owner, gid_t group)
{
    (void)descriptor; (void)owner; (void)group;
    return 0;
}

/* Reading and writing at an offset. These C libraries have no pread() or
 * pwrite(), so the unix VFS would lseek() and then read() or write(). But
 * AmigaDOS cannot Seek() past the end of a file, and libnix's lseek()
 * makes up for it by writing the gap - from whatever is in its buffer. A
 * new, empty wekan.sqlite was read at offset 24 by SQLite's first look at
 * its header, and became 24 bytes of stray memory ("file is not a
 * database"), on every start after it too.
 *
 * So: a read at or past the end is the end of the file, as pread() is; a
 * write past the end first fills the gap with zeros, from the end, as a
 * POSIX file would read back. lseek() is only ever asked for the end or an
 * offset inside the file. */
ssize_t wena_sqlite_pread(int descriptor, void *buffer, size_t count, off_t offset)
{
    off_t end = lseek(descriptor, 0, SEEK_END);
    if (end < 0 || offset < 0) return -1;
    if (offset >= end) return 0;
    if (lseek(descriptor, offset, SEEK_SET) != offset) return -1;
    return read(descriptor, buffer, count);
}

ssize_t wena_sqlite_pwrite(int descriptor, const void *buffer, size_t count, off_t offset)
{
    static const char zeros[512];
    off_t end = lseek(descriptor, 0, SEEK_END);
    if (end < 0 || offset < 0) return -1;
    while (end < offset) {
        size_t part = offset - end > (off_t)sizeof(zeros) ? sizeof(zeros) : (size_t)(offset - end);
        ssize_t written = write(descriptor, zeros, part);
        if (written <= 0) return -1;
        end += written;
    }
    if (end != offset && lseek(descriptor, offset, SEEK_SET) != offset) return -1;
    return write(descriptor, buffer, count);
}

/* "unix-none": these systems have no fcntl() byte-range locks, and one
 * desktop process owns its board file. The copy becomes the default VFS. */
int wena_sqlite_amiga_init(const char *unused)
{
    sqlite3_vfs *base;
    (void)unused;
    if (amiga_vfs.zName != NULL) return SQLITE_OK;
    base = sqlite3_vfs_find("unix-none");
    if (base == NULL) return SQLITE_ERROR;
    amiga_vfs = *base;
    amiga_vfs.pNext = NULL;
    amiga_vfs.zName = "amiga";
    amiga_vfs.xFullPathname = amiga_full_pathname;
    return sqlite3_vfs_register(&amiga_vfs, 1);
}
