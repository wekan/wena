#ifndef WENA_SERVER_SQLITE_BACKUP_H
#define WENA_SERVER_SQLITE_BACKUP_H

#include <sqlite3.h>

typedef int (*WenaBackupFreeSpace)(void *context, const char *path,
                                   unsigned long *bytes);

int wena_sqlite_backup_create(sqlite3 *source, const char *path,
                              WenaBackupFreeSpace free_space, void *context);
int wena_sqlite_backup_posix_free_space(void *context, const char *path,
                                        unsigned long *bytes);

#endif
