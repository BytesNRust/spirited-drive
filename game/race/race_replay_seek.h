/* The replay's seek to a minute (0044e354): the race's state as it was at
 * that minute's mark, then the cars from the recording. */
#ifndef TNFS_RACE_REPLAY_SEEK_H
#define TNFS_RACE_REPLAY_SEEK_H

#include "game/race/race_state.h"

#include <stdint.h>

enum {
    REPLAY_MARK = 0xa2,              /* a mark's bytes (005120d8) */
    REPLAY_MARK_FIRST = 0x10,        /* the marks after the sixteen minutes */
    REPLAY_MARK_COUNT = 4,           /* 00512f84's ticks */
    REPLAY_MARKS = REPLAY_MARK_FIRST + REPLAY_MARK_COUNT
};

typedef struct {
    void *ctx;
    void (*restore_car)(void *ctx, int32_t human, int32_t minute, int32_t index);  /* 0044df20 */
} RaceReplaySeekOps;

/* 0044e354 (eax clock, edx mark; both callers, 0044eb0f and 0044ece8, pass
 * 00512d90 and -1). mark >= 0: the tick from 00512f84[mark], the tick
 * flags from it (1 on even ticks, 4 every 4th, 0x10 every 8th), the
 * minute 0x10 + mark; else the minute clock / 0x708. Then 00450250,
 * 004501a8(tick / 0x708); from the minute's mark: by_node and by_progress
 * for the cars (004fb7e8), the other cars' count (+0x50), 004b9628,
 * 0052b694, 0052b66c (byte +0x99), 004c5fa0, 004c5cd8, 004c5d40,
 * 0052b330, 0052b334, 0052b680, 00509e10, 004b9624 (+0x98 & 1), 0052b478
 * (24 bytes); each car's +0x4ee and +0x4ea zeroed; 004f80fc, 0051c970,
 * 0051c974. Each human's car back (0044df20(1, minute, i); 0051372c = 1
 * first with a mark), the other cars' (0044df20(0, minute, i)); 0051372c
 * = 0. With a mark: racing (004c5390 1): 004501fc; 004c5390 = 2, 00513728
 * = 1. Not racing: the camera follows the player's car in the record's
 * next kind (+4), 7 only while 0043f8f8(0, 5) (slot 2's car, else the
 * player's in kind 5) and 6 while 0043f948(0, 5) (the other of slots 0
 * and 1, else as 7). 00513734 set: cleared, 00443820, then 00444394 when
 * the player's car is in physics 1 or 2, else 0044435c. */
void race_replay_seek(RaceState *race, const RaceReplaySeekOps *ops, int32_t clock, int32_t mark);

/* 0044eb34 (eax a cut's word, 00512d9c + 12 i; from 0044ed3f in 0044ecc4): the
 * replay's camera. 0043f8f8(0, 5) (slot 2's car just behind) and
 * 0043f948(0, 5) (the other of slots 0 and 1): both: race_rand_below(10)
 * < 6 picks 6, else 7; one: that one; 6 follows the other of slots 0 and
 * 1, 7 slot 2's car. Neither: the player's car in 2 when the word is
 * 0xd6..0xf7 and race_rand_below(10) >= 8, else 0..4 (race_rand_below(5))
 * for 0xdb..0xe3, else 1..4. The kind to the record's next kind (004f8020),
 * the camera follows; 00512f78 >= 0 and the kind above 0 draw one more
 * race_rand_below(10), unused; 004c53a8 = 0. */
void race_replay_camera(RaceState *race, int32_t word);

typedef struct {
    void *ctx;
    void (*seek)(void *ctx, int32_t clock, int32_t mark);   /* 0044e354 */
    void (*play)(void *ctx);                                /* 0044dbc8 */
    void (*camera)(void *ctx, int32_t word);                /* 0044eb34 */
    void (*cycle)(void *ctx);                               /* 0040594c(004f801c) */
} RaceReplayStepOps;

