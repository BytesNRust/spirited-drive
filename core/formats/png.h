/* PNG for the image exchange. Writing is ours (stored deflate blocks: big, but trivial and exact); reading
 * parses 8-bit indexed images itself so the palette indices survive, and hands everything else to stb_image
 * (third_party/stb_image.h) as RGBA. */
#ifndef TNFS_PNG_H
#define TNFS_PNG_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PNG_OK = 0,
    PNG_ERR_MEMORY,
    PNG_ERR_SIZE,      /* zero or oversized dimensions */
    PNG_ERR_FORMAT,    /* not a PNG, or a PNG stb_image cannot read */
    PNG_ERR_CORRUPT    /* chunk, checksum or filter damage in an indexed image */
} PngResult;

typedef struct {
    uint32_t width, height;
    int indexed;            /* 1: `pixels` are palette indices and `palette` is filled (RGB, 256 entries, zero padded) */
    uint8_t palette[768];
    uint8_t *pixels;        /* indexed: width x height bytes; else width x height x 4 (RGBA); free with png_free */
} PngImage;

PngResult png_write_indexed(const uint8_t *indices, uint32_t width, uint32_t height, const uint8_t palette[768],
                            uint8_t **out, size_t *out_len);
PngResult png_write_rgba(const uint8_t *rgba, uint32_t width, uint32_t height, uint8_t **out, size_t *out_len);
PngResult png_read(const uint8_t *data, size_t len, PngImage *image);
void png_free(void *buffer);

const char *png_result_str(PngResult result);

#endif
