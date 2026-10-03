/* AmigaOS 3.x, AmigaOS 4 and AROS: SQLite's unix VFS without file locks and
 * with AmigaDOS file names. Linked into the SQLite object of those builds only
 * (scripts/build_desktop_amiga_container.sh), which compiles the amalgamation
 * with -DSQLITE_EXTRA_INIT=wena_sqlite_amiga_init so this runs inside
 * sqlite3_initialize(), before any database is opened. */
#include <sqlite3.h>
#include <string.h>
#include <sys/types.h>

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
