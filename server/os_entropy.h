#ifndef WENA_SERVER_OS_ENTROPY_H
#define WENA_SERVER_OS_ENTROPY_H

#include <stddef.h>

int wena_os_entropy(void *context, unsigned char *output, size_t length);

#endif
