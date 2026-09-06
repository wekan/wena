#ifndef WENA_SERVER_RUNTIME_H
#define WENA_SERVER_RUNTIME_H
#include "http_listener.h"
#include "sqlite_persistence.h"
#include "sqlite_storage.h"
#include "embedded_migration.h"
typedef struct WenaServerRuntime{sqlite3 *database;WenaSqlitePersistence persistence;WenaDomainAdapter domain;WenaHttpListener listener;int running;}WenaServerRuntime;
void wena_server_runtime_init(WenaServerRuntime *runtime);
int wena_server_runtime_start(WenaServerRuntime *runtime,WenaServerSettings *settings,const char *database_path,const unsigned char *migration,size_t migration_length,const char *migration_sha256,WenaSecurityStore *security,WenaHttpNow now,void *now_context);
void wena_server_runtime_stop(WenaServerRuntime *runtime,WenaServerSettings *settings);
int wena_server_runtime_start_executable(WenaServerRuntime *runtime,WenaServerSettings *settings,
 const char *database_path,const char *executable_path,WenaSecurityStore *security,
 WenaHttpNow now,void *now_context);
#endif
