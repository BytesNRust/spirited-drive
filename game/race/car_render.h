/* A car in the view (004591d4, from 0045a848 and the mirror's 0045a8fc):
 * near or far model by its camera-space distance, its body matrix through
 * the camera into 0051c774, 00458d4c, then the followed car's crash trail
 * or another racer's mark. */
#ifndef TNFS_CAR_RENDER_H
#define TNFS_CAR_RENDER_H

#include "game/race/car_struct.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*body)(void *ctx, Car *car, const int32_t matrix[9], const int32_t position[3]);  /* 00458d4c(car, 0051c774, +0x391) */
    void (*near)(void *ctx, uint32_t object, int on);                    /* the object's +0x10 bit 1 */
    void (*vertex)(void *ctx, uint32_t object, int32_t index, int32_t out[2]);  /* the object's +0x7c points (3 words each) */
    void (*point)(void *ctx, Car *car, int32_t camera[3], int32_t screen[2]);  /* 0044a1c4 */
    void (*trail)(void *ctx, const int32_t offset[2]);                   /* 00456f34(005146ac, offset) */
    void (*tag)(void *ctx, int32_t x, int32_t y, int32_t slot, int32_t z);  /* 00459064 */
} CarRenderOps;

typedef struct {
    const int32_t *camera_matrix;   /* *00511fe4 */
    int32_t followed;               /* 004fbb08 */
    int16_t replay;                 /* 0052b316 */
    int32_t race_type;              /* 004fb7d0 */
    int32_t racers;                 /* 004fb7dc */
    int32_t high;                   /* 004fa0c0 */
    int32_t port_reach;             /* port: 0 the exe's culls; else how far the car's model reaches from its centre
                                     * (its units): drawn while any of it can be in front of the eye, whatever its side
                                     * (a field wider than the exe's cone), its mark only with its centre in front; the
                                     * followed car always as the exe (the bumper view's eye is inside it) */
} CarRenderInputs;

/* 004591d4 (watcall (car, mirror)): z = +0x399; nothing unless 0x20 <= z
 * <= 0x5a00 and |+0x391| <= 2z. Mirror: nothing past 0x2100, far (+0x78e)
 * past 0xa00; else far past 0x1900 unless slot (+0x516) 2. The body matrix:
 * +0x24 with +0x20 set, else x(0042ecd0(+0x42d, +0x428) + pitch + +0x369 >>
 * 14) z(0042ecd0(+0x429, +0x427) + roll + +0x365 - +0x365 / 4 >> 14), z x x
 * then x y(+0x351 >> 14); 0051c774 = it x the camera's. Both objects near
 * (+0x10 bit 1) below z 0x219. 00458d4c(car, 0051c774, +0x391). The
 * followed slot without a replay: with +0x175 bit 1 the crash trail
 * (00456f34) offset by the 'smok' point (+0x616) less the car's place on
 * screen (0044a1c4), or 0 for the far model. Else with race type >= 1 and
 * slot < racers: the mark (00459064) at the
 * car's place, raised by 0x3800 / z + 15 (high) or 0x2000 / z + 7. */
void car_render(const CarRenderOps *ops, const CarRenderInputs *in, Car *car, int32_t mirror, int32_t world[9]);

#endif
