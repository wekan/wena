#ifndef WENA_SERVER_SQLITE_RESTORE_H
#define WENA_SERVER_SQLITE_RESTORE_H
#include "sqlite_backup.h"
#include <stddef.h>
typedef int (*WenaRestoreStop)(void *context);
typedef int (*WenaRestoreStart)(void *context,const char *database_path);
typedef struct WenaRestoreLifecycle{WenaRestoreStop stop;WenaRestoreStart start;void *context;}WenaRestoreLifecycle;
int wena_sqlite_restore(const char *backup_path,const char *database_path,
 const char *migration_sha256,WenaBackupFreeSpace free_space,void *space_context,
 const WenaRestoreLifecycle *lifecycle);
#endif
