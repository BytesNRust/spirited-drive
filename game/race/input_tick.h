/* Each human's word for the tick taken from its queue (004397f4, from
 * 0044e8f8), waiting on the platform while a slot is not in yet. */
#ifndef TNFS_INPUT_TICK_H
#define TNFS_INPUT_TICK_H

#include "game/race/input_queue.h"
#include "game/race/race_state.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    /* 00439a7c on 004faa7c + player x 0x18c */
    int (*take)(void *ctx, int32_t player, uint32_t *control, int32_t slot);
    void (*music)(void *ctx);           /* 00464240: the music's service */
    void (*service)(void *ctx);         /* 00475c14(0): the platform's */
    void (*notice_slow)(void *ctx);     /* 0041e324: 004f9a98 = 1, 004f9a94 = tick + 120 */
    void (*notice_lost)(void *ctx);     /* 0041e318: 004f9a8c = 1 */
    void (*notice_off)(void *ctx);      /* 0041e340: 004f9a8c = 0, 004f9a94 = tick */
    void (*drop)(void *ctx, int16_t player);  /* 004463ec: a network player dropped */
} InputTickOps;

typedef struct {
    int32_t humans;        /* 004fb7e0 */
    int32_t idle;          /* 004bb8dc: counted up by the timer while the services run */
    int32_t timer_8d8;     /* 004bb8d8: below 500 the idle count is held at 0 */
    int32_t clock;         /* 004bb8e0 */
    int32_t notice_on;     /* 004fb7a8 */
    int32_t notice_until;  /* 004fb7a4 */
    int16_t music_on;      /* 00524e63 */
    int16_t playback;      /* 0052b312 */
    int16_t dropped;       /* 0052b314 */
    int32_t w4fb7d4;       /* 004fb7d4 */
    int32_t w4fbb0c;       /* 004fbb0c */
    int32_t player;        /* 004fbb08 */
    int32_t waiting;       /* 004bb8e8: written */
} InputTickWords;

/* 004397f4 (eax clock): first, with the notice on and 004bb8e0 past its
 * end: notice off. slot = (clock / 2, toward zero) & 63. For each human i
 * (the count read again each time): idle = 0, inputs[i] = i, then take(i)
 * until it gives the word. While it does not: with stand_in[i] and playback
 * 2 or 3 the word is 0xffff87a0; else the music's service (music on) or the
 * platform's, idle held at 0 while 004bb8d8 < 500, past 150 with 004fb7d4
 * 1 the slow notice; past 4000 with 004fb7d4 1: the lost notice (once,
 * until clock + 900), gave_up and the record's finish 999999, then with
 * 004fbb0c 0, or i the followed player, or playback not 1: dropped = 1,
 * waiting = 1, return; else drop(i). Then dropped set: waiting = 1, return.
 * With the word: gear up and down both held (0x3000) are cleared with the
 * high half (& 0xcfff) and launch_ready[i] = 1; else launch_ready[i] = 0. */
void input_tick(const InputTickOps *ops, InputTickWords *w, int32_t clock, InputQueue *queues, uint32_t *inputs,
                const int16_t *stand_in, PlayerRecord *records, int32_t *launch_ready);

#endif
