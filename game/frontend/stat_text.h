/* The texts of the race's figures on the menus' screens: speeds and times. */
#ifndef TNFS_STAT_TEXT_H
#define TNFS_STAT_TEXT_H

#include <stdint.h>

enum {
    STAT_TEXT = 0x18,  /* the longest text and its NUL fit */
    STAT_KMH = 0x9b    /* the font's km/h sign */
};

/* 00466c9c: a 16.16 speed as "N/A" when it is at most 0x9fff6, else its
 * whole part x 2.24 ("%d.%dmph") or x 3.6 in German ("%d.%d" and the km/h
 * sign), rounded up by 1, with the first decimal of the speed's own
 * fraction. */
void stat_speed_text(char out[STAT_TEXT], int german, int32_t speed);

/* 00466d5c: a time in ticks of 1/60 s as "%d:%02d.%d" (minutes, seconds,
 * tenths) from a minute on, "%d.%d" (seconds, tenths) under it, and "N/A"
 * when it is 0 or less, or 99999 or 999999 (no time). */
void stat_time_text(char out[STAT_TEXT], int32_t ticks);

#endif
