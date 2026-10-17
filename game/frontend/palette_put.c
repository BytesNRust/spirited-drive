#include "game/frontend/palette_put.h"

#include <string.h>

/* 00463df0 */
void palette_put_clear(uint8_t pal[PALETTE_PUT_BYTES])
{
    memset(pal, 0, PALETTE_PUT_BYTES);
}

/* 00463e08 */
void palette_put(uint8_t pal[PALETTE_PUT_BYTES], int first, int count, const uint8_t *src, int mode,
                 uint8_t screen[PALETTE_PUT_BYTES])
{
    enum { RAW = 0x24 };
    uint8_t *to = pal + 3 * (int16_t)first;
    const int bytes = 3 * (int16_t)count;
    for (int i = 0; i < bytes; i++) to[i] = (uint16_t)mode == RAW ? src[i] : (uint8_t)(src[i] << 2);
    memcpy(screen, pal, PALETTE_PUT_BYTES);
}
