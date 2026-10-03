/* The desktop's migration bundle comes from the compiled registry: every
 * migration in order, matching the lock's newest bundle size and SHA-256. */
#include "../server/sqlite_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    unsigned char *bytes;
    size_t length;
    char sha256[65];
    unsigned long expected_length;
    if (argc != 3) return 2;
    expected_length = strtoul(argv[1], NULL, 10);
    if (!wena_sqlite_compiled_bundle(&bytes, &length, sha256)) {
        fputs("no compiled bundle\n", stderr); return 1;
    }
    if (length != expected_length || strcmp(sha256, argv[2]) != 0 ||
        wena_sqlite_migration_target(sha256) < 1 ||
        memcmp(bytes, "CREATE TABLE schema_migrations", 30) != 0) {
        fprintf(stderr, "bundle %lu %s, expected %lu %s\n",
                (unsigned long)length, sha256, expected_length, argv[2]);
        free(bytes); return 1;
    }
    free(bytes);
    /* Negative: NULL arguments are refused, not dereferenced. */
    if (wena_sqlite_compiled_bundle(NULL, &length, sha256) ||
        wena_sqlite_compiled_bundle(&bytes, NULL, sha256) ||
        wena_sqlite_compiled_bundle(&bytes, &length, NULL)) {
        fputs("NULL accepted\n", stderr); return 1;
    }
    puts("compiled bundle matches the lock");
    return 0;
}
