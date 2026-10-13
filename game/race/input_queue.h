/* Each human's queue of control words (004faa7c, 0x18c a player): the poll
 * (004394dc) stores a word in a slot and marks it ready; the tick (004397f4)
 * takes the slot's word into 004fb704[player] through 00439a7c. */
#ifndef TNFS_INPUT_QUEUE_H
#define TNFS_INPUT_QUEUE_H

#include "game/race/race_state.h"

#include <stdint.h>

enum { INPUT_SLOTS = 64 };

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

#endif
