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

void class_overlay_apply(uint8_t *mask, int mask_width, size_t rows, const uint8_t *layer, int width, int height, int scale, const uint8_t *marks)
{
    for (int cy = 0; cy < height; cy++) {
        if (!marks[cy]) continue;
        const uint8_t *row = layer + (size_t)cy * (size_t)width;
        for (int ky = 0; ky < scale; ky++) {
            const size_t y = (size_t)cy * (size_t)scale + (size_t)ky;
            if (y >= rows) return;
            uint8_t *out = mask + y * (size_t)mask_width;
            for (int x = 0; x < width; x++) {
                const uint8_t c = row[x];
                uint8_t v;
                if (c == PSX_CLASS_DASH) v = 2;
                else if (c == PSX_CLASS_HUD) v = 0;
                else if (c >= PSX_CLASS_LIT && c <= LAST_MARK) v = c;
                else continue;
                for (int k = 0; k < scale; k++) out[(size_t)x * (size_t)scale + (size_t)k] = v;
            }
        }
    }
}
