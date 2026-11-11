/* A car's place in camera space and its shadow's corners on the screen
 * (0045a3e8, from 0045a724 before 004591d4 draws the car). */
#ifndef TNFS_CAR_SHADOW_H
#define TNFS_CAR_SHADOW_H

#include "game/gfx/quad.h"
#include "game/race/car_struct.h"
#include "game/race/model_draw.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*road_contact)(void *ctx, Car *car);  /* 00428918(car, 1): the road plane under it (car_road_contact) */
    /* 004596a8 (car; forward, side, point) for the followed car */
    void (*followed)(void *ctx, Car *car, const int32_t forward[3], const int32_t side[3], const int32_t point[3]);
    /* port (car_soft_shadow; NULL the exe's): the box's corners nearer than the view's plane are slid along its edges to it (follow_arm_clip_quad's contract)
     * instead of projected to INT32_MIN, which lost the whole shadow of a car whose centre passes the culls with its back corners behind the lens,
     * and a car whose centre is nearer than 0x20 keeps the front of its box */
    int (*clip_quad)(const int32_t *const in[4], int32_t plane, int32_t out[4][3]);
    /* port (car_soft_shadow, with clip_quad): the exe's 45 degree cone (|x| <= z of the car's centre) is the view's own frustum, tested on the box's cut
     * corners (a car in a wide field, or just behind the lens, keeps its shadow); 0 the exe's cone */
    int frustum;
} CarShadowOps;

/* 0045a3e8 (watcall (car, pass)): car +0x391 = the car's place in camera
 * space ((position - camera) >> 9 through the matrix). Pass non-zero (the
 * mirror's) returns 0 there. Else 0 unless 0x20 <= z <= 0x1900 and |x| <= z;
 * then 00428918, and the four corners of the box half forward (+0x36d) and
 * half wide (+0x379), each >> 10, about the road point (+0x359): -f + s,
 * -f - s, f - s, f + s, each turned by the matrix, moved by the point's
 * camera place and projected (00475eac) into `screen` (0051c770's 4 x 2
 * words); the followed car then gets 004596a8; 1. */
int car_shadow(const CarShadowOps *ops, Car *car, const ModelView *mv, int pass, int followed, int32_t screen[8]);

/* 0045a768 (watcall (car)): the followed car when it is not drawn (0045a848,
 * the in-car view): 00428918, then 004596a8 with the box >> 10 and the road
 * point, as 0045a3e8 gives them. */
void car_skid_unseen(const CarShadowOps *ops, Car *car);

/* 0045a724 (watcall (car, pass)): a car with a body matrix (+0x20) gets
 * its heading back from it (+0x351 = 0042ebbc(m[8], m[6]): atan2(m[6],
 * m[8]) << 8); then 0045a3e8 and, when it gives 1, the shadow darkened
 * (00482794: 4 points, the 004c6604 remap). Returns 0045a3e8's answer. */
int car_prepare(const CarShadowOps *ops, Car *car, const ModelView *mv, int pass, int followed, const QuadTarget *target,
                const uint8_t *remap, int32_t screen[8]);

#endif
