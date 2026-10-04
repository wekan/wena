#include "image_decode.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Inflate (RFC 1951) ------------------------------------------------------- */

typedef struct Bits {
    const unsigned char *data;
    size_t length, at;
    unsigned long buffer;
    int count;
    int error;
} Bits;

static unsigned int bits(Bits *b, int need)
{
    unsigned int value;
    while (b->count < need) {
        if (b->at >= b->length) { b->error = 1; return 0; }
        b->buffer |= (unsigned long)b->data[b->at++] << b->count;
        b->count += 8;
    }
    value = (unsigned int)(b->buffer & ((1UL << need) - 1UL));
    b->buffer >>= need;
    b->count -= need;
    return value;
}

/* A canonical Huffman code: counts per length, symbols by code. */
typedef struct Huffman {
    unsigned short count[16];
    unsigned short symbol[320];
} Huffman;

static int huffman_build(Huffman *h, const unsigned char *lengths, int n)
{
    unsigned short offsets[16];
    int i, left = 1;
    memset(h->count, 0, sizeof(h->count));
    for (i = 0; i < n; ++i) h->count[lengths[i]]++;
    h->count[0] = 0;
    for (i = 1; i < 16; ++i) { left <<= 1; left -= h->count[i]; if (left < 0) return 0; }
    offsets[1] = 0;
    for (i = 1; i < 15; ++i) offsets[i + 1] = (unsigned short)(offsets[i] + h->count[i]);
    for (i = 0; i < n; ++i) if (lengths[i]) h->symbol[offsets[lengths[i]]++] = (unsigned short)i;
    return 1;
}

static int huffman_decode(Bits *b, const Huffman *h)
{
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len < 16; ++len) {
        int count;
        code |= (int)bits(b, 1);
        if (b->error) return -1;
        count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static const unsigned short length_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                               67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned char length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
                                               5, 5, 5, 5, 0};
static const unsigned short distance_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
                                                 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char distance_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
                                                 10, 11, 11, 12, 12, 13, 13};

static int inflate_block(Bits *b, const Huffman *lit, const Huffman *dist, unsigned char *out, size_t capacity,
                         size_t *used)
{
    for (;;) {
        int symbol = huffman_decode(b, lit);
        if (symbol < 0) return 0;
        if (symbol < 256) {
            if (*used >= capacity) return 0;
            out[(*used)++] = (unsigned char)symbol;
        } else if (symbol == 256) return 1;
        else {
            int length, distance, d;
            symbol -= 257;
            if (symbol >= 29) return 0;
            length = length_base[symbol] + (int)bits(b, length_extra[symbol]);
            d = huffman_decode(b, dist);
            if (d < 0 || d >= 30) return 0;
            distance = distance_base[d] + (int)bits(b, distance_extra[d]);
            if (b->error || (size_t)distance > *used || *used + (size_t)length > capacity) return 0;
            while (length-- > 0) { out[*used] = out[*used - (size_t)distance]; ++*used; }
        }
    }
}

