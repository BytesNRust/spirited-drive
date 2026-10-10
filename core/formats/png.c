#include "formats/png.h"

#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include "../../third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

enum {
    MAX_DIMENSION = 16384,
    STORED_BLOCK = 65535,
    COLOR_INDEXED = 3,
    COLOR_RGBA = 6
};

static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    static uint32_t table[256];
    if (table[1] == 0) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    }
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static uint32_t get_be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* Appends a chunk: length, type, data, CRC of type + data. */
static uint8_t *chunk(uint8_t *at, const char type[4], const uint8_t *data, size_t n)
{
    put_be32(at, (uint32_t)n);
    memcpy(at + 4, type, 4);
    if (n) memcpy(at + 8, data, n);
    put_be32(at + 8 + n, crc32_update(0, at + 4, 4 + n));
    return at + 12 + n;
}

/* zlib stream of stored blocks over `raw`. */
static size_t zlib_stored(const uint8_t *raw, size_t n, uint8_t *out)
{
    size_t o = 0;
    out[o++] = 0x78;
    out[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    size_t left = n, at = 0;
    do {
        const size_t take = left > STORED_BLOCK ? STORED_BLOCK : left;
        out[o++] = left == take ? 1 : 0;
        out[o++] = (uint8_t)take; out[o++] = (uint8_t)(take >> 8);
        out[o++] = (uint8_t)~take; out[o++] = (uint8_t)(~take >> 8);
        memcpy(out + o, raw + at, take);
        o += take; at += take; left -= take;
    } while (left > 0);
    put_be32(out + o, b << 16 | a);
    return o + 4;
}

static PngResult write_image(const uint8_t *pixels, uint32_t w, uint32_t h, int color, int bytes_per_pixel,
                             const uint8_t *palette, uint8_t **out, size_t *out_len)
{
    if (w == 0 || h == 0 || w > MAX_DIMENSION || h > MAX_DIMENSION) return PNG_ERR_SIZE;
    const size_t row = 1 + (size_t)w * (size_t)bytes_per_pixel;
    const size_t raw_len = row * h;
    const size_t blocks = raw_len / STORED_BLOCK + 1;
    const size_t z_len = 2 + raw_len + 5 * blocks + 4;
    uint8_t *raw = malloc(raw_len);
    uint8_t *file = malloc(8 + 25 + (palette ? 12 + 768 : 0) + 12 + z_len + 12);
    uint8_t *z = malloc(z_len);
    if (raw == NULL || file == NULL || z == NULL) { free(raw); free(file); free(z); return PNG_ERR_MEMORY; }
    for (uint32_t y = 0; y < h; ++y) {
        raw[y * row] = 0;  /* filter: none */
        memcpy(raw + y * row + 1, pixels + (size_t)y * w * (size_t)bytes_per_pixel, (size_t)w * (size_t)bytes_per_pixel);
    }
    const size_t zn = zlib_stored(raw, raw_len, z);
    uint8_t ihdr[13];
    put_be32(ihdr, w);
    put_be32(ihdr + 4, h);
    ihdr[8] = 8; ihdr[9] = (uint8_t)color; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    uint8_t *at = file;
    memcpy(at, signature, 8);
    at += 8;
    at = chunk(at, "IHDR", ihdr, 13);
    if (palette) at = chunk(at, "PLTE", palette, 768);
    at = chunk(at, "IDAT", z, zn);
    at = chunk(at, "IEND", NULL, 0);
    free(raw);
    free(z);
    *out = file;
    *out_len = (size_t)(at - file);
    return PNG_OK;
}

PngResult png_write_indexed(const uint8_t *indices, uint32_t width, uint32_t height, const uint8_t palette[768],
                            uint8_t **out, size_t *out_len)
{
    return write_image(indices, width, height, COLOR_INDEXED, 1, palette, out, out_len);
}

PngResult png_write_rgba(const uint8_t *rgba, uint32_t width, uint32_t height, uint8_t **out, size_t *out_len)
{
    return write_image(rgba, width, height, COLOR_RGBA, 4, NULL, out, out_len);
}

/* Undoes the PNG row filters in place; `raw` holds height rows of 1 filter byte + width bytes. */
static int unfilter(uint8_t *raw, uint32_t width, uint32_t height)
{
    const size_t row = 1 + (size_t)width;
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t *cur = raw + y * row + 1;
        const uint8_t *up = y ? raw + (y - 1) * row + 1 : NULL;
        for (uint32_t x = 0; x < width; ++x) {
            const int left = x ? cur[x - 1] : 0, above = up ? up[x] : 0, corner = (x && up) ? up[x - 1] : 0;
            int predict;
            switch (raw[y * row]) {
            case 0: predict = 0; break;
            case 1: predict = left; break;
            case 2: predict = above; break;
            case 3: predict = (left + above) / 2; break;
            case 4: {
                const int p = left + above - corner;
                const int pa = abs(p - left), pb = abs(p - above), pc = abs(p - corner);
                predict = (pa <= pb && pa <= pc) ? left : (pb <= pc ? above : corner);
                break;
            }
            default: return 0;
            }
            cur[x] = (uint8_t)(cur[x] + predict);
        }
    }
    return 1;
}

