#include "game/frontend/stat_text.h"

#include <stdio.h>

/* 00466c9c */
void stat_speed_text(char out[STAT_TEXT], int german, int32_t speed)
{
    enum { SLOWEST = 0x9fff6 };
    const int32_t tenths = (int32_t)(((uint32_t)speed & 0xffff) * 10 >> 16);
    const int32_t whole = speed >> 16;
    if (speed <= SLOWEST) snprintf(out, STAT_TEXT, "N/A");
    else if (german) snprintf(out, STAT_TEXT, "%d.%d%c", whole * 360 / 100 + 1, tenths, STAT_KMH);
    else snprintf(out, STAT_TEXT, "%d.%dmph", whole * 224 / 100 + 1, tenths);
}
