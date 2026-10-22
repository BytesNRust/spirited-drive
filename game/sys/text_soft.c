/* port: the look of the port's large text (text_soft.h). */
#include "game/sys/text_soft.h"

static int letter(const uint8_t *mask, int w, int h, int x, int y)
{
    return x >= 0 && x < w && y >= 0 && y < h && mask[y * w + x] >= TEXT_SOFT_SOLID_FROM;
}

TextSoftPart text_soft_back(const uint8_t *mask, int w, int h, int x, int y)
{
    if (!letter(mask, w, h, x, y) && letter(mask, w, h, x + 1, y + 1)) return TEXT_SOFT_RIM;
    for (int d = 1; d <= TEXT_SOFT_SHADOW_DEPTH; d++)
        if (letter(mask, w, h, x - d, y - d)) return TEXT_SOFT_SHADOW;
    return TEXT_SOFT_NONE;
}

int text_soft_edge(const uint8_t *mask, int w, int h, int x, int y)
{
    return !letter(mask, w, h, x + 1, y + 1);
}

int text_soft_shadow(const uint8_t *mask, int w, int h, int x, int y)
{
    int coverage = 0;
    for (int d = 1; d <= TEXT_SOFT_SHADOW_DEPTH; d++)
        if (x - d >= 0 && y - d >= 0 && x - d < w && y - d < h && mask[(y - d) * w + x - d] > coverage)
            coverage = mask[(y - d) * w + x - d];
    return (coverage * TEXT_SOFT_SHADOW_LEVEL + 7) / 15;
}

int text_soft_title_shadow(const uint8_t *mask, int w, int h, int x, int y)
{
    static const int LEVEL[TEXT_SOFT_SHADOW_DEPTH + 1] = { 0, TEXT_SOFT_TITLE_HARD_LEVEL, TEXT_SOFT_TITLE_SOFT_LEVEL };
    int dark = 0;
    for (int d = 1; d <= TEXT_SOFT_SHADOW_DEPTH; d++)
        if (x - d >= 0 && y - d >= 0 && x - d < w && y - d < h) {
            const int level = (mask[(y - d) * w + x - d] * LEVEL[d] + 7) / 15;
            if (level > dark) dark = level;
        }
    return dark;
}

int text_soft_grain(int x, int y)
{
    const unsigned hash = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    return ((hash >> 7) & 3) == 0;
}

int text_soft_header_shadow(const uint8_t *mask, int w, int h, int x, int y)
{
    return x >= 1 && y >= 1 && x - 1 < w && y - 1 < h ? mask[(y - 1) * w + x - 1] : 0;
}

int text_soft_last_row(const uint8_t *mask, int w, int h)
{
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++)
            if (letter(mask, w, h, x, y)) return y;
    return -1;
}
