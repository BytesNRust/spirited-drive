/* Each human's queue of control words (004faa7c, 0x18c a player): the poll
 * (004394dc) stores a word in a slot and marks it ready; the tick (004397f4)
 * takes the slot's word into 004fb704[player] through 00439a7c. */
#ifndef TNFS_INPUT_QUEUE_H
#define TNFS_INPUT_QUEUE_H

#include "game/race/race_state.h"

#include <stdint.h>

enum { INPUT_SLOTS = 64, INPUT_QUEUES = 8 };

typedef struct {
    int32_t active;                /* +0: 0 takes nothing */
    int32_t gave_up;               /* +4: set by 004397f4 after 4000 idle polls */
    int32_t clock;                 /* +8: twice the next slot to fill (00439738) */
    int16_t input[INPUT_SLOTS];    /* +0xc: the slot's control word */
    int16_t ready[INPUT_SLOTS];    /* +0x8c: the slot holds a word not yet taken */
    int16_t filled[INPUT_SLOTS];   /* +0x10c: the slot's word is in already */
} InputQueue;

typedef struct {
    void *ctx;
    /* 00445e00 (eax to, edx data, ebx size): the network send */
    void (*send)(void *ctx, int32_t to, const int16_t *words, int32_t size);
} InputSendOps;

typedef struct {
    int16_t playback;   /* 0052b312 */
    int16_t dropped;    /* 0052b314: the network game was dropped */
    int32_t w4fbb0c;    /* 004fbb0c */
    int32_t player;     /* 004fbb08 */
    int32_t waiting;    /* 004bb8e8: written */
    int32_t w4fcdac;    /* 004fcdac: written */
} InputWords;

/* 00439a7c (eax queue, edx control, ebx slot), returns 1 when the control
 * word is set. An inactive queue: 1, nothing changed. Playback and dropped
 * both set: the stand-in 0x87a0 (steering centred, pedal at rest,
 * handbrake), the slot no longer ready and marked filled. A ready slot:
 * waiting = 0, then taken. A slot not ready is taken all the same (waiting
 * kept) when 004fbb0c is set and playback is 1; else 0 is returned, after
 * waiting = 1 unless playback is 1 or *control (the caller's player index)
 * is the followed player. Taken: *control = input[slot] sign-extended, the
 * slot no longer ready. */
/* 00439430 (watcall queue, from 00455a5c's start for each human): active
 * and gave_up 0, clock 1, every slot's ready and filled flags 0; 004fb790
 * (`late`) and 004fb78c 0. The inputs are left as they are. */
void input_queue_reset(InputQueue *q, int32_t *late, int32_t *unknown_4fb78c);

typedef struct {
    int32_t camera_keys;   /* 004fb7b4 */
    int32_t w4fb7ac;       /* 004fb7ac */
    int32_t w4fb7b0;       /* 004fb7b0 */
    int32_t w4bafe4;       /* 004bafe4 */
    int32_t late;          /* 004fb790 */
    int32_t w4fb78c;       /* 004fb78c */
} InputResetWords;

/* 004381ac (watcall (), from 004275f8): 004fb7b4, 004fb7ac, 004fb7b0 and
 * 004bafe4 0, then 00439430 on each human's queue (004fb7e0 of them).
 * Port guard: at most the 8 queues. */
void input_queues_reset(InputQueue *queues, int32_t humans, InputResetWords *w);

/* 00439478 (watcall queue, after 00439430): in a network game (004fb7d4 1)
 * the first n = max(004fbac8 sar 1, 2) slots hold 0x7a0, ready; the clock
 * (and 004fb7a0) 2n, 004c5598 2n - 2. Else the clock 0. */
void input_queue_prime(InputQueue *q, int32_t net_start, int32_t wait_ticks, int32_t *clock, int32_t *replay_due);

int input_take(InputQueue *q, uint32_t *control, int slot, InputWords *w);

/* 00439b24 (no arguments): slot = (clock 004fb7a0 / 2, toward zero) & 63.
 * For each of the humans (004fb7e0, at most 8: a ninth queue would run
 * into 004fb704) queues: gave up: input[slot] = 0x803f
 * (steering full, no pedal, handbrake) and the queue's clock += 2; else a slot not filled
 * repeats the slot before it (63 before 0), or 0x87a0 when stand_in[i]
 * (004f82a0) is set, and the queue's clock += 2; then the slot is ready and not filled.
 * Then clock += 2 and 00445e00(0, words, 18): the word 2, then each human's
 * input[slot] (the stack buffer holds 10 words; past the humans its words
 * are whatever the stack held, 0 here). */
void input_store(InputQueue *queues, int32_t *clock, int32_t humans, const int16_t *stand_in, const InputSendOps *ops);

/* 00439738 (eax queue, edx word): nothing for an inactive queue. slot =
 * (clock / 2, toward zero) & 63. Playback 1: when the slot is still ready,
 * or filled with 004fbb0c set, and not dropped yet: dropped = 1, 004fcdac =
 * 1, waiting = 1. Otherwise: waiting = 1 when slot + 42 (& 63) is ready
 * (the reader is 22 slots behind). Then the slot is ready and filled with
 * the word, clock += 2. */
