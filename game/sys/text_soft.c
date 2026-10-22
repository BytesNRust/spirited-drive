/* port: the soft look of the port's large text (text_soft.h). */
#include "game/sys/text_soft.h"

enum { MAX_LEVEL = 15, KERNEL_SUM = 36 };

static const int KERNEL[3] = { 1, 4, 1 };

void text_soft_shadow(const uint8_t *in, uint8_t *out, int w, int h)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (x + dx >= 0 && x + dx < w && y + dy >= 0 && y + dy < h)
                        sum += in[(y + dy) * w + x + dx] * KERNEL[dx + 1] * KERNEL[dy + 1];
            out[y * w + x] = (uint8_t)((sum * TEXT_SOFT_SHADOW_PERCENT + KERNEL_SUM * 100 / 2) / (KERNEL_SUM * 100));
        }
}

int text_soft_lift(int level)
{
    const int sharp = (level - TEXT_SOFT_EDGE_FROM) * MAX_LEVEL / TEXT_SOFT_EDGE_SPAN;
    return sharp < 0 ? 0 : sharp > MAX_LEVEL ? MAX_LEVEL : sharp;
}