/* 0044ecc4 (no arguments; from 0044fc69, 0044fe18, 0044fe4e, 0044ff1e,
 * 0044ffa9, 00450153): a replay's frame. On a minute (00512d90 % 0x708 0)
 * 0044e354(clock, -1); 0044dbc8. 00513728 set (a seek happened):
 * 004c538c = 0, the camera follows the player's car in the record's next
 * kind, 00512f78 >= -1: 0044eb34(cut 00512f78's word); 004c5370 and
 * 00513728 = 0; 00512f7c set: cleared, and with 004fbb0c and playback not
 * 1 the tick to 00512f84; 00443820; 00444394 for the player's car in
 * physics 1 or 2, else 0044435c. Then 0051371c set and the tick's low
 * byte 0xfc: 0040594c. */
void race_replay_step(RaceState *race, const RaceReplayStepOps *ops);

typedef struct {
    void *ctx;
    void (*input_tick)(void *ctx, int32_t clock);               /* 004397f4 */
    void (*record)(void *ctx);                                  /* 0044db34 */
    void (*save_car)(void *ctx, int32_t slot, int32_t index);   /* 0044e154 on cars[slot] */
    void (*seek)(void *ctx, int32_t clock, int32_t mark);       /* 0044e354 */
} RaceReplayTickOps;

enum { REPLAY_MARK_EVERY = 0x1c2, REPLAY_INPUT_WAIT = 0x7a0, REPLAY_INPUT_TICKS = 0x78 };

/* 0044e8f8 (no arguments; from 0045009f): 004397f4(004c5590). Then with
 * 004fbb0c set, playback not 1 and (playback 0 or 0052b314 0): before tick
 * 0x78 each human's control word 0x7a0. The countdown (005120d4) 0: it is
 * 0x1c2, 004501fc, and this minute's mark (00512d90 / 0x708) takes what
 * 0044e354 restores (+0x98 and +0x99 whole bytes), then 0044db34, 0044e154
 * on each car with the count of those not human so far, that count to +0x50
 * and 0044e354(00512d90, -1). Else 0044db34. Either way the countdown - 1. */
void race_replay_tick(RaceState *race, const RaceReplayTickOps *ops);

/* The replay's words outside RaceState (0044e700, 0044fc74, 0044fe84) */
typedef struct {
    int32_t end_clock;          /* 005120c8: the recording's last clock */
    int32_t until;              /* 00513720: a jump's end, at most end_clock */
    int32_t target;             /* 00513724: the clock run forward to (phase 3) */
    int32_t saved_timer;        /* 00512d80: 004bb8d8 when the replay began */
    int32_t saved_input_clock;  /* 00512d84: 004c5590 */
    int32_t saved_due;          /* 00512d88: 004c5594 */
    int32_t timer;              /* 004bb8d8 */
    int32_t due;                /* 004c5594 */
    int32_t loading;            /* 004fcdac */
    int32_t jump_clock;         /* 004c53b0: the clock a jump asked for */
    int32_t unknown_4b91c4;
    int32_t unknown_4c5388, unknown_4c53a0;
    int32_t unknown_513730, unknown_513888, unknown_513894, unknown_513898;
    int32_t close_up;           /* 004c53d8: 1 while a close cut plays */
    uint8_t unknown_5137e4;     /* 005137e4: set, the director stops at its last cut */
    int32_t phase_before;       /* 004c5394: the phase the replay came from */
    int32_t unknown_4c53ac;
    int32_t saved_kind;         /* 00512f80: the camera kind the replay began with */
} RaceReplayDirector;

typedef struct {
    void *ctx;
    void (*keep_scenery)(void *ctx);    /* 004501fc */
    void (*channels_stop)(void *ctx);   /* 00443c34 */
    void (*trail_clear)(void *ctx);     /* 00456a48(005146ac) */
    void (*notices_off)(void *ctx);     /* 0041e3c4 */
    void (*marks_back)(void *ctx);      /* 00457560 */
    void (*road_yaw)(void *ctx);        /* 004043d4 */
} RaceReplayJumpOps;

