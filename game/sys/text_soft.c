/* port: the look of the port's large text (text_soft.h). */
#include "game/sys/text_soft.h"

static int letter(const uint8_t *mask, int w, int h, int x, int y)
{
    return x >= 0 && x < w && y >= 0 && y < h && mask[y * w + x] >= TEXT_SOFT_SOLID_FROM;
}

TextSoftPart text_soft_part(const uint8_t *mask, int w, int h, int x, int y)
{
    if (letter(mask, w, h, x, y))
        return letter(mask, w, h, x + 1, y + 1) ? TEXT_SOFT_FILL : TEXT_SOFT_EDGE;
    if (letter(mask, w, h, x + 1, y + 1)) return TEXT_SOFT_RIM;
    for (int d = 1; d <= TEXT_SOFT_SHADOW_DEPTH; d++)
        if (letter(mask, w, h, x - d, y - d)) return TEXT_SOFT_SHADOW;
    return TEXT_SOFT_NONE;
}

int text_soft_grain(int x, int y)
{
    const unsigned hash = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    return ((hash >> 7) & 3) == 0;
}
