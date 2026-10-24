/* port: see psx_bigf.h. Every offset is checked against the buffer before it is read: the files come from outside. */
#include "formats/psx_bigf.h"

#include <string.h>

enum {
    BIGF_COUNT = 8, BIGF_ENTRIES = 16,      /* u32 count at 8; entries from 16: be32 offset, be32 size, then the name */
    SHPP_COUNT = 8, SHPP_ENTRIES = 16,      /* u32 count at 8; entries from 16: a 4-byte name, le32 offset */
    SHPP_NAME = 4, SHPP_STRIDE = 8, SHPP_MAX = 256,
    BLOCK_HEADER = 16, BLOCK_CLUT = 0x23, BLOCK_IMAGE_4BPP = 0x40,
    CLUT_ENTRIES = 16, PIXEL_MASK = 0x7fff
};

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
static uint32_t le16(const uint8_t *p) { return (uint32_t)p[1] << 8 | p[0]; }
static uint32_t le24(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16; }

int psx_bigf_member(const uint8_t *file, size_t size, const char *name, const uint8_t **data, size_t *length)
{
    if (size < BIGF_ENTRIES) return 0;
    const uint32_t count = be32(file + BIGF_COUNT);
    size_t at = BIGF_ENTRIES;
    for (uint32_t i = 0; i < count; i++) {
        if (at + 8 > size) return 0;
        const uint32_t offset = be32(file + at), span = be32(file + at + 4);
        at += 8;
        size_t end = at;
        while (end < size && file[end] != '\0') end++;
        if (end >= size) return 0;
        if (end - at == strlen(name) && memcmp(file + at, name, end - at) == 0) {
            if ((size_t)offset > size || (size_t)span > size - offset) return 0;
            *data = file + offset, *length = span;
            return 1;
        }
        at = end + 1;
    }
    return 0;
}

/* The texture's end: the next texture's offset after it, or the directory's end. */
static size_t span_end(size_t size, const uint32_t *offsets, uint32_t count, uint32_t at)
{
    size_t end = size;
    for (uint32_t i = 0; i < count; i++)
        if (offsets[i] > at && offsets[i] < end) end = offsets[i];
    return end;
}

/* The pixels of one texture through its CLUT, each counted. The image block comes first; the CLUT is the first 0x23
 * block after it. 0 for a texture that is not 4bpp or does not parse. */
static int count_texture(const uint8_t *tex, size_t length, uint32_t counts[PSX_COLOURS])
{
    if (length < BLOCK_HEADER || tex[0] != BLOCK_IMAGE_4BPP) return 0;
    const uint32_t image_len = le24(tex + 1), w = le16(tex + 4), h = le16(tex + 6);
    if (image_len < BLOCK_HEADER || image_len > length) return 0;
    const size_t stride = ((size_t)w + 3) / 4 * 2;
    if ((size_t)h * stride > image_len - BLOCK_HEADER) return 0;
    size_t at = image_len;
    while (at + BLOCK_HEADER <= length && tex[at] != BLOCK_CLUT) {
        const uint32_t len = le24(tex + at + 1);
        if (len == 0 || len > length - at) return 0;
        at += len;
    }
    if (at + BLOCK_HEADER + 2 * CLUT_ENTRIES > length || tex[at] != BLOCK_CLUT) return 0;
    const uint8_t *clut = tex + at + BLOCK_HEADER, *pixels = tex + BLOCK_HEADER;
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t byte = pixels[y * stride + x / 2];
            const uint32_t index = (x & 1) ? byte >> 4 : byte & 15;
            counts[le16(clut + 2 * index) & PIXEL_MASK]++;
        }
    return 1;
}

int psx_shpp_colours(const uint8_t *file, size_t size, uint32_t counts[PSX_COLOURS])
{
    if (size < SHPP_ENTRIES) return 0;
    const uint32_t count = le32(file + SHPP_COUNT);
    if (count > SHPP_MAX || count > (size - SHPP_ENTRIES) / SHPP_STRIDE) return 0;
    uint32_t offsets[SHPP_MAX];
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *entry = file + SHPP_ENTRIES + SHPP_STRIDE * i;
        offsets[i] = le32(entry + SHPP_NAME);
        if (offsets[i] >= size) return 0;
    }
    int any = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (memcmp(file + SHPP_ENTRIES + SHPP_STRIDE * i, "shad", SHPP_NAME) == 0) continue;
        const size_t end = span_end(size, offsets, count, offsets[i]);
        any |= count_texture(file + offsets[i], end - offsets[i], counts);
    }
    return any;
}
