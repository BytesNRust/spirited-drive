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

/* 00466d5c */
void stat_time_text(char out[STAT_TEXT], int32_t ticks)
{
    enum { SECOND = 60, MINUTE = 60 * SECOND, TENTH = SECOND / 10, NO_TIME = 999999, NO_TIME_2 = 99999 };
    if (ticks <= 0 || ticks == NO_TIME || ticks == NO_TIME_2) {
        snprintf(out, STAT_TEXT, "N/A");
        return;
    }
    const int32_t minutes = ticks / MINUTE, left = ticks - minutes * MINUTE;
    const int32_t seconds = left / SECOND, tenths = (left - seconds * SECOND) / TENTH;
    if (ticks >= MINUTE) snprintf(out, STAT_TEXT, "%d:%02d.%d", minutes, seconds, tenths);
    else snprintf(out, STAT_TEXT, "%d.%d", seconds, tenths);
}
