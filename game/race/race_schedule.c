#include "game/race/race_schedule.h"

void race_schedule_run(const RaceScheduleOps *ops, RaceScheduled sched[RACE_SCHEDULE_SLOTS], int16_t frame)
{
    for (int i = 0; i < RACE_SCHEDULE_RUN; i++) {
        if (frame == 0 || sched[i].frame == 0 || frame < sched[i].frame) continue;
        ops->call(ops->ctx, i);
        sched[i].frame = 0;
    }
}

enum { MESSAGE_KIND = 3, MESSAGE_BYTES = 8, SENT_DELAY = 8, PLAYBACK_DELAY = 600 };

void race_schedule_set(const InputSendOps *send, RaceScheduled sched[RACE_SCHEDULE_SLOTS], const RaceScheduleWords *w,
                       int16_t frame, int32_t which, int16_t arg)
{
    if (which < 0 || which >= RACE_SCHEDULE_SLOTS) return;
    sched[which].frame = frame;
    sched[which].arg = arg;
    if (w->w4fb7d4 != 1) return;
    const int16_t message[4] = { MESSAGE_KIND, (int16_t)which, arg, (int16_t)(frame + SENT_DELAY) };
    send->send(send->ctx, 0, message, MESSAGE_BYTES);
    if (w->w4fbb0c == 0 && w->playback == 1)
        sched[which].frame = (int16_t)(sched[which].frame + PLAYBACK_DELAY);
    else
        sched[which].frame = message[3];
}
