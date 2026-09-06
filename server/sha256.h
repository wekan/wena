#ifndef WENA_SERVER_SHA256_H
#define WENA_SERVER_SHA256_H

#include <stddef.h>

void wena_sha256_hex(const unsigned char *data, size_t length, char output[65]);

#endif
