#ifndef WENA_SERVER_FERRETDB_COMPAT_H
#define WENA_SERVER_FERRETDB_COMPAT_H
typedef enum WenaFerretFormat { WENA_FERRET_UNKNOWN=0,WENA_FERRET_V1_SJSON_INDEX2=1 } WenaFerretFormat;
struct sqlite3;
/* Uses the caller's existing read transaction; does not change connection state. */
WenaFerretFormat wena_ferretdb_probe_connection(struct sqlite3 *database);
WenaFerretFormat wena_ferretdb_probe_readonly(const char *database_path);
#endif
