/* Each human's word for the tick (input_tick.h). */
#include "game/race/input_tick.h"

#include "game/gfx/x86_int.h"

static const uint32_t STAND_IN_WORD = 0xffff87a0u;

enum {
    IDLE_HOLD = 500, IDLE_SLOW = 0x96, IDLE_LOST = 0xfa0, NOTICE_TICKS = 0x384, GAVE_UP_FINISH = 999999,
    BOTH_GEARS = 0x3000, CLEAR_GEARS = 0xcfff
};

/* Returns 1 when the tick has to stop (the game dropped). */
static int wait_once(const InputTickOps *ops, InputTickWords *w, int32_t i, InputQueue *q, PlayerRecord *record)
{
    if (w->music_on != 0) ops->music(ops->ctx);
    else ops->service(ops->ctx);
    if (w->timer_8d8 < IDLE_HOLD) w->idle = 0;
    if (w->idle > IDLE_SLOW && w->w4fb7d4 == 1) ops->notice_slow(ops->ctx);
    if (w->idle > IDLE_LOST && w->w4fb7d4 == 1) {
        if (w->notice_on == 0) {
            w->notice_until = x_add(w->clock, NOTICE_TICKS);
            w->notice_on = w->w4fb7d4;
            ops->notice_lost(ops->ctx);
        }
        q->gave_up = 1;
        record->finish = GAVE_UP_FINISH;
        if (w->w4fbb0c == 0 || i == w->player || w->playback != 1) {
            w->dropped = 1;
            w->waiting = 1;
            return 1;
        }
        ops->drop(ops->ctx, (int16_t)i);
    }
    if (w->dropped != 0) {
        w->waiting = 1;
        return 1;
    }
    return 0;
}

void input_tick(const InputTickOps *ops, InputTickWords *w, int32_t clock, InputQueue *queues, uint32_t *inputs,
                const int16_t *stand_in, PlayerRecord *records, int32_t *launch_ready)
{
    if (w->notice_on != 0 && w->clock > w->notice_until) {
        w->notice_on = 0;
        ops->notice_off(ops->ctx);
    }
    const int32_t slot = (int32_t)((uint32_t)(clock / 2) & (INPUT_SLOTS - 1));
    for (int32_t i = 0; i < w->humans; i++) {
        w->idle = 0;
        inputs[i] = (uint32_t)i;
        while (ops->take(ops->ctx, i, &inputs[i], slot) == 0) {
            if (stand_in[i] != 0 && (w->playback == 3 || w->playback == 2)) {
                inputs[i] = STAND_IN_WORD;
                break;
            }
            if (wait_once(ops, w, i, &queues[i], &records[i])) return;
        }
        if ((inputs[i] & BOTH_GEARS) == BOTH_GEARS) {
            inputs[i] &= CLEAR_GEARS;
            launch_ready[i] = 1;
        } else {
            launch_ready[i] = 0;
        }
    }
}
