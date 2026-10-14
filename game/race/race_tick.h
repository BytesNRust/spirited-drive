/* The race's tick (00440ef0, from 0045597c while the race runs): the
 * controllers, the finishes and the race's end, the physics, the order of
 * the cars, the split times, the camera, the player's sounds and the cars'
 * draw state. */
#ifndef TNFS_RACE_TICK_H
#define TNFS_RACE_TICK_H

#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*replay)(void *ctx);                                  /* 00450020 */
    void (*controller)(void *ctx, Car *car, int32_t kind);      /* 00509dd4[kind] (eax the car) */
    void (*window)(void *ctx, int32_t *first, int32_t *size);   /* 004054cc */
    void (*window_set)(void *ctx, int32_t size);                /* 00405370 */
    void (*finish)(void *ctx, Car *car);                        /* 00440c30 */
    void (*chase_take)(void *ctx, Car *car);                    /* 00404530(004f801c, car, 3) */
    void (*engines_start)(void *ctx);                           /* 00444394 */
    void (*music)(void *ctx);                                   /* 00464240 */
    void (*service)(void *ctx);                                 /* 00475c14(0) */
    void (*physics)(void *ctx, Car *car, int32_t kind);         /* 00509dac[kind] (eax the car) */
    void (*lateral)(void *ctx, Car *car);                       /* 00440a68 */
    void (*sort)(void *ctx);                                    /* 00440dd8 */
    void (*contacts)(void *ctx);                                /* 0040d8e8 */
    void (*splits_pass)(void *ctx, int32_t mine, int32_t rival);  /* 0045b24c */
    void (*splits_gap)(void *ctx);                              /* 0045b1d0 */
    void (*camera_pick)(void *ctx, Car *car);                   /* 0045573c */
    void (*chase_tick)(void *ctx);                              /* 004046b0(00511fe0) */
    void (*platform)(void *ctx);                                /* 00476f94 */
    void (*scrape)(void *ctx, Car *car);                        /* 0043ce88 (0043ce74 after it is a bare ret) */
    void (*force)(void *ctx, Car *car);                         /* 0043ce08 */
    void (*sound_held)(void *ctx);                              /* 00472e34 */
    void (*sound_follow)(void *ctx);                            /* 004723b8 */
    void (*sound_472217c)(void *ctx);                           /* 0047217c */
    void (*sound_472348)(void *ctx);                            /* 00472348, with two humans or more */
    void (*sound_472560)(void *ctx);                            /* 00472560 */
} RaceTickOps;

typedef struct {
    int16_t dropped;            /* 0052b314: the network game dropped */
    uint8_t screen_kind;        /* 004fb789: 4 and 5 size the window by speed */
    int32_t high;               /* 004fa0c0 */
    int16_t music_on;           /* 00524e63 */
    int32_t wait_ticks;         /* 004fbac8 */
    const int32_t *timer;       /* 004bb8d8: the timer the services move on */
    int32_t split_views[2];     /* 0052b8d4, 0052b8d8: written when the view moves on */
} RaceTickWords;

/* 00440ef0 (watcall ()). See race_tick.c for the steps. */
void race_tick(const RaceTickOps *ops, RaceState *race, RaceTickWords *w);

#endif