/* 0044e700 (eax clock, edx until; RaceReplayDirector's first eleven; from 0044f090, 0044f188, 0044f378,
 * 0044fc74, 0044fe84): the replay jumps to `clock`. until held to 005120c8
 * from above (signed) into 00513720; the clock % 0x7080 (16 minutes) on a
 * minute (% 0x708 == 0): the tick the clock, else the minute's first tick
 * and 00513724 = the clock. Racing (phase 1): 004501fc, and 004bb8d8,
 * 004c5590, 004c5594 kept in 00512d80 / 84 / 88. Phase 2 on a minute
 * (00513728 = 1), else 3 (00513728 = 0); 00443c34; 004fcdac = 0. Then the
 * clock the tick % 0x7080, the tick flags (1 even, | 4 every 4th, | 0x10
 * every 8th), 0051372c = 1, 00456a48(005146ac), 0041e3c4, 004b91c0 =
 * 004b91c4 = 0, 004b92e4 = -1000, 004c5370 = 1, 004c53b0 = the clock,
 * 00457560, 00513734 = 1, the split gap 0051cc48 and its count 0051cc4c 0,
 * 004043d4. (0045aab0, called before 004501fc, is a bare ret; 004fcdac's 1
 * while racing is overwritten before anything reads it.) */
void race_replay_jump(RaceState *race, RaceReplayDirector *w, const RaceReplayJumpOps *ops, int32_t clock, int32_t until);

typedef struct {
    void *ctx;
    void (*jump)(void *ctx, int32_t clock, int32_t until);  /* 0044e700 */
    void (*step)(void *ctx);                                /* 0044ecc4 */
} RaceReplayCutOps;

/* 0044fc74 (no arguments; from 0044fe84 with 00512f78 >= 0): the
 * director's next cut. 004c5388 = 004c53a8 = 004c53a0 = 00513894 = 0,
 * 004c5398 = 4, 00513730 = 00513888 = 6, 00513898 = 1; 00512f78 + 1.
 * Past the last (>= 00512f74, signed): unless 005137e4, back to cut 0
 * (00512f78 = 0, 0044e700(its clock, until), 0044ecc4, 004c53d8 = 0);
 * with it the end state 00509d70 = 0x20. Else race_rand_below(10); the
 * cut's word 0xe4: near when another human's car (slot below 004fb7e0,
 * not the player's) is within 3 nodes (|node difference| < 3) of the
 * player's, else always near; a word 0xdb..0xf7, the draw above 4, no
 * close cut playing (004c53d8 0) and near: a close cut (004c5398 = 3,
 * 004c53a0 = 2, 00513730 = 8, 00513888 = 6, 00513894 = 00513898 =
 * 004c53d8 = 1), else 004c53d8 = 0. Then 0044e700(the cut's clock,
 * until) and 0044ecc4. (004fbb08 is written back as read.) A cut past
 * the 40 (the original reads on) returns after the count. */
void race_replay_next_cut(RaceState *race, RaceReplayDirector *w, const RaceReplayCutOps *ops);

/* 0044daf4 (no arguments): the tick flags (004fcdb0) from the tick
 * 00513e70: 1 on an even tick, | 4 every 4th, | 0x10 every 8th. */
void race_replay_tick_flags(RaceState *race);

typedef struct {
    void *ctx;
    void (*step)(void *ctx);                                /* 0044ecc4 */
    void (*next_cut)(void *ctx);                            /* 0044fc74 */
    void (*jump)(void *ctx, int32_t clock, int32_t until);  /* 0044e700 */
    void (*hud_redraw)(void *ctx);                          /* 0041e358: 004f9a28 = 1 */
    void (*reselect)(void *ctx);                            /* 00405260 */
} RaceReplayFrameOps;

