#include "game/frontend/tgv.h"

/* 004a2b90 */
uint32_t tgv_shape_type(uint32_t bits)
{
    switch (bits) {
    case 1: return 0x79;
    case 4: return 0x7a;
    case 8: return 0x7b;
    case 15: return 0x7e;
    case 16: return 0x78;
    case 24: return 0x7f;
    case 32: return 0x7d;
    default: return 0;
    }
}
