/* tests/test_image_decode.py's decoder side: decodes argv[1] and prints its
 * result, size and RGBA bytes as hex. */
#include "../models/image_decode.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *file;
    unsigned char *data;
    long length;
    WenaImage image;
    WenaImageResult result;
    unsigned long i;
    if (argc != 2 || (file = fopen(argv[1], "rb")) == NULL) return 2;
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = (unsigned char *)malloc((size_t)length + 1);
    if (data == NULL || fread(data, 1, (size_t)length, file) != (size_t)length) return 2;
    fclose(file);
    result = wena_image_decode(data, (size_t)length, &image);
    printf("%d %lu %lu\n", (int)result, image.width, image.height);
    for (i = 0; result == WENA_IMAGE_OK && i < image.width * image.height * 4; ++i) printf("%02x", image.rgba[i]);
    printf("\n");
    wena_image_free(&image);
    free(data);
    return 0;
}
