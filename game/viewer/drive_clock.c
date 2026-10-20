/* port: the race's steps by real time (drive_clock.h) */
#include "game/viewer/drive_clock.h"

#include "game/race/race_timer.h"

enum { FIRST_TICKS = 3, STEPS_A_ROUND = 3, MS_PER_SECOND = 1000 };

DriveClock drive_clock_of_level(int32_t level)
{
    RaceTimerLevel l = { 0 };
    race_timer_level(&l, DRIVE_CLOCK_LEVEL);
    race_timer_level(&l, level);   /* an unknown level leaves level 1's */
    return (DriveClock){ l.unknown_4fcd88, l.unknown_4fcd8c };
}

int32_t drive_clock_round_ticks(const DriveClock *c) { return 2 * c->short_ticks + c->long_ticks; }

int32_t drive_clock_steps(const DriveClock *c, uint32_t ms)
{
    const int64_t base = (int64_t)ms * DRIVE_CLOCK_BASE_HZ / MS_PER_SECOND;
    if (base < FIRST_TICKS) return 0;
    const int64_t after = base - FIRST_TICKS, rest = after % drive_clock_round_ticks(c);
    int64_t steps = 1 + STEPS_A_ROUND * (after / drive_clock_round_ticks(c));
    if (rest >= c->short_ticks) steps++;
    if (rest >= 2 * c->short_ticks) steps++;
    return (int32_t)steps;
}

uint32_t drive_clock_ms_of(const DriveClock *c, int32_t steps)
{
    if (steps <= 0) return 0;
    const int64_t k = steps - 1;
    const int64_t base = FIRST_TICKS + k / STEPS_A_ROUND * drive_clock_round_ticks(c) + k % STEPS_A_ROUND * c->short_ticks;
    return (uint32_t)((base * MS_PER_SECOND + DRIVE_CLOCK_BASE_HZ - 1) / DRIVE_CLOCK_BASE_HZ);
}
