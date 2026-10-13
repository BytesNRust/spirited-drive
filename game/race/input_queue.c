/* Each human's queue of control words (input_queue.h). */
#include "game/race/input_queue.h"

enum { STAND_IN = 0x87a0 };

int input_take(InputQueue *q, uint32_t *control, int slot, InputTakeWords *w)
{
    if (q->active == 0) return 1;
    if (w->playback != 0 && w->dropped != 0) {
        *control = STAND_IN;
        q->ready[slot] = 0;
        q->forced[slot] = 1;
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
