#ifndef WENA_SERVER_EMBEDDED_MIGRATION_H
#define WENA_SERVER_EMBEDDED_MIGRATION_H
#include <stddef.h>
typedef struct WenaEmbeddedMigration{unsigned char *bytes;size_t length;char sha256[65];}WenaEmbeddedMigration;
int wena_embedded_migration_load(const char *executable,WenaEmbeddedMigration *migration);
void wena_embedded_migration_free(WenaEmbeddedMigration *migration);
#endif
