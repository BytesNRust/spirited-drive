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

/* 0045b778: a 16.16 speed as 00466c9c's English one without the unit
 * ("%d.%d"), in either language. */
void stat_speed_number_text(char out[STAT_TEXT], int32_t speed);

/* 0045b7e8: a distance d as d x 3000 / 12000 (the product wrapping at
 * 32 bits) metres, "%d M" in German, or that x 33 / 10 feet ("%d Ft");
 * "N/A" when d is below 100 or 999. */
void stat_distance_text(char out[STAT_TEXT], int german, int32_t distance);

/* 0045b878: as 0045b7e8 with d x 3857 / 12000 metres. */
void stat_distance_long_text(char out[STAT_TEXT], int german, int32_t distance);

/* 00466d5c: a time in ticks of 1/60 s as "%d:%02d.%d" (minutes, seconds,
 * tenths) from a minute on, "%d.%d" (seconds, tenths) under it, and "N/A"
 * when it is 0 or less, or 99999 or 999999 (no time). */
void stat_time_text(char out[STAT_TEXT], int32_t ticks);

/* 00466e5c: an average speed, distance over ticks, as distance x 1344 /
 * ticks tenths of mph ("%d.%dmph"), or x 2160 in German ("%d.%d" and the
 * km/h sign); the product wraps at 32 bits. "N/A" when either is 0 or
 * less, or the ticks are 99999 or 999999 (no time). */
void stat_average_speed_text(char out[STAT_TEXT], int german, int32_t ticks, int32_t distance);

#endif
