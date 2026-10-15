/* The cars entering the race (00440858, from 00455a5c): a car made for
 * each slot, entered, marked as a racer or the police by its place, its
 * model and body loaded, then the order sorted. */
#ifndef TNFS_RACE_ENTER_H
#define TNFS_RACE_ENTER_H

#include "game/race/race_state.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*tables)(void *ctx);                      /* 0043f994 */
    void (*load_pick)(void *ctx, int32_t first);    /* 004571e8 */
    Car *(*alloc)(void *ctx, int32_t size);         /* 00474c8c("..." 004afd9c, 0x79a, 0) */
    void (*enter)(void *ctx, Car *car);             /* 004406d0 */
    void (*load)(void *ctx, Car *car);              /* 00457d40 */
    void (*body)(void *ctx, Car *car, int32_t slot);    /* 004407b8 */
    void (*sort)(void *ctx);                        /* 00440dd8 */
} RaceEnterOps;

/* 00440858 (watcall ()): tables; for each slot i below the car count
 * (004fb7e8, read each turn): load_pick(0) before slot 2; a car (0x79a)
 * with index (+0x516) and player (+0x47d) i in cars[i], by_node[i] =
 * by_progress[i] = i; enter; flags_51a | 4, then & ~8; a computer racer
 * (humans <= i < racers) flags_174 | 4, the police (racers <= i < racers +
 * cops) | 8; load; body(car, i). Then each car's +0x39d 0, and sort. */
void race_cars_enter(const RaceEnterOps *ops, RaceState *race);

#endif
