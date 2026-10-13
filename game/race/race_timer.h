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

#endif
