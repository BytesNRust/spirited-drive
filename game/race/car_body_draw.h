/* A car's 3D object drawn at its place (00458d4c, from 004591d4): the far
 * model whole, or the near one in its four parts with the followed car's
 * wheels, smoke and the lights in the palette. */
#ifndef TNFS_CAR_BODY_DRAW_H
#define TNFS_CAR_BODY_DRAW_H

#include "game/race/car_lights.h"
#include "game/race/car_struct.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*eye)(void *ctx, const int32_t eye[3]);                                   /* 0049163e */
    void (*matrix)(void *ctx, uint32_t object, const void *matrix);                 /* 0048da62 */
    void (*transform)(void *ctx, uint32_t object);                                  /* 00491737 */
    void (*project)(void *ctx, uint32_t object);                                    /* 00491690 */
    void (*sort)(void *ctx, uint32_t object);                                       /* 004919b4 */
    void (*draw)(void *ctx, uint32_t object);                                       /* 00491d35 */
    void (*sort_list)(void *ctx, uint32_t object, int32_t count, uint32_t list);    /* 004918e1 */
    void (*draw_list)(void *ctx, uint32_t object, int32_t count, uint32_t list);    /* 00491c8a */
    void (*wheel_frames)(void *ctx, Car *car);                                      /* 00458614 */
    void (*steer)(void *ctx, Car *car);                                             /* 00458818 */
    void (*smoke)(void *ctx, Car *car, int32_t in_car);                             /* 00458988 */
    void (*palette)(void *ctx, int32_t first, int32_t count, const uint8_t *colours);  /* 00476460 */
} CarBodyOps;

/* 00458d4c (watcall (car, matrix, position)): 0049163e(-position). The far
 * model (+0x78e set): 0048da62(+0x666, matrix), 00491737, 00491690,
 * 004919b4, 00491d35 on it. Else: the followed car (00509e1c[004fbb08])
 * gets 00458614 and 00458818; the lights (car_lights); 0048da62(+0x52e,
 * matrix), 00491737, 00491690 on it; the followed car 00458988(car, 1);
 * the parts inside, surface, outside, convex in that order, each below 8
 * (unsigned): 004918e1 and 00491c8a(+0x52e, count, list); the followed car
 * 00458988(car, 0). */
void car_body_draw(const CarBodyOps *ops, const CarLightInputs *lights, Car *car, const Car *followed,
                   const void *matrix, const int32_t position[3]);

#endif
