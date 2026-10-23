/* See ico.h. */
#include "formats/ico.h"

enum { HEADER = 6, ENTRY = 16, DIB_MIN = 40 };

static uint32_t u16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | u16(p + 2) << 16; }

/* the pixel (x, y) of a bottom-up row set: its palette index, or its BGR(A) bytes' start for 24 and 32 bits */
static uint32_t pixel_at(const uint8_t *rows, size_t stride, int bpp, int x, int y, int h)
{
    const uint8_t *const row = rows + (size_t)(h - 1 - y) * stride;
    if (bpp >= 24) return (uint32_t)((size_t)x * (size_t)(bpp / 8));
    const int bit = x * bpp;
    return (uint32_t)(row[bit / 8] >> (8 - bpp - bit % 8)) & ((1u << bpp) - 1);
}

/* one BMP image at `img` (`len` bytes) into rgba */
static int decode_dib(const uint8_t *img, size_t len, uint8_t *rgba, int w, int h)
{
    const uint32_t header = u32(img), bpp = u16(img + 14);
    if (header < DIB_MIN || header > len || u32(img + 16) != 0) return 0;   /* BI_RGB only */
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) return 0;
    const size_t colours = bpp <= 8 ? (u32(img + 32) != 0 ? u32(img + 32) : 1u << bpp) : 0;
    if (colours > 256) return 0;
    const size_t stride = ((size_t)w * bpp + 31) / 32 * 4, mask_stride = ((size_t)w + 31) / 32 * 4;
    const size_t at_palette = header, at_rows = at_palette + colours * 4, at_mask = at_rows + stride * (size_t)h;
    if (at_mask + mask_stride * (size_t)h > len) return 0;
    const uint8_t *const palette = img + at_palette, *const rows = img + at_rows, *const mask = img + at_mask;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *const out = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4;
            const uint32_t v = pixel_at(rows, stride, (int)bpp, x, y, h);
            const uint8_t *const bgr = bpp <= 8 ? (v < colours ? palette + v * 4 : palette)
                                                : rows + (size_t)(h - 1 - y) * stride + v;
            const uint8_t *const mrow = mask + (size_t)(h - 1 - y) * mask_stride;
            const int clear = (mrow[x / 8] >> (7 - x % 8)) & 1;
            out[0] = bgr[2], out[1] = bgr[1], out[2] = bgr[0];
            out[3] = clear ? 0 : bpp == 32 ? bgr[3] : 0xff;
            if (clear) out[0] = out[1] = out[2] = 0;
        }
    return 1;
}

int ico_decode(const uint8_t *data, size_t size, uint8_t *rgba, size_t capacity, int *w, int *h)
{
    if (size < HEADER || u16(data) != 0 || u16(data + 2) != 1) return 0;
    const uint32_t count = u16(data + 4);
    if (count == 0 || HEADER + (size_t)count * ENTRY > size) return 0;
    int best = -1, best_side = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *const e = data + HEADER + i * ENTRY;
        const int side = e[0] == 0 ? ICO_MAX_SIDE : e[0];
        if (side > best_side) best = (int)i, best_side = side;
    }
    const uint8_t *const e = data + HEADER + (size_t)best * ENTRY;
    const uint32_t length = u32(e + 8), offset = u32(e + 12);
    if (offset >= size || length > size - offset || length < DIB_MIN) return 0;
    const uint8_t *const img = data + offset;
    const int32_t iw = (int32_t)u32(img + 4), ih = (int32_t)u32(img + 8) / 2;   /* biHeight counts the mask too */
    if (iw <= 0 || ih <= 0 || iw > ICO_MAX_SIDE || ih > ICO_MAX_SIDE) return 0;
    if ((size_t)iw * (size_t)ih * 4 > capacity) return 0;
    if (!decode_dib(img, length, rgba, iw, ih)) return 0;
    *w = iw, *h = ih;
    return 1;
}
