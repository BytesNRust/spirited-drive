/* Each human's queue of control words (input_queue.h). */
#include "game/race/input_queue.h"

#include "game/gfx/x86_int.h"

enum { STAND_IN = 0x87a0, GAVE_UP = 0x803f, SEND_WORDS = 10, SEND_SIZE = 0x12, SEND_KIND = 2 };

int input_take(InputQueue *q, uint32_t *control, int slot, InputTakeWords *w)
{
    if (q->active == 0) return 1;
    if (w->playback != 0 && w->dropped != 0) {
        *control = STAND_IN;
        q->ready[slot] = 0;
        q->filled[slot] = 1;
        return 1;
    }
    if (q->ready[slot] != 0) w->waiting = 0;
    else if (w->w4fbb0c == 0 || w->playback != 1) {
        if (w->playback != 1 && w->player != (int32_t)*control) w->waiting = 1;
        return 0;
    }
    *control = (uint32_t)(int32_t)q->input[slot];
    q->ready[slot] = 0;
    return 1;
}

void input_store(InputQueue *queues, int32_t *clock, int32_t humans, const int16_t *stand_in, const InputSendOps *ops)
{
    const int slot = (int)((uint32_t)(*clock / 2) & (INPUT_SLOTS - 1));
    int16_t words[SEND_WORDS] = { 0 };
    for (int i = 0; i < humans; i++) {
        InputQueue *q = &queues[i];
        if (q->gave_up != 0) {
            q->input[slot] = (int16_t)GAVE_UP;
            q->unknown_8 = x_add(q->unknown_8, 2);
        } else if (q->filled[slot] == 0) {
            q->input[slot] = q->input[(slot - 1) & (INPUT_SLOTS - 1)];
            if (stand_in[i] != 0) q->input[slot] = (int16_t)STAND_IN;
            q->unknown_8 = x_add(q->unknown_8, 2);
        }
        if (i + 1 < SEND_WORDS) words[i + 1] = q->input[slot];
        q->filled[slot] = 0;
        q->ready[slot] = 1;
    }
    words[0] = SEND_KIND;
    *clock = x_add(*clock, 2);
    ops->send(ops->ctx, 0, words, SEND_SIZE);
}
