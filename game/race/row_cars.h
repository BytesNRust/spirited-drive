/* The cars by road row (00459d70 before the track, 0045a848 between the
 * rows): which rows ahead of the camera hold a car, and the cars of a row. */
#ifndef TNFS_ROW_CARS_H
#define TNFS_ROW_CARS_H

#include "game/race/car_struct.h"
#include "game/race/mirror_scene.h"

#include <stdint.h>

enum { ROW_CARS_AHEAD = 0x23 };

/* 00459d70 (watcall (camera node), from 0044ce3f): `ahead` (0051c7ac,
 * ROW_CARS_AHEAD) cleared; each of the `count` cars (00509e1c, 004fb7e8) not
 * physics 6 moves its draw_node (+0x341) back by one when its lateral
 * (+0x39d) is below 0x29999, on by the ring (0050420c x 4) when that is
 * negative; the row it is ahead of the camera, under 0x23, is set. Then
 * 00459cc4 (mirror_car_rows) into `behind`. */
void car_rows_ahead(Car *const *cars, int32_t count, int32_t segments, int32_t camera_node,
                    uint8_t ahead[ROW_CARS_AHEAD], uint8_t behind[MIRROR_CAR_ROWS]);

typedef struct {
    void *ctx;
    void (*prepare)(void *ctx, Car *car, int pass);  /* 0045a724 */
    void (*draw)(void *ctx, Car *car, int pass);     /* 004591d4 */
    void (*unseen)(void *ctx, Car *car);             /* 0045a768 */
} RowCarsOps;

typedef struct {
    int32_t camera_node;        /* chase +0x38 */
    int32_t segments;           /* 0050420c */
    const uint8_t *ahead;       /* 0051c7ac: ROW_CARS_AHEAD flags (car_rows_ahead) */
    Car *const *list;           /* 00509e8c: the cars to draw */
    int32_t count;              /* 00509e14 */
    const Car *followed;        /* cars[004fbb08] */
    int32_t chase_kind;         /* 004f801c */
    uint8_t alternate;          /* 004fb787 */
} RowCarsInputs;

/* 0045a848 (watcall (row), between the track's rows): nothing unless the row
 * is 0..0x22 ahead of the camera (round the ring) with its flag set; then the
 * listed cars, last first, not physics 6 and with draw_node (+0x341) the
 * row: the followed car while the chase kind is 0 (in the car) only lays
 * its skid marks (0045a768, not when alternate), the others 0045a724 and
 * 004591d4, pass 0. */
void cars_on_row(const RowCarsOps *ops, const RowCarsInputs *in, int32_t row);

#endif
