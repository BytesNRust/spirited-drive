/* A car's place in camera space and its shadow's corners on the screen
 * (0045a3e8, from 0045a724 before 004591d4 draws the car). */
#ifndef TNFS_CAR_SHADOW_H
#define TNFS_CAR_SHADOW_H

#include "game/race/car_struct.h"
#include "game/race/model_draw.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*road_contact)(void *ctx, Car *car);  /* 00428918(car, 1): the road plane under it (car_road_contact) */
    /* 004596a8 (car; forward, side, point) for the followed car */
    void (*followed)(void *ctx, Car *car, const int32_t forward[3], const int32_t side[3], const int32_t point[3]);
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

#endif
