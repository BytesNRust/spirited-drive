/* The race's two deferred calls (004f8100, 004b90dc): the in-race menu
 * (00447564, Esc) and the pause (00455600, P), each due at a frame. */
#ifndef TNFS_RACE_SCHEDULE_H
#define TNFS_RACE_SCHEDULE_H

#include "game/race/input_queue.h"

#include <stdint.h>

enum {
    RACE_SCHEDULE_MENU = 0,     /* 004b90dc[0]: 00447564 */
    RACE_SCHEDULE_PAUSE = 1,    /* 004b90dc[1]: 00455600 */
    RACE_SCHEDULE_RUN = 2,      /* the entries 00410b04 looks at */
    RACE_SCHEDULE_SLOTS = 3     /* the entries 00410a70 sets (004b90dc[2] is 0) */
};

typedef struct {
    int16_t frame;              /* 004f8100 + 4 x i: due at this frame, 0 none */
    int16_t arg;                /* 004f8102 + 4 x i: set by 00410a70, not read by 00410b04 */
} RaceScheduled;

typedef struct {
    void *ctx;
    void (*call)(void *ctx, int which);  /* 004b90dc[which] (both keep every register) */
} RaceScheduleOps;

typedef struct {
    int32_t w4fb7d4;            /* 004fb7d4: 1 sends the entry to the other machine */
    int32_t w4fbb0c;            /* 004fbb0c */
    int16_t playback;           /* 0052b312 */
} RaceScheduleWords;

/* 00410a70 (watcall (frame, which, arg), from 004557f4): which 0..2 (signed),
 * else nothing: the entry's frame and arg set. With 004fb7d4 == 1: 00445e00
 * (0, {3, which, arg, frame + 8}, 8), then the entry's frame + 600 when
 * 004fbb0c is 0 and 0052b312 is 1, else frame + 8 (the message's word read
 * back). Shorts throughout. */
void race_schedule_set(const InputSendOps *send, RaceScheduled sched[RACE_SCHEDULE_SLOTS], const RaceScheduleWords *w,
                       int16_t frame, int32_t which, int16_t arg);

/* 00410b04 (watcall (frame), from 0045597c each frame): for the menu and
 * then the pause, when frame (a short) is not 0 and the entry's frame is not
 * 0 and at most frame (signed): its call, then its frame = 0. The 1 left in
 * eax is not read by 0045597c. */
void race_schedule_run(const RaceScheduleOps *ops, RaceScheduled sched[RACE_SCHEDULE_SLOTS], int16_t frame);

/* 00410b5c (watcall (), from 004275f8 before a race): the menu's and the
 * pause's entries (frame and arg) = 0; the third is left. */
void race_schedule_clear(RaceScheduled sched[RACE_SCHEDULE_SLOTS]);

/* Read again after the send (the ctx may change them) */
typedef struct {
    int16_t playback;           /* 0052b312 */
    int32_t w4fbb0c;            /* 004fbb0c */
    int16_t clock;              /* 004bb8d8 (its low word) */
} RaceScheduleInWords;

/* 004109e4 (004bcb98[3], message kind 3: eax link, edx data, the message
 * 00410a70 sends): with playback 1 and 004fbb0c, the message's frame (+6)
 * becomes 1 in place. The entry (the short at +2) takes frame +6 and arg +4.
 * With playback 1 and 004fbb0c again: the 8 bytes relayed (00445e00(0, copy,
 * 8)) and the entry's frame = 004bb8d8 + 8 (read after). Port guard: an entry
 * outside 0..2 stores nothing (the original writes past 004f8100). */
void race_schedule_on_message(const InputSendOps *send, RaceScheduled sched[RACE_SCHEDULE_SLOTS],
                              const RaceScheduleInWords *w, uint8_t *data);

typedef struct {
    void *ctx;
    int32_t (*notice_up)(void *ctx);                                     /* 0041e38c */
    void (*set)(void *ctx, int16_t frame, int32_t which, int16_t arg);   /* 00410a70 */
    void (*detail)(void *ctx, int32_t segments);                         /* 0044c1bc */
} RaceMenuKeysOps;

