#ifndef WENA_MODELS_IMAGE_DECODE_H
#define WENA_MODELS_IMAGE_DECODE_H
/* The images WeKan lets a board admin upload for the Map view (image/png,
 * image/jpeg, image/gif - mapView.jade's accept list but WebP), decoded to
 * 8-bit RGBA rows, top to bottom:
 *   PNG   every color type and bit depth, palette transparency, Adam7
 *   JPEG  baseline and extended sequential Huffman (SOF0/SOF1), grayscale or
 *         YCbCr with any sampling; progressive JPEG is refused
 *   GIF   the first frame, with its transparent color
 * Images larger than WENA_IMAGE_MAX_SIDE on a side are refused. */
#include <stddef.h>

#define WENA_IMAGE_MAX_SIDE 8192

typedef struct WenaImage {
    unsigned char *rgba;      /* width * height * 4 bytes */
    unsigned long width, height;
} WenaImage;

typedef enum WenaImageResult {
    WENA_IMAGE_OK,
    WENA_IMAGE_UNKNOWN,       /* not PNG, JPEG or GIF */
    WENA_IMAGE_UNSUPPORTED,   /* WebP, progressive or arithmetic JPEG, ... */
    WENA_IMAGE_CORRUPT,
    WENA_IMAGE_TOO_LARGE,
    WENA_IMAGE_NO_MEMORY
} WenaImageResult;

WenaImageResult wena_image_decode(const unsigned char *data, size_t length, WenaImage *image);
void wena_image_free(WenaImage *image);

/* RFC 1950 zlib data inflated into `out`; returns the bytes written, or
 * (size_t)-1 when corrupt or `capacity` is too small. */
size_t wena_inflate_zlib(const unsigned char *data, size_t length, unsigned char *out, size_t capacity);

#endif
