#ifndef WENA_SERVER_SHA256_H
#define WENA_SERVER_SHA256_H

#include <stddef.h>
typedef unsigned long WenaShaU32;
typedef struct WenaSha256{WenaShaU32 h[8];unsigned char block[64];size_t used;WenaShaU32 hi,lo;}WenaSha256;
void wena_sha256_init(WenaSha256 *state);
void wena_sha256_update(WenaSha256 *state,const unsigned char *data,size_t length);
void wena_sha256_final_hex(WenaSha256 *state,char output[65]);

void wena_sha256_hex(const unsigned char *data, size_t length, char output[65]);

#endif
