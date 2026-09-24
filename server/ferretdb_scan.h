#ifndef WENA_SERVER_FERRETDB_SCAN_H
#define WENA_SERVER_FERRETDB_SCAN_H
#include <stddef.h>
/* Validate every mapped collection in one read-only snapshot. A caller-supplied
 * document limit bounds work; exceeding it fails, never reports partial success.
 * On any failure *documents is unchanged. No write-compatibility guarantee. */
int wena_ferretdb_scan_readonly(const char *path,size_t limit,size_t *documents);
#endif
