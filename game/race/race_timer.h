/* The race's timer callback (0043b970, through 0043b969): the input poll
 * every second tick while racing, and the tick count 004bb8d8. */
#ifndef TNFS_RACE_TIMER_H
#define TNFS_RACE_TIMER_H

#include <stdint.h>

typedef struct {
    void *ctx;
    void (*lock)(void *ctx);    /* 00476c78 (cdecl handle): wait for the semaphore 004f8160 */
    void (*poll)(void *ctx);    /* 004394dc */
    void (*unlock)(void *ctx);  /* 00476cfc (cdecl handle): its release */
} RaceTimerOps;

typedef struct {
    int32_t w4fcdac;            /* 004fcdac: set, the timer does nothing */
    int32_t waiting;            /* 004bb8e8: set, the timer does nothing */
    int32_t phase;              /* 004c5390 */
    int32_t ticks;              /* 004bb8d8: counted up here */
} RaceTimerWords;

/* 0043b970 (): nothing while 004fcdac or 004bb8e8 is set; else, holding the
 * semaphore, the poll when phase is 1 and the tick count even (a signed
 * remainder), then the count + 1 unless phase is 3. */
void race_timer_tick(const RaceTimerOps *ops, RaceTimerWords *w);

/* The words 004fcd78..004fcd8f the start and the level set (read nowhere
 * else in the code seen so far) */
typedef struct {
    int32_t unknown_4fcd78;
    int32_t unknown_4fcd7c;
    int32_t unknown_4fcd80;
    int32_t unknown_4fcd84;
    int32_t unknown_4fcd88;
    int32_t unknown_4fcd8c;
} RaceTimerLevel;

/* 0043bb44 (watcall (), from 004275f8) on the level 004fbabc: 0: 004fcd88 4,
 * 004fcd8c 3; 1: 3, 3; 2: 2, 3; 3: 3, 2; 4: 2, 2; 5: 1, 2; else left. */
void race_timer_level(RaceTimerLevel *l, int32_t level);

/* 0043ba6c (watcall (), from 004275f8): 004fcd78 = 004fcd7c = 3, 004fcd88 4,
 * 004fcd8c 3, then 0043bb44's levels; the tick count 004bb8d8 = 0 and
 * 004bb8e4 = 1 (running). */
void race_timer_start(RaceTimerLevel *l, int32_t level, int32_t *ticks, int32_t *running);

/* 0043bbec (watcall (), from 004275f8): 004bb8e4 = 0. */
void race_timer_stop(int32_t *running);

#endif
