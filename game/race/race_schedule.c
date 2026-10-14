#include "game/race/race_schedule.h"

void race_schedule_run(const RaceScheduleOps *ops, RaceScheduled sched[RACE_SCHEDULE_SLOTS], int16_t frame)
{
    for (int i = 0; i < RACE_SCHEDULE_RUN; i++) {
        if (frame == 0 || sched[i].frame == 0 || frame < sched[i].frame) continue;
        ops->call(ops->ctx, i);
        sched[i].frame = 0;
    }
}
