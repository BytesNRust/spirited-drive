/* The race's side of the network game: the waits on the other machines and
 * the words they read. The transport itself (004799a8 and on) is not here. */
#ifndef TNFS_RACE_NET_H
#define TNFS_RACE_NET_H

#include <stdint.h>

enum { RACE_NET_NO_DEADLINE = -1 };

typedef struct {
    void *ctx;
    void (*keys)(void *ctx);     /* 0046417c: the keys read again (and the music fed) */
    void (*service)(void *ctx);  /* 00475c14(0) */
} RaceNetOps;

/* The words the waits read; `keys` may change them (the caller's ctx) */
typedef struct {
    int16_t dropped;             /* 0052b314: the network game dropped */
    int32_t deadline;            /* 004f8158: the wait's end on 004bb8e0, -1 none */
    int32_t clock;               /* 004bb8e0: the master timer's count (0043b9e8) */
} RaceNetWords;

/* Two link counts by kind (00442e3c's last argument): the host starts them
 * at 1 / 0 (00442970); a link that fails (00442f58) or ends (004460cc,
 * 004463ec) takes one off */
typedef struct {
    int16_t unknown_4bc6d8;      /* kind != 0 (the entries at 0050a5dc) */
    int16_t unknown_4bc6da;      /* kind 0 (0050a828) */
} RaceNetCounts;

/* 00410bac (watcall ()), one step of a wait: 0046417c; 1 when the game was
 * dropped (0052b314 != 0) or a deadline (not -1) is below the clock
 * (signed); else 00475c14(0) and 0. */
int32_t race_net_wait_step(const RaceNetOps *ops, const RaceNetWords *w);

/* 00442dd8 (watcall (kind)): kind (all 32 bits) != 0: 004bc6d8 - 1, else
 * 004bc6da - 1 (16-bit, wrapping) */
void race_net_count_down(RaceNetCounts *c, int32_t kind);

#endif
