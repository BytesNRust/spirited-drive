#include "game/frontend/palette_put.h"

#include <string.h>

/* 00463df0 */
void palette_put_clear(uint8_t pal[PALETTE_PUT_BYTES])
{
    memset(pal, 0, PALETTE_PUT_BYTES);
}