/* 8-bit indexed, no interlace: 1 handled, 0 not of that kind, -1 damaged. */
static int read_indexed(const uint8_t *data, size_t len, PngImage *image)
{
    if (len < 8 + 25 || memcmp(data, signature, 8) != 0) return 0;
    uint32_t w = 0, h = 0;
    int indexed = 0, have_palette = 0;
    uint8_t *idat = NULL;
    size_t idat_len = 0;
    size_t at = 8;
    int status = 1;
    while (at + 12 <= len && status == 1) {
        const uint32_t n = get_be32(data + at);
        if (n > len - at - 12) { status = -1; break; }
        const uint8_t *type = data + at + 4, *body = data + at + 8;
        if (crc32_update(0, type, 4 + n) != get_be32(body + n)) { status = -1; break; }
        if (memcmp(type, "IHDR", 4) == 0 && n == 13) {
            w = get_be32(body);
            h = get_be32(body + 4);
            indexed = body[8] == 8 && body[9] == COLOR_INDEXED && body[12] == 0;
            if (!indexed) { status = 0; break; }
        } else if (memcmp(type, "PLTE", 4) == 0 && n % 3 == 0 && n <= 768) {
            memset(image->palette, 0, 768);
            memcpy(image->palette, body, n);
            have_palette = 1;
        } else if (memcmp(type, "IDAT", 4) == 0) {
            uint8_t *grown = realloc(idat, idat_len + n + 1);
            if (grown == NULL) { status = -1; break; }
            idat = grown;
            memcpy(idat + idat_len, body, n);
            idat_len += n;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        at += 12 + n;
    }
    if (status == 1) {
        int out_len = 0;
        uint8_t *raw = NULL;
        if (!indexed || !have_palette || w == 0 || h == 0 || w > MAX_DIMENSION || h > MAX_DIMENSION) status = -1;
        else raw = (uint8_t *)stbi_zlib_decode_malloc((const char *)idat, (int)idat_len, &out_len);
        if (status == 1 && (raw == NULL || (size_t)out_len != (1 + (size_t)w) * h || !unfilter(raw, w, h))) status = -1;
        if (status == 1) {
            image->pixels = malloc((size_t)w * h);
            if (image->pixels == NULL) status = -1;
            else for (uint32_t y = 0; y < h; ++y) memcpy(image->pixels + (size_t)y * w, raw + y * (1 + (size_t)w) + 1, w);
        }
        free(raw);
    }
    free(idat);
    if (status == 1) { image->width = w; image->height = h; image->indexed = 1; }
    return status;
}

PngResult png_read(const uint8_t *data, size_t len, PngImage *image)
{
    memset(image, 0, sizeof *image);
    const int r = read_indexed(data, len, image);
    if (r == 1) return PNG_OK;
    if (r < 0) return PNG_ERR_CORRUPT;
    int w, h, channels;
    uint8_t *rgba = stbi_load_from_memory(data, (int)len, &w, &h, &channels, 4);
    if (rgba == NULL) return PNG_ERR_FORMAT;
    if (w <= 0 || h <= 0 || w > MAX_DIMENSION || h > MAX_DIMENSION) { stbi_image_free(rgba); return PNG_ERR_SIZE; }
    image->width = (uint32_t)w;
    image->height = (uint32_t)h;
    image->pixels = rgba;
    return PNG_OK;
}

void png_free(void *buffer)
{
    free(buffer);
}

const char *png_result_str(PngResult result)
{
    switch (result) {
    case PNG_OK: return "ok";
    case PNG_ERR_MEMORY: return "out of memory";
    case PNG_ERR_SIZE: return "image size is zero or over 16384";
    case PNG_ERR_FORMAT: return "not a readable PNG";
    case PNG_ERR_CORRUPT: return "damaged PNG (chunk, checksum or filter)";
    }
    return "unknown error";
}
