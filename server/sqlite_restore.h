#ifndef WENA_SERVER_SQLITE_RESTORE_H
#define WENA_SERVER_SQLITE_RESTORE_H
#include "sqlite_backup.h"
#include <stddef.h>
typedef int (*WenaRestoreStop)(void *context);
typedef int (*WenaRestoreStart)(void *context,const char *database_path);
typedef struct WenaRestoreLifecycle{WenaRestoreStop stop;WenaRestoreStart start;void *context;}WenaRestoreLifecycle;
/* migration_sha256 identifies a compiled bundle target. A validated older
 * backup is upgraded in a verified private copy before lifecycle.stop; the
 * source backup is unchanged. Unknown targets and downgrades are refused. */
int wena_sqlite_restore(const char *backup_path,const char *database_path,
 const char *migration_sha256,WenaBackupFreeSpace free_space,void *space_context,
 const WenaRestoreLifecycle *lifecycle);
#endif
