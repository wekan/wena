#ifndef WENA_TEST_FILES_H
#define WENA_TEST_FILES_H
#include <stddef.h>

/* Files a native test reads - schemas and fixture SQL - compiled into the test
 * by scripts/embed_test_files.py (the test script lists them). A path argument
 * only selects one by its last component; the bytes always come from the
 * program, so a test never executes SQL read from outside it. */
typedef struct WenaTestFile {
    const char *name;
    const unsigned char *data;
    size_t size;
} WenaTestFile;

/* The embedded file named by `path`'s last component, NUL-terminated; aborts
 * the test when it was not compiled in. `size` may be NULL. */
const char *wena_test_file(const char *path, size_t *size);
/* The same as a malloc'd copy, for code that frees what it read. */
char *wena_test_file_copy(const char *path, long *size);

#endif
