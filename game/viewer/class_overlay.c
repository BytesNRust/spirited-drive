/* The 2D layer's marks over the light's world mask (class_overlay.h). */
#include "viewer/class_overlay.h"

#include "viewer/drive_light.h"

#include <string.h>

enum { FIRST_MARK = PSX_CLASS_DASH, LAST_MARK = PSX_CLASS_LIT_COLOUR + PSX_CLASS_LIT_COLOURS - 1 };

/* a class that changes the mask is 1..15: a byte with its high nibble 0 that is not 0. Eight bytes at a time: a word none of whose bytes has a zero
 * high nibble (0xff, the "nothing drawn" class, among them) holds no mark. */
static int word_may_mark(uint64_t w)
{
    const uint64_t hi = w & 0xf0f0f0f0f0f0f0f0ull;
    return ((hi - 0x0101010101010101ull) & ~hi & 0x8080808080808080ull) != 0;
}

void class_overlay_rows(const uint8_t *layer, int width, int height, uint8_t *marks)
{
    for (int y = 0; y < height; y++) {
        const uint8_t *row = layer + (size_t)y * (size_t)width;
        uint8_t any = 0;
        int x = 0;
        for (; x + 8 <= width && !any; x += 8) {
            uint64_t w;
            memcpy(&w, row + x, sizeof w);
            if (!word_may_mark(w)) continue;
            for (int k = 0; k < 8 && !any; k++) any = (uint8_t)(row[x + k] >= FIRST_MARK && row[x + k] <= LAST_MARK);
        }
        for (; x < width && !any; x++) any = (uint8_t)(row[x] >= FIRST_MARK && row[x] <= LAST_MARK);
        marks[y] = any;
    }
}

/* the mask byte a class makes, or -1 when it leaves the byte alone */
static int mark_value(uint8_t c)
{
    if (c == PSX_CLASS_DASH) return 2;
    if (c == PSX_CLASS_HUD) return 0;
    if (c >= PSX_CLASS_LIT && c <= LAST_MARK) return c;
    return -1;
}

void class_overlay_apply(uint8_t *mask, int mask_width, size_t rows, const uint8_t *layer, int width, int height, int scale, const uint8_t *marks)
{
    for (int cy = 0; cy < height; cy++) {
        if (!marks[cy]) continue;
        const uint8_t *row = layer + (size_t)cy * (size_t)width;
        /* the layer's classes come in runs (the HUD's pixels are one class across a bar): a run is one write of `scale` times its length, per output row */
        for (int x = 0; x < width;) {
            const uint8_t c = row[x];
            int end = x + 1;
            const uint64_t same = 0x0101010101010101ull * c;
            for (uint64_t w; end + 8 <= width && (memcpy(&w, row + end, sizeof w), w == same);) end += 8;
            while (end < width && row[end] == c) end++;
            const int v = mark_value(c);
            if (v >= 0) {
                for (int ky = 0; ky < scale; ky++) {
                    const size_t y = (size_t)cy * (size_t)scale + (size_t)ky;
                    if (y >= rows) return;
                    memset(mask + y * (size_t)mask_width + (size_t)x * (size_t)scale, v, (size_t)(end - x) * (size_t)scale);
                }
            }
            x = end;
        }
    }
}
