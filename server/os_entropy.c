#include "os_entropy.h"

#include <stddef.h>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <stdlib.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__) || defined(__ANDROID__)
#include <sys/random.h>
#endif
#endif

int wena_os_entropy(void *context, unsigned char *output, size_t length)
{
    size_t offset;
    (void)context;
    if (output == NULL || length == 0) return 0;
#if defined(_WIN32)
    if (length > 0xffffffffu) return 0;
    return BCryptGenRandom(NULL, output, (ULONG)length,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    arc4random_buf(output, length);
    return 1;
#else
    offset = 0;
#if defined(__linux__) || defined(__ANDROID__)
    while (offset < length) {
        ssize_t received;
        received = getrandom(output + offset, length - offset, 0);
        if (received > 0) offset += (size_t)received;
        else if (received < 0 && errno == EINTR) continue;
        else break;
    }
    if (offset == length) return 1;
#endif
    {
        int file;
        file = open("/dev/urandom", O_RDONLY);
        if (file < 0) return 0;
        while (offset < length) {
            ssize_t received;
            received = read(file, output + offset, length - offset);
            if (received > 0) offset += (size_t)received;
            else if (received < 0 && errno == EINTR) continue;
            else { close(file); return 0; }
        }
        close(file);
    }
    return 1;
#endif
}