size_t wena_inflate_zlib(const unsigned char *data, size_t length, unsigned char *out, size_t capacity)
{
    Bits b;
    size_t used = 0;
    int last;
    if (data == NULL || length < 2 || (data[0] & 15) != 8 || ((data[0] << 8) | data[1]) % 31 != 0 || (data[1] & 32))
        return (size_t)-1;
    memset(&b, 0, sizeof(b));
    b.data = data + 2;
    b.length = length - 2;
    do {
        int type;
        last = (int)bits(&b, 1);
        type = (int)bits(&b, 2);
        if (b.error) return (size_t)-1;
        if (type == 0) {
            unsigned int len, nlen;
            b.buffer = 0;
            b.count = 0;
            if (b.at + 4 > b.length) return (size_t)-1;
            len = b.data[b.at] | (b.data[b.at + 1] << 8);
            nlen = b.data[b.at + 2] | (b.data[b.at + 3] << 8);
            b.at += 4;
            if ((len ^ 0xFFFFu) != nlen || b.at + len > b.length || used + len > capacity) return (size_t)-1;
            memcpy(out + used, b.data + b.at, len);
            used += len;
            b.at += len;
        } else if (type == 1) {
            static Huffman fixed_lit, fixed_dist;
            static int ready;
            if (!ready) {
                unsigned char lengths[288];
                int i;
                for (i = 0; i < 144; ++i) lengths[i] = 8;
                for (; i < 256; ++i) lengths[i] = 9;
                for (; i < 280; ++i) lengths[i] = 7;
                for (; i < 288; ++i) lengths[i] = 8;
                huffman_build(&fixed_lit, lengths, 288);
                for (i = 0; i < 30; ++i) lengths[i] = 5;
                huffman_build(&fixed_dist, lengths, 30);
                ready = 1;
            }
            if (!inflate_block(&b, &fixed_lit, &fixed_dist, out, capacity, &used)) return (size_t)-1;
        } else if (type == 2) {
            static const unsigned char order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            unsigned char lengths[320], code_lengths[19];
            Huffman lit, dist, codes;
            int nlit = (int)bits(&b, 5) + 257, ndist = (int)bits(&b, 5) + 1, ncode = (int)bits(&b, 4) + 4, i, n = 0;
            if (b.error || nlit > 286 || ndist > 30) return (size_t)-1;
            memset(code_lengths, 0, sizeof(code_lengths));
            for (i = 0; i < ncode; ++i) code_lengths[order[i]] = (unsigned char)bits(&b, 3);
            if (b.error || !huffman_build(&codes, code_lengths, 19)) return (size_t)-1;
            while (n < nlit + ndist) {
                int symbol = huffman_decode(&b, &codes), repeat;
                unsigned char value = 0;
                if (symbol < 0) return (size_t)-1;
                if (symbol < 16) { lengths[n++] = (unsigned char)symbol; continue; }
                if (symbol == 16) { if (n == 0) return (size_t)-1; value = lengths[n - 1]; repeat = 3 + (int)bits(&b, 2); }
                else if (symbol == 17) repeat = 3 + (int)bits(&b, 3);
                else repeat = 11 + (int)bits(&b, 7);
                if (b.error || n + repeat > nlit + ndist) return (size_t)-1;
                while (repeat-- > 0) lengths[n++] = value;
            }
            if (!huffman_build(&lit, lengths, nlit) || !huffman_build(&dist, lengths + nlit, ndist)) {
                /* A single distance code is allowed to be incomplete. */
                huffman_build(&dist, lengths + nlit, ndist);
            }
            if (!inflate_block(&b, &lit, &dist, out, capacity, &used)) return (size_t)-1;
        } else return (size_t)-1;
    } while (!last);
    return used;
}

/* PNG ---------------------------------------------------------------------- */

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* Unfilters one pass's scanlines in place; returns 0 when corrupt. */
static int unfilter(unsigned char *rows, unsigned long width, unsigned long height, size_t stride, int bpp)
{
    unsigned long y;
    size_t x;
    for (y = 0; y < height; ++y) {
        unsigned char *line = rows + y * (stride + 1), *prior = y ? rows + (y - 1) * (stride + 1) + 1 : NULL;
        int filter = line[0];
        unsigned char *cur = line + 1;
        (void)width;
        if (filter > 4) return 0;
        for (x = 0; x < stride; ++x) {
            int a = x >= (size_t)bpp ? cur[x - (size_t)bpp] : 0, b = prior ? prior[x] : 0,
                c = prior && x >= (size_t)bpp ? prior[x - (size_t)bpp] : 0;
            switch (filter) {
            case 1: cur[x] = (unsigned char)(cur[x] + a); break;
            case 2: cur[x] = (unsigned char)(cur[x] + b); break;
            case 3: cur[x] = (unsigned char)(cur[x] + ((a + b) >> 1)); break;
            case 4: cur[x] = (unsigned char)(cur[x] + paeth(a, b, c)); break;
            default: break;
            }
        }
    }
    return 1;
}

static unsigned int sample(const unsigned char *line, unsigned long x, int depth, int channel, int channels)
{
    unsigned long index = x * (unsigned long)channels + (unsigned long)channel;
    if (depth == 8) return line[index];
    if (depth == 16) return line[index * 2];
    {
        unsigned long bit = index * (unsigned long)depth;
        unsigned int value = (line[bit / 8] >> (8 - depth - (int)(bit % 8))) & ((1u << depth) - 1u);
        return value;
    }
}

static unsigned int sample16(const unsigned char *line, unsigned long x, int channel, int channels)
{
    unsigned long index = (x * (unsigned long)channels + (unsigned long)channel) * 2;
    return ((unsigned int)line[index] << 8) | line[index + 1];
}