typedef struct {
    int32_t tick;               /* 00513e70 */
    int32_t phase;              /* 004c5390 */
    uint8_t esc, pause;         /* 004c693d, 004c6955: keys 1 and 0x19 */
    uint8_t lshift, rshift;     /* 004c6966, 004c6972: keys 0x2a and 0x36 */
    int16_t playback;           /* 0052b312 */
    int16_t replay;             /* 0052b316 */
    uint8_t detail;             /* 004fb785 */
} RaceMenuKeysInputs;

typedef struct {
    int32_t pending;            /* 00513e50: a call scheduled, until the tick is below 0x3c again */
    int32_t last;               /* 00513e54: the tick it was scheduled at */
    int16_t dropped;            /* 0052b314: written 1 */
    int32_t w509d70;            /* 00509d70: written 2 */
} RaceMenuKeysState;

/* 004557f4 (watcall (frame), from 0045597c each frame): tick < 0x3c: pending
 * and last = 0. Esc with phase 1, tick > 0x78 and nothing pending: during
 * playback with a shift held, dropped = 1 and 00509d70 = 2; else once tick >=
 * last + 0xb4: last = tick, and without 0052b316 the menu at frame + 1
 * (00410a70(frame + 1, 0, 1)), pending = 1 and 0044c1bc(0x1c / 0xe / 9) for
 * detail 0 / 1 / 2. Otherwise, without playback, P with tick > 0x78, nothing
 * pending, no stop notice (0041e38c) and tick >= last + 0x78: last = tick,
 * the pause at frame + 1 (00410a70(frame + 1, 1, 1)), pending = 1. */
void race_menu_keys(const RaceMenuKeysOps *ops, const RaceMenuKeysInputs *in, RaceMenuKeysState *st, int16_t frame);
/* 004557e8 (the in-race menu's way out): last = tick, so ESC is dead for 0xb4 ticks. port_resume_keys 1 clears it
 * instead, as the pause does, and ESC opens the menu again at once. */
void race_menu_keys_resumed(RaceMenuKeysState *st, int32_t tick, int32_t port_resume_keys);

typedef struct {
    void *ctx;
    void (*sound_pause)(void *ctx);                  /* 0041eba4 */
    void (*voices_off)(void *ctx);                   /* 004437c8 */
    void (*end_43cbcc)(void *ctx);                   /* 0043cbcc */
    void (*movie)(void *ctx);                        /* 00464240 */
    void (*service)(void *ctx);                      /* 00475c14(0) */
    void (*hud_clear)(void *ctx);                    /* 0041e364 */
    void (*take)(void *ctx, int32_t kind);           /* 00405b00 on the follow record 004f801c */
    void (*wait_screen)(void *ctx);                  /* 0041ec0c */
    void (*net_send)(void *ctx, int32_t to);         /* 00410b7c */
    int32_t (*net_wait)(void *ctx, int32_t kind);    /* 00410c18 */
    void (*music_on)(void *ctx);                     /* 004437f0 */
    void (*setup_43cbe0)(void *ctx);                 /* 0043cbe0 */
} RacePauseOps;

/* Read again after every call (the services move the key and 00524e63 on). */
typedef struct {
    uint8_t pause;              /* 004c6955: key 0x19 */
    int16_t movie;              /* 00524e63: 00464240 rather than 00475c14(0) */
    int32_t follow_kind;        /* 004f801c */
    int32_t w4fb7d4;            /* 004fb7d4 */
    int16_t w524e9f;            /* 00524e9f */
    int32_t tick;               /* 00513e70 */
    int32_t port_resume_keys;   /* port: 1 clears `last` on the way out, so ESC works at once; 0 the exe's (the tick:
                                 * ESC dead for MENU_GAP ticks after the pause) */
} RacePauseInputs;

/* 00455600 (004b90dc[1], from 00410b04): 004fcdac = 1; 0041eba4, 004437c8,
 * 0043cbcc; served (00464240 while 00524e63, else 00475c14(0)) while P is
 * down, then until it is down again; 0041e364 and 00405b00(004f801c,
 * [004f801c]); served while P is down; with 004fb7d4 1 and 00524e9f 0:
 * 0041ec0c, 00410b7c(-1), 00410c18(5); 004437f0; pending 0 and last the
 * tick; 0043cbe0; 004fcdac = 0. */
void race_pause(const RacePauseOps *ops, const RacePauseInputs *in, RaceMenuKeysState *st, int32_t *loading);

#endif