void input_put(InputQueue *q, int16_t word, InputWords *w);

typedef struct {
    void *ctx;
    void (*lock)(void *ctx, uint32_t lock);            /* 00476c78 (cdecl) */
    void (*unlock)(void *ctx, uint32_t lock);          /* 00476cfc (cdecl) */
    int16_t (*slot_of)(void *ctx, uint32_t link);      /* 0044659c */
} InputMessageOps;

/* Read again after every call (the ctx may change them) */
typedef struct {
    uint32_t sem;       /* 004f8160 */
    InputWords w;       /* playback, dropped, 004fbb0c, player (004fbb08), waiting, 004fcdac */
    int32_t late;       /* 004fb790: written 0 */
    int32_t w4fb78c;    /* 004fb78c */
    int32_t w4c5598;    /* 004c5598: written, a queue's clock */
    int16_t humans;     /* 004f81d2 */
} InputMessageWords;

/* 004395cc (004bcb98[2], message kind 2: eax link, edx data): lock(004f8160).
 * Playback not 1: 004c5598 = queue[004fbb0c]'s clock, then 00439738 on queue
 * 1 when 004fbb08 is 0, else 0, with the word at +2. Playback 1 with
 * 004fbb0c: the sender's queue (short) 0044659c(link): its clock past queue
 * 0's + 2 (signed): the word at +2 replaces its last slot's (clock - 2, / 2
 * toward zero, & 63); else 00439738 with it. Playback 1 without: 004fb790 =
 * 0, 004bb8e8 = 0 unless 004fb78c, 004c5598 = queue 0's clock, then 00439738
 * on queue i with the word at +2 + 2i for each of the 004f81d2 humans.
 * unlock(004f8160 read anew). Port guard: queues 0..7 only. */
void input_on_message(const InputMessageOps *ops, InputQueue queues[RACE_PLAYERS], InputMessageWords *m, uint32_t link,
                      const uint8_t *data);

typedef struct {
    void *ctx;
    uint32_t (*read)(void *ctx);                        /* 0043915c */
    void (*send)(void *ctx, int32_t to, const int16_t *words, int32_t size);  /* 00445e00 */
    void (*put)(void *ctx, int32_t player, int32_t word);  /* 00439738 on 004faa7c + player x 0x18c */
    void (*store)(void *ctx);                           /* 00439b24 */
} InputPollOps;

typedef struct {
    int16_t replay;     /* 0052b316 */
    int16_t playback;   /* 0052b312 */
    int32_t player;     /* 004fbb08 */
    int32_t w4fbb0c;    /* 004fbb0c */
    int32_t late;       /* 004fb790: polls sent while playback 1 */
    int32_t waiting;    /* 004bb8e8: written */
} InputPollWords;

/* 004394dc (no arguments, from 0043b970): nothing in a replay. Else word =
 * read(). With 004fbb0c 0 or playback 2 or 3: send(0, { 2, word }, 4),
 * then playback 1: late += 1, waiting = 1 past 15; else put(player, word
 * sign-extended from 16 bits). Else put(player, word) and, playback 1 with
 * 004fbb0c set, store(). */
void input_poll(const InputPollOps *ops, InputPollWords *w);

typedef struct {
    int16_t race_type;  /* 00524e5d: 0 a time trial */
    int16_t replay;     /* 0052b316 */
    int16_t playback;   /* 0052b312 */
    int32_t player;     /* 004fbb08 */
    int32_t w4fbb0c;    /* 004fbb0c */
} InputStandInWords;

/* 0041019c (eax player): nothing in a time trial, a replay, for the followed
 * player once finished (record +0x1c4), or for one stood in already unless
 * playback is 2 or 3; else block[0] (004f829e) = 10 and the player's stand-in
 * (block[1 + player], 004f82a0) = 1, sent (00445e00 (0, block, 0x12)) when
 * playback is on, and with 004fbb0c set the record's finish = 999999. */
void input_stand_in(int32_t player, int16_t *block, PlayerRecord *records, const InputStandInWords *w,
                    const InputSendOps *ops);

typedef struct {
    int32_t w4fbb0c;    /* 004fbb0c */
    int16_t playback;   /* 0052b312 */
    int32_t racers;     /* 004fb7dc */
    int32_t humans;     /* 004fb7e0 */
} InputStandInStartWords;

typedef struct {
    void *ctx;
    /* 00445e00 (eax to, edx data, ebx size): the network send */
    void (*send)(void *ctx, int32_t to, const int16_t *words, int32_t size);
    /* [0050a578 + 4 player]'s first dword: non-zero while the player's link is up */
    int32_t (*linked)(void *ctx, int32_t player);
} InputStandInStartOps;

/* 00410250 (): with 004fbb0c set, player 0's finish (record +0x1c4) and
 * stand-in (block[1]) = 0; then with playback on, each racer 1..racers - 1:
 * a human with playback 1 whose link is down stands in (1) with the finish
 * 999999, any other 0 and 0; then block[0] = 10, sent (0, block, 0x12). */
void input_stand_in_start(int16_t *block, PlayerRecord *records, const InputStandInStartWords *w,
                          const InputStandInStartOps *ops);

#endif
