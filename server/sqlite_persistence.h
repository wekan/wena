#ifndef WENA_SERVER_SQLITE_PERSISTENCE_H
#define WENA_SERVER_SQLITE_PERSISTENCE_H
#include "domain_operation.h"
#include <sqlite3.h>
typedef struct WenaSqlitePersistence { sqlite3 *database; } WenaSqlitePersistence;
void wena_sqlite_persistence_init(WenaSqlitePersistence *store, sqlite3 *database);
int wena_sqlite_persistence_apply(void *context, const WenaDomainCommand *command,
                                  WenaRegionResponse *response);
#endif