/* 0044fe84 (no arguments; from 00450020 in phase 2): a replay's frame.
 * 004c5398 0 and 004c539c <= 0 (a still frame): 00513730 = 5, the tick
 * flags (0044daf4's), done. Before 00513720 (signed) with 004c538c 0 or
 * 00512f78 < 0: 004c5388 = 0, 0044ecc4, done. At the recording's end
 * (00513720 == 005120c8) with 00512f78 < 0: the phase back from
 * 004c5394, 0041e358, 00405260 (00446b48 is a bare ret); racing again
 * (phase 1): 004bb8d8 / 004c5590 / 004c5594 back from 00512d80 / 84 / 88
 * and 004c53ac = 1; else 004c5388 = 0, the tick 00512f84, its flags, the
 * clock the tick % 0x7080, 0044e700(tick, 005120c8), 0044ecc4. Then
 * 00443820, and 00444394 for the player's car in physics 1 or 2, else
 * 0044435c. Otherwise with 00512f78 >= 0: 0044fc74. */
void race_replay_frame(RaceState *race, RaceReplayDirector *w, const RaceReplayFrameOps *ops);

typedef struct {
    void *ctx;
    void (*mute)(void *ctx);                                /* 00443768 */
    void (*unmute)(void *ctx);                              /* 0044379c */
    void (*jump)(void *ctx, int32_t clock, int32_t until);  /* 0044e700 */
} RaceReplayButtonOps;

typedef struct {
    void *ctx;
    void (*jump)(void *ctx, int32_t clock, int32_t until);  /* 0044e700 */
    void (*unmute)(void *ctx);                              /* 0044379c */
    void (*road_yaw)(void *ctx);                            /* 004043d4 */
} RaceReplayToEndOps;

/* 0044f090 (no arguments; from 0044f9b8): the replay to the recording's
 * end, to race on from there: 0044e700(max(005120c8 - 8, 0) rounded down
 * to 4, 005120c8); 00512f78 = -2, 004c5398 = 4, 00513730 = 6, 004c5394 =
 * 1, 004c53a8 = 004c53a0 = 00513888 = 00513894 = 0, 00513898 = 1,
 * 0052b8f8 = 1, the record's next kind (004f8020) 00512f80, and with that
 * 0 and the leading car's (00509e18) +0x14 inside (0x1e0000, 0xdc0000)
 * the record's kind (004f801c) 4; 004c5388 = 0; 0044379c; 00443820, then
 * 00444394 for the player's car in physics 1 or 2, else 0044435c;
 * 004043d4. */
void race_replay_to_end(RaceState *race, RaceReplayDirector *w, const RaceReplayToEndOps *ops);

enum { REPLAY_PAUSE = 5, REPLAY_PLAY = 6, REPLAY_SLOW = 7, REPLAY_SLOWER = 8, REPLAY_SLOWEST = 9 };

/* 0044f378 (eax button, edx held; from 0044f9b8): a replay panel button,
 * nothing while racing (phase 1). The speeds set 004c5398, 004c53a0,
 * 00513730, 00513888, 00513894 and 00513898: 6 (4, 0, 6, 6, 0, 1), 7 (3,
 * 2, 8, 6, 1, 1), 8 (2, 4, 9, 6, 2, 1), 9 (1, 8, 10, 6, 3, 1); all but 9
 * then 0044379c. 5 (pause) only when not held: in phase 3 or with 004c539c
 * not 0 nothing, else 00443768, 004c5398 = 0, 004c539c = 2, 00513730 = 5.
 * Any other button: nothing. After 5..9 in phase 4: 004c5394 = 4,
 * 00512f7c = 1, 0044e700(tick - tick % 4, 005120c8), 0051371c = 0. */
void race_replay_button(RaceState *race, RaceReplayDirector *w, const RaceReplayButtonOps *ops, int32_t button,
                        int32_t held);

#endif