static WenaImageResult png(const unsigned char *data, size_t length, WenaImage *image)
{
    size_t at = 8, idat_length = 0, raw_length, offset;
    unsigned char *idat = NULL, *raw = NULL, palette[256][4];
    unsigned long width = 0, height = 0;
    int depth = 0, color = 0, interlace = 0, channels, palette_count = 0, has_trns = 0, pass;
    unsigned int trns[3] = {0, 0, 0};
    static const int starts_x[7] = {0, 4, 0, 2, 0, 1, 0}, starts_y[7] = {0, 0, 4, 0, 2, 0, 1};
    static const int steps_x[7] = {8, 8, 4, 4, 2, 2, 1}, steps_y[7] = {8, 8, 8, 4, 4, 2, 2};
    memset(palette, 255, sizeof(palette));
    while (at + 12 <= length) {
        unsigned long chunk = be32(data + at);
        const unsigned char *type = data + at + 4, *body = data + at + 8;
        if (chunk > length - at - 12) { free(idat); return WENA_IMAGE_CORRUPT; }
        if (!memcmp(type, "IHDR", 4) && chunk >= 13) {
            width = be32(body);
            height = be32(body + 4);
            depth = body[8];
            color = body[9];
            interlace = body[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            unsigned long i;
            palette_count = (int)(chunk / 3 > 256 ? 256 : chunk / 3);
            for (i = 0; i < (unsigned long)palette_count; ++i) {
                palette[i][0] = body[i * 3]; palette[i][1] = body[i * 3 + 1]; palette[i][2] = body[i * 3 + 2];
            }
        } else if (!memcmp(type, "tRNS", 4)) {
            unsigned long i;
            has_trns = 1;
            if (color == 3) for (i = 0; i < chunk && i < 256; ++i) palette[i][3] = body[i];
            else if (color == 0 && chunk >= 2) trns[0] = ((unsigned int)body[0] << 8) | body[1];
            else if (color == 2 && chunk >= 6) {
                trns[0] = ((unsigned int)body[0] << 8) | body[1];
                trns[1] = ((unsigned int)body[2] << 8) | body[3];
                trns[2] = ((unsigned int)body[4] << 8) | body[5];
            }
        } else if (!memcmp(type, "IDAT", 4)) {
            unsigned char *bigger = (unsigned char *)realloc(idat, idat_length + chunk + 1);
            if (bigger == NULL) { free(idat); return WENA_IMAGE_NO_MEMORY; }
            idat = bigger;
            memcpy(idat + idat_length, body, chunk);
            idat_length += chunk;
        } else if (!memcmp(type, "IEND", 4)) break;
        at += 12 + chunk;
    }
    channels = color == 0 ? 1 : color == 2 ? 3 : color == 3 ? 1 : color == 4 ? 2 : color == 6 ? 4 : 0;
    if (idat == NULL || width == 0 || height == 0 || channels == 0 || interlace > 1 ||
        !(depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16) ||
        (color == 3 && depth == 16) || ((color == 2 || color == 4 || color == 6) && depth < 8)) {
        free(idat);
        return idat == NULL || width == 0 ? WENA_IMAGE_CORRUPT : WENA_IMAGE_UNSUPPORTED;
    }
    if (width > WENA_IMAGE_MAX_SIDE || height > WENA_IMAGE_MAX_SIDE) { free(idat); return WENA_IMAGE_TOO_LARGE; }
    /* The raw size of every pass's filtered scanlines. */
    raw_length = 0;
    for (pass = 0; pass < (interlace ? 7 : 1); ++pass) {
        unsigned long pw = interlace ? (width + (unsigned long)(steps_x[pass] - starts_x[pass] - 1)) / (unsigned long)steps_x[pass] : width;
        unsigned long ph = interlace ? (height + (unsigned long)(steps_y[pass] - starts_y[pass] - 1)) / (unsigned long)steps_y[pass] : height;
        if (pw && ph) raw_length += ph * ((pw * (unsigned long)channels * (unsigned long)depth + 7) / 8 + 1);
    }
    raw = (unsigned char *)malloc(raw_length + 1);
    image->rgba = (unsigned char *)calloc(width * height, 4);
    if (raw == NULL || image->rgba == NULL) { free(idat); free(raw); free(image->rgba); image->rgba = NULL; return WENA_IMAGE_NO_MEMORY; }
    if (wena_inflate_zlib(idat, idat_length, raw, raw_length) != raw_length) {
        free(idat); free(raw); free(image->rgba); image->rgba = NULL;
        return WENA_IMAGE_CORRUPT;
    }
    free(idat);
    offset = 0;
    for (pass = 0; pass < (interlace ? 7 : 1); ++pass) {
        unsigned long pw = interlace ? (width + (unsigned long)(steps_x[pass] - starts_x[pass] - 1)) / (unsigned long)steps_x[pass] : width;
        unsigned long ph = interlace ? (height + (unsigned long)(steps_y[pass] - starts_y[pass] - 1)) / (unsigned long)steps_y[pass] : height;
        size_t stride = (pw * (unsigned long)channels * (unsigned long)depth + 7) / 8;
        int bpp = (channels * depth + 7) / 8;
        unsigned long y, x;
        if (pw == 0 || ph == 0) continue;
        if (!unfilter(raw + offset, pw, ph, stride, bpp)) { free(raw); free(image->rgba); image->rgba = NULL; return WENA_IMAGE_CORRUPT; }
        for (y = 0; y < ph; ++y) {
            const unsigned char *line = raw + offset + y * (stride + 1) + 1;
            unsigned long iy = interlace ? (unsigned long)starts_y[pass] + y * (unsigned long)steps_y[pass] : y;
            for (x = 0; x < pw; ++x) {
                unsigned long ix = interlace ? (unsigned long)starts_x[pass] + x * (unsigned long)steps_x[pass] : x;
                unsigned char *px = image->rgba + (iy * width + ix) * 4;
                unsigned int scale = depth < 8 ? 255u / ((1u << depth) - 1u) : 1u;
                if (color == 3) {
                    unsigned int index = sample(line, x, depth, 0, 1);
                    if ((int)index >= palette_count) index = 0;
                    memcpy(px, palette[index], 4);
                } else if (color == 0 || color == 4) {
                    unsigned int g = sample(line, x, depth, 0, channels);
                    px[0] = px[1] = px[2] = (unsigned char)(g * scale);
                    px[3] = color == 4 ? (unsigned char)sample(line, x, depth, 1, channels) : 255;
                    if (color == 0 && has_trns && (depth == 16 ? sample16(line, x, 0, 1) : g) == trns[0]) px[3] = 0;
                } else {
                    px[0] = (unsigned char)sample(line, x, depth, 0, channels);
                    px[1] = (unsigned char)sample(line, x, depth, 1, channels);
                    px[2] = (unsigned char)sample(line, x, depth, 2, channels);
                    px[3] = color == 6 ? (unsigned char)sample(line, x, depth, 3, channels) : 255;
                    if (color == 2 && has_trns) {
                        unsigned int r = depth == 16 ? sample16(line, x, 0, 3) : px[0];
                        unsigned int g = depth == 16 ? sample16(line, x, 1, 3) : px[1];
                        unsigned int b = depth == 16 ? sample16(line, x, 2, 3) : px[2];
                        if (r == trns[0] && g == trns[1] && b == trns[2]) px[3] = 0;
                    }
                }
            }
        }
        offset += ph * (stride + 1);
    }
    free(raw);
    image->width = width;
    image->height = height;
    return WENA_IMAGE_OK;
}

/* JPEG (baseline) ------------------------------------------------------------ */

typedef struct JpegHuffman {
    unsigned char lengths[17];
    unsigned char values[256];
    int max_code[18];
    int val_offset[17];
    int present;
} JpegHuffman;

typedef struct JpegComponent {
    int id, h, v, quant, dc_table, ac_table;
    int dc_pred;
    unsigned long width, height;   /* in blocks * 8, padded */
    unsigned char *pixels;
} JpegComponent;

typedef struct Jpeg {
    const unsigned char *data;
    size_t length, at;
    unsigned long buffer;
    int count;
    int marker_hit;
    unsigned short quant[4][64];
    JpegHuffman dc[4], ac[4];
    JpegComponent comp[3];
    int components;
    unsigned long width, height;
    int hmax, vmax;
    int restart;
} Jpeg;

static const unsigned char zigzag[64] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52,
    45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

static void jpeg_huffman(JpegHuffman *h)
{
    int code = 0, k = 0, len;
    for (len = 1; len <= 16; ++len) {
        h->val_offset[len] = k - code;
        if (h->lengths[len]) {
            code += h->lengths[len];
            k += h->lengths[len];
            h->max_code[len] = code - 1;
        } else h->max_code[len] = -1;
        code <<= 1;
    }
    h->max_code[17] = 0x7fffffff;
    h->present = 1;
}

static int jpeg_bit(Jpeg *j)
{
    if (j->count == 0) {
        unsigned int byte;
        if (j->marker_hit || j->at >= j->length) { j->buffer = 0; j->count = 8; }
        else {
            byte = j->data[j->at++];
            if (byte == 0xFF) {
                unsigned int next = j->at < j->length ? j->data[j->at] : 0;
                if (next == 0) ++j->at;
                else { j->marker_hit = 1; --j->at; byte = 0; }
            }
            j->buffer = byte;
            j->count = 8;
        }
    }
    --j->count;
    return (int)((j->buffer >> j->count) & 1);
}

static int jpeg_receive(Jpeg *j, int n)
{
    int value = 0;
    while (n-- > 0) value = (value << 1) | jpeg_bit(j);
    return value;
}

static int jpeg_extend(int value, int n)
{
    return n == 0 ? 0 : value < (1 << (n - 1)) ? value - (1 << n) + 1 : value;
}

static int jpeg_decode(Jpeg *j, const JpegHuffman *h)
{
    int code = 0, len;
    for (len = 1; len <= 16; ++len) {
        code = (code << 1) | jpeg_bit(j);
        if (code <= h->max_code[len]) {
            int index = h->val_offset[len] + code;
            return index >= 0 && index < 256 ? h->values[index] : -1;
        }
    }
    return -1;
}

/* The inverse DCT of one block, separable and in floating point. */
static void idct(const int *in, unsigned char *out, unsigned long stride)
{
    static double table[8][8];
    static int ready;
    double tmp[64];
    int u, x, y;
    if (!ready) {
        for (x = 0; x < 8; ++x)
            for (u = 0; u < 8; ++u)
                table[x][u] = (u == 0 ? 0.70710678118654752 : 1.0) * cos((2.0 * x + 1.0) * u * 3.14159265358979323846 / 16.0);
        ready = 1;
    }
    for (y = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) {
            double sum = 0.0;
            for (u = 0; u < 8; ++u) sum += table[x][u] * in[y * 8 + u];
            tmp[y * 8 + x] = sum / 2.0;
        }
    for (x = 0; x < 8; ++x)
        for (y = 0; y < 8; ++y) {
            double sum = 0.0;
            int value;
            for (u = 0; u < 8; ++u) sum += table[y][u] * tmp[u * 8 + x];
            value = (int)floor(sum / 2.0 + 128.5);
            out[(unsigned long)y * stride + (unsigned long)x] = (unsigned char)(value < 0 ? 0 : value > 255 ? 255 : value);
        }
}

static int jpeg_block(Jpeg *j, JpegComponent *c, int *block)
{
    int t, k, value;
    memset(block, 0, 64 * sizeof(int));
    if (!j->dc[c->dc_table].present || !j->ac[c->ac_table].present) return 0;
    t = jpeg_decode(j, &j->dc[c->dc_table]);
    if (t < 0 || t > 11) return 0;
    c->dc_pred += jpeg_extend(jpeg_receive(j, t), t);
    block[0] = c->dc_pred * j->quant[c->quant][0];
    for (k = 1; k < 64;) {
        int rs = jpeg_decode(j, &j->ac[c->ac_table]), r, s;
        if (rs < 0) return 0;
        r = rs >> 4;
        s = rs & 15;
        if (s == 0) { if (r != 15) break; k += 16; continue; }
        k += r;
        if (k > 63) return 0;
        value = jpeg_extend(jpeg_receive(j, s), s);
        block[zigzag[k]] = value * j->quant[c->quant][k];
        ++k;
    }
    return 1;
}

static WenaImageResult jpeg(const unsigned char *data, size_t length, WenaImage *image)
{
    Jpeg j;
    int i, frame = 0;
    memset(&j, 0, sizeof(j));
    j.data = data;
    j.length = length;
    j.at = 2;
    for (;;) {
        unsigned int marker, segment;
        while (j.at < length && data[j.at] != 0xFF) ++j.at;
        while (j.at < length && data[j.at] == 0xFF) ++j.at;
        if (j.at >= length) return WENA_IMAGE_CORRUPT;
        marker = data[j.at++];
        if (marker == 0xD9) return WENA_IMAGE_CORRUPT;
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
        if (j.at + 2 > length) return WENA_IMAGE_CORRUPT;
        segment = ((unsigned int)data[j.at] << 8) | data[j.at + 1];
        if (segment < 2 || j.at + segment > length) return WENA_IMAGE_CORRUPT;
        if (marker == 0xDB) {
            size_t p = j.at + 2, end = j.at + segment;
            while (p < end) {
                int precision = data[p] >> 4, id = data[p] & 3, k;
                ++p;
                for (k = 0; k < 64; ++k) {
                    if (p + (precision ? 2 : 1) > end) return WENA_IMAGE_CORRUPT;
                    j.quant[id][k] = (unsigned short)(precision ? (data[p] << 8) | data[p + 1] : data[p]);
                    p += precision ? 2 : 1;
                }
            }
        } else if (marker == 0xC4) {
            size_t p = j.at + 2, end = j.at + segment;
            while (p < end) {
                int cls = data[p] >> 4, id = data[p] & 3, total = 0, k;
                JpegHuffman *h = cls ? &j.ac[id] : &j.dc[id];
                ++p;
                if (p + 16 > end) return WENA_IMAGE_CORRUPT;
                for (k = 1; k <= 16; ++k) { h->lengths[k] = data[p + (size_t)k - 1]; total += h->lengths[k]; }
                p += 16;
                if (total > 256 || p + (size_t)total > end) return WENA_IMAGE_CORRUPT;
                memcpy(h->values, data + p, (size_t)total);
                p += (size_t)total;
                jpeg_huffman(h);
            }
        } else if (marker == 0xC0 || marker == 0xC1) {
            const unsigned char *b = data + j.at + 2;
            if (segment < 8 || b[0] != 8) return WENA_IMAGE_UNSUPPORTED;
            j.height = ((unsigned long)b[1] << 8) | b[2];
            j.width = ((unsigned long)b[3] << 8) | b[4];
            j.components = b[5];
            if (j.width == 0 || j.height == 0 || (j.components != 1 && j.components != 3) ||
                segment < 8u + 3u * (unsigned int)j.components) return j.components == 4 ? WENA_IMAGE_UNSUPPORTED : WENA_IMAGE_CORRUPT;
            if (j.width > WENA_IMAGE_MAX_SIDE || j.height > WENA_IMAGE_MAX_SIDE) return WENA_IMAGE_TOO_LARGE;
            j.hmax = j.vmax = 1;
            for (i = 0; i < j.components; ++i) {
                j.comp[i].id = b[6 + i * 3];
                j.comp[i].h = b[7 + i * 3] >> 4;
                j.comp[i].v = b[7 + i * 3] & 15;
                j.comp[i].quant = b[8 + i * 3] & 3;
                if (j.comp[i].h < 1 || j.comp[i].h > 4 || j.comp[i].v < 1 || j.comp[i].v > 4) return WENA_IMAGE_CORRUPT;
                if (j.comp[i].h > j.hmax) j.hmax = j.comp[i].h;
                if (j.comp[i].v > j.vmax) j.vmax = j.comp[i].v;
            }
            frame = 1;
        } else if ((marker >= 0xC2 && marker <= 0xCF) && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            return WENA_IMAGE_UNSUPPORTED;   /* progressive, lossless or arithmetic */
        } else if (marker == 0xDD) {
            if (segment >= 4) j.restart = (data[j.at + 2] << 8) | data[j.at + 3];
        } else if (marker == 0xDA) {
            unsigned long mcu_w, mcu_h, mx, my, mcus_x, mcus_y;
            int ns, order[3], block[64], restart_count = 0;
            if (!frame) return WENA_IMAGE_CORRUPT;
            ns = data[j.at + 2];
            if (ns != j.components) return WENA_IMAGE_UNSUPPORTED;   /* a scan a component: rare in baseline */
            for (i = 0; i < ns; ++i) {
                int id = data[j.at + 3 + (size_t)i * 2], tables = data[j.at + 4 + (size_t)i * 2], k;
                order[i] = -1;
                for (k = 0; k < j.components; ++k) if (j.comp[k].id == id) order[i] = k;
                if (order[i] < 0) return WENA_IMAGE_CORRUPT;
                j.comp[order[i]].dc_table = (tables >> 4) & 3;
                j.comp[order[i]].ac_table = tables & 3;
            }
            j.at += segment;
            mcu_w = 8UL * (unsigned long)j.hmax;
            mcu_h = 8UL * (unsigned long)j.vmax;
            mcus_x = (j.width + mcu_w - 1) / mcu_w;
            mcus_y = (j.height + mcu_h - 1) / mcu_h;
            for (i = 0; i < j.components; ++i) {
                j.comp[i].width = mcus_x * 8UL * (unsigned long)j.comp[i].h;
                j.comp[i].height = mcus_y * 8UL * (unsigned long)j.comp[i].v;
                j.comp[i].pixels = (unsigned char *)malloc(j.comp[i].width * j.comp[i].height);
                if (j.comp[i].pixels == NULL) { int k; for (k = 0; k < i; ++k) free(j.comp[k].pixels); return WENA_IMAGE_NO_MEMORY; }
            }
            for (my = 0; my < mcus_y; ++my)
                for (mx = 0; mx < mcus_x; ++mx) {
                    if (j.restart && restart_count == j.restart) {
                        /* RSTn: the bits and the predictions start again. */
                        j.count = 0;
                        j.marker_hit = 0;
                        while (j.at + 1 < length && !(data[j.at] == 0xFF && data[j.at + 1] >= 0xD0 && data[j.at + 1] <= 0xD7)) ++j.at;
                        if (j.at + 1 < length) j.at += 2;
                        for (i = 0; i < j.components; ++i) j.comp[i].dc_pred = 0;
                        restart_count = 0;
                    }
                    ++restart_count;
                    for (i = 0; i < ns; ++i) {
                        JpegComponent *c = &j.comp[order[i]];
                        int bx, by;
                        for (by = 0; by < c->v; ++by)
                            for (bx = 0; bx < c->h; ++bx) {
                                unsigned long px = (mx * (unsigned long)c->h + (unsigned long)bx) * 8UL;
                                unsigned long py = (my * (unsigned long)c->v + (unsigned long)by) * 8UL;
                                if (!jpeg_block(&j, c, block)) {
                                    int k;
                                    for (k = 0; k < j.components; ++k) free(j.comp[k].pixels);
                                    return WENA_IMAGE_CORRUPT;
                                }
                                idct(block, c->pixels + py * c->width + px, c->width);
                            }
                    }
                }
            /* Upsampled and converted: YCbCr (JFIF) to RGB. */
            image->rgba = (unsigned char *)malloc(j.width * j.height * 4);
            if (image->rgba == NULL) { for (i = 0; i < j.components; ++i) free(j.comp[i].pixels); return WENA_IMAGE_NO_MEMORY; }
            {
                unsigned long x, y;
                for (y = 0; y < j.height; ++y)
                    for (x = 0; x < j.width; ++x) {
                        unsigned char *out = image->rgba + (y * j.width + x) * 4;
                        int values[3];
                        for (i = 0; i < j.components; ++i) {
                            JpegComponent *c = &j.comp[i];
                            unsigned long sx = x * (unsigned long)c->h / (unsigned long)j.hmax;
                            unsigned long sy = y * (unsigned long)c->v / (unsigned long)j.vmax;
                            values[i] = c->pixels[sy * c->width + sx];
                        }
                        if (j.components == 1) out[0] = out[1] = out[2] = (unsigned char)values[0];
                        else {
                            double yy = values[0], cb = values[1] - 128.0, cr = values[2] - 128.0;
                            double r = yy + 1.402 * cr, g = yy - 0.344136 * cb - 0.714136 * cr, b = yy + 1.772 * cb;
                            out[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : (int)(r + 0.5));
                            out[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : (int)(g + 0.5));
                            out[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : (int)(b + 0.5));
                        }
                        out[3] = 255;
                    }
            }
            for (i = 0; i < j.components; ++i) free(j.comp[i].pixels);
            image->width = j.width;
            image->height = j.height;
            return WENA_IMAGE_OK;
        }
        j.at += segment;
    }
}

/* GIF -------------------------------------------------------------------------- */

static WenaImageResult gif(const unsigned char *data, size_t length, WenaImage *image)
{
    unsigned char palette[256][3];
    size_t at = 13;
    unsigned long width, height;
    int flags, transparent = -1, i;
    if (length < 13) return WENA_IMAGE_CORRUPT;
    width = data[6] | (data[7] << 8);
    height = data[8] | (data[9] << 8);
    flags = data[10];
    memset(palette, 0, sizeof(palette));
    if (flags & 0x80) {
        int size = 2 << (flags & 7);
        if (at + (size_t)size * 3 > length) return WENA_IMAGE_CORRUPT;
        for (i = 0; i < size; ++i) memcpy(palette[i], data + at + (size_t)i * 3, 3);
        at += (size_t)size * 3;
    }
    while (at < length) {
        int block = data[at++];
        if (block == 0x21) {
            int label;
            if (at >= length) return WENA_IMAGE_CORRUPT;
            label = data[at++];
            if (label == 0xF9 && at + 5 <= length && data[at] >= 4 && (data[at + 1] & 1)) transparent = data[at + 4];
            while (at < length && data[at]) at += (size_t)data[at] + 1;
            ++at;
        } else if (block == 0x2C) {
            unsigned long left, top, w, h, x, y, pixel = 0;
            int local, interlaced, min_code, clear, end, code_size, next, previous = -1;
            unsigned char local_palette[256][3], (*colors)[3] = palette;
            unsigned short prefix[4096];
            unsigned char suffix[4096], stack[4097], first[4096];
            unsigned long buffer = 0;
            int count = 0, block_left = 0;
            unsigned char *indices;
            if (at + 9 > length) return WENA_IMAGE_CORRUPT;
            left = data[at] | (data[at + 1] << 8);
            top = data[at + 2] | (data[at + 3] << 8);
            w = data[at + 4] | (data[at + 5] << 8);
            h = data[at + 6] | (data[at + 7] << 8);
            local = data[at + 8];
            interlaced = local & 0x40;
            at += 9;
            if (local & 0x80) {
                int size = 2 << (local & 7);
                if (at + (size_t)size * 3 > length) return WENA_IMAGE_CORRUPT;
                for (i = 0; i < size; ++i) memcpy(local_palette[i], data + at + (size_t)i * 3, 3);
                at += (size_t)size * 3;
                colors = local_palette;
            }
            if (width == 0 || height == 0) { width = w; height = h; }
            if (width > WENA_IMAGE_MAX_SIDE || height > WENA_IMAGE_MAX_SIDE) return WENA_IMAGE_TOO_LARGE;
            if (at >= length || w == 0 || h == 0) return WENA_IMAGE_CORRUPT;
            min_code = data[at++];
            if (min_code < 2 || min_code > 11) return WENA_IMAGE_CORRUPT;
            indices = (unsigned char *)malloc(w * h);
            image->rgba = (unsigned char *)calloc(width * height, 4);
            if (indices == NULL || image->rgba == NULL) { free(indices); free(image->rgba); image->rgba = NULL; return WENA_IMAGE_NO_MEMORY; }
            clear = 1 << min_code;
            end = clear + 1;
            code_size = min_code + 1;
            next = clear + 2;
            for (i = 0; i < clear; ++i) { prefix[i] = 0xFFFF; suffix[i] = (unsigned char)i; first[i] = (unsigned char)i; }
            /* LZW over the image's sub-blocks. */
            while (pixel < w * h) {
                int code, top_of = 0, c;
                while (count < code_size) {
                    if (block_left == 0) {
                        if (at >= length || data[at] == 0) break;
                        block_left = data[at++];
                    }
                    if (at >= length) break;
                    buffer |= (unsigned long)data[at++] << count;
                    count += 8;
                    --block_left;
                }
                if (count < code_size) break;
                code = (int)(buffer & ((1UL << code_size) - 1));
                buffer >>= code_size;
                count -= code_size;
                if (code == clear) { code_size = min_code + 1; next = clear + 2; previous = -1; continue; }
                if (code == end) break;
                if (previous < 0) {
                    if (code >= clear) break;
                    indices[pixel++] = (unsigned char)code;
                    previous = code;
                    continue;
                }
                c = code;
                if (code >= next) {
                    if (code > next) break;
                    stack[top_of++] = first[previous];
                    c = previous;
                }
                while (c >= clear && top_of < 4096) { stack[top_of++] = suffix[c]; c = prefix[c]; }
                stack[top_of++] = (unsigned char)c;
                if (next < 4096) {
                    prefix[next] = (unsigned short)previous;
                    suffix[next] = (unsigned char)c;
                    first[next] = first[previous];
                    ++next;
                    if (next == (1 << code_size) && code_size < 12) ++code_size;
                }
                while (top_of > 0 && pixel < w * h) indices[pixel++] = stack[--top_of];
                previous = code;
            }
            /* Into the canvas, with GIF's interlaced row order. */
            {
                static const int row_starts[4] = {0, 4, 2, 1}, row_steps[4] = {8, 8, 4, 2};
                unsigned long row = 0;
                int pass = 0;
                for (y = 0; y < h; ++y) {
                    unsigned long target = y;
                    if (interlaced) {
                        while (pass < 4 && (unsigned long)row_starts[pass] + row * (unsigned long)row_steps[pass] >= h) { ++pass; row = 0; }
                        target = pass < 4 ? (unsigned long)row_starts[pass] + row * (unsigned long)row_steps[pass] : y;
                        ++row;
                    }
                    for (x = 0; x < w; ++x) {
                        unsigned long cx = left + x, cy = top + target;
                        int index = y * w + x < pixel ? indices[y * w + x] : 0;
                        unsigned char *out;
                        if (cx >= width || cy >= height) continue;
                        out = image->rgba + (cy * width + cx) * 4;
                        out[0] = colors[index][0]; out[1] = colors[index][1]; out[2] = colors[index][2];
                        out[3] = index == transparent ? 0 : 255;
                    }
                }
            }
            free(indices);
            image->width = width;
            image->height = height;
            return WENA_IMAGE_OK;
        } else if (block == 0x3B) break;
        else return WENA_IMAGE_CORRUPT;
    }
    return WENA_IMAGE_CORRUPT;
}

WenaImageResult wena_image_decode(const unsigned char *data, size_t length, WenaImage *image)
{
    if (image == NULL) return WENA_IMAGE_CORRUPT;
    memset(image, 0, sizeof(*image));
    if (data == NULL || length < 12) return WENA_IMAGE_UNKNOWN;
    if (!memcmp(data, "\x89PNG\r\n\x1a\n", 8)) return png(data, length, image);
    if (data[0] == 0xFF && data[1] == 0xD8) return jpeg(data, length, image);
    if (!memcmp(data, "GIF87a", 6) || !memcmp(data, "GIF89a", 6)) return gif(data, length, image);
    if (!memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WEBP", 4)) return WENA_IMAGE_UNSUPPORTED;
    return WENA_IMAGE_UNKNOWN;
}

void wena_image_free(WenaImage *image)
{
    if (image == NULL) return;
    free(image->rgba);
    memset(image, 0, sizeof(*image));
}
