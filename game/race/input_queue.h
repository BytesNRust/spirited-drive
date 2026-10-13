/* Each human's queue of control words (004faa7c, 0x18c a player): the poll
 * (004394dc) stores a word in a slot and marks it ready; the tick (004397f4)
 * takes the slot's word into 004fb704[player] through 00439a7c. */
#ifndef TNFS_INPUT_QUEUE_H
#define TNFS_INPUT_QUEUE_H

#include <stdint.h>

enum { INPUT_SLOTS = 64 };

typedef struct {
    int32_t active;                /* +0: 0 takes nothing */
    int32_t gave_up;               /* +4: set by 004397f4 after 4000 idle polls */
    int32_t unknown_8;             /* +8: 2 more each slot 00439b24 fills itself */
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
} InputTakeWords;

/* 00439a7c (eax queue, edx control, ebx slot), returns 1 when the control
 * word is set. An inactive queue: 1, nothing changed. Playback and dropped
 * both set: the stand-in 0x87a0 (steering centred, pedal at rest,
 * handbrake), the slot no longer ready and marked filled. A ready slot:
 * waiting = 0, then taken. A slot not ready is taken all the same (waiting
 * kept) when 004fbb0c is set and playback is 1; else 0 is returned, after
 * waiting = 1 unless playback is 1 or *control (the caller's player index)
 * is the followed player. Taken: *control = input[slot] sign-extended, the
 * slot no longer ready. */
int input_take(InputQueue *q, uint32_t *control, int slot, InputTakeWords *w);

/* 00439b24 (no arguments): slot = (clock 004fb7a0 / 2, toward zero) & 63.
 * For each of the humans (004fb7e0, at most 8: a ninth queue would run
 * into 004fb704) queues: gave up: input[slot] = 0x803f
 * (steering full, no pedal, handbrake) and +8 += 2; else a slot not filled
 * repeats the slot before it (63 before 0), or 0x87a0 when stand_in[i]
 * (004f82a0) is set, and +8 += 2; then the slot is ready and not filled.
 * Then clock += 2 and 00445e00(0, words, 18): the word 2, then each human's
 * input[slot] (the stack buffer holds 10 words; past the humans its words
 * are whatever the stack held, 0 here). */
void input_store(InputQueue *queues, int32_t *clock, int32_t humans, const int16_t *stand_in, const InputSendOps *ops);

#endif
