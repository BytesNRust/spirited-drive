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

/* 0045b778 */
void stat_speed_number_text(char out[STAT_TEXT], int32_t speed)
{
    enum { SLOWEST = 0x9fff6 };
    if (speed <= SLOWEST) snprintf(out, STAT_TEXT, "N/A");
    else snprintf(out, STAT_TEXT, "%d.%d", (speed >> 16) * 224 / 100 + 1, (int32_t)(((uint32_t)speed & 0xffff) * 10 >> 16));
}

/* 0045b7e8 */
void stat_distance_text(char out[STAT_TEXT], int german, int32_t distance)
{
    enum { SHORTEST = 100, NONE = 999, SCALE = 3000, DIVISOR = 12000 };
    const int32_t metres = (int32_t)((uint32_t)distance * SCALE) / DIVISOR;
    if (distance < SHORTEST || distance == NONE) snprintf(out, STAT_TEXT, "N/A");
    else if (german) snprintf(out, STAT_TEXT, "%d M", metres);
    else snprintf(out, STAT_TEXT, "%d Ft", metres * 33 / 10);
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

/* 00466e5c */
void stat_average_speed_text(char out[STAT_TEXT], int german, int32_t ticks, int32_t distance)
{
    enum { NO_TIME = 999999, NO_TIME_2 = 99999, TENTHS_MPH = 1344, TENTHS_KMH = 2160 };
    if (ticks <= 0 || distance <= 0 || ticks == NO_TIME_2 || ticks == NO_TIME) {
        snprintf(out, STAT_TEXT, "N/A");
        return;
    }
    const int32_t tenths = (int32_t)((uint32_t)distance * (german ? TENTHS_KMH : TENTHS_MPH)) / ticks;
    if (german) snprintf(out, STAT_TEXT, "%d.%d%c", tenths / 10, tenths % 10, STAT_KMH);
    else snprintf(out, STAT_TEXT, "%d.%dmph", tenths / 10, tenths % 10);
}
