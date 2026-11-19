/* PNG for the image exchange. Writing is ours (stored deflate blocks: big, but trivial and exact); reading
 * parses 8-bit indexed images itself so the palette indices survive, and hands everything else to stb_image
 * (third_party/stb_image.h) as RGBA. */
#ifndef TNFS_PNG_H
#define TNFS_PNG_H

#include <stddef.h>
#include <stdint.h>

/* the most pixels a PNG may have (width x height) to be read: a mod's picture is far smaller; more is refused (PNG_ERR_SIZE) before
 * anything is allocated */
enum { PNG_MAX_PIXELS = 2048 * 2048 };

typedef enum {
    PNG_OK = 0,
    PNG_ERR_MEMORY,
    PNG_ERR_SIZE,      /* zero or oversized dimensions */
    PNG_ERR_FORMAT,    /* not a PNG, or a PNG stb_image cannot read */
    PNG_ERR_CORRUPT,   /* chunk, checksum or filter damage, or pixel data short of the image's rows */
    PNG_ERR_PAST_ROWS  /* pixel data that inflates past the rows the header implies (an odd encoder, or a bomb): re-save the picture */
} PngResult;

typedef struct {
    uint32_t width, height;
    int indexed;            /* 1: `pixels` are palette indices and `palette` is filled (RGB, 256 entries, zero padded) */
    uint8_t palette[768];
    uint8_t *pixels;        /* indexed: width x height bytes; else width x height x 4 (RGBA); free with png_free */
} PngImage;

PngResult png_write_indexed(const uint8_t *indices, uint32_t width, uint32_t height, const uint8_t palette[768],
                            uint8_t **out, size_t *out_len);
/* A grey 8-bit picture: `grey` is width x height bytes, one level a texel (the masks' format, data/car_masks). */
PngResult png_write_grey(const uint8_t *grey, uint32_t width, uint32_t height, uint8_t **out, size_t *out_len);
PngResult png_write_rgba(const uint8_t *rgba, uint32_t width, uint32_t height, uint8_t **out, size_t *out_len);
PngResult png_read(const uint8_t *data, size_t len, PngImage *image);
/* The width and height the PNG's header (IHDR, its first chunk) says, read without unpacking anything: what a reader that must bound the
 * pixels asks before png_read. PNG_ERR_FORMAT when there is no such header. */
PngResult png_size(const uint8_t *data, size_t len, uint32_t *width, uint32_t *height);
void png_free(void *buffer);

const char *png_result_str(PngResult result);

#endif
