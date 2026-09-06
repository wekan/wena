#ifndef WENA_SERVER_ROOT_URL_H
#define WENA_SERVER_ROOT_URL_H

#include "settings.h"

#include <stddef.h>

int wena_root_url_join(const WenaRootUrl *root, const char *route_path,
                       char *output, size_t capacity);

#endif
