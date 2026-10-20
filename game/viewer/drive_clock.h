/* port: the race's steps by real time as the original's timer gives them. 0043b9e8 runs at a 90 Hz base and wakes
 * the race (0043b970) when its countdown 004fcd78 runs out: 3 ticks first (0043ba6c), then 004fcd8c, 004fcd8c and
 * 004fcd88 in turn (the counter 004fcd7c), the two by the level 004fbabc (0043bb44): 1 in a race (3, 3: 30 steps a
 * second), 3 with the Turbo cheat (004275f8 on 004fb7d8 bit 3: 3, 2). */
#ifndef TNFS_DRIVE_CLOCK_H
#define TNFS_DRIVE_CLOCK_H

#include <stdint.h>

enum { DRIVE_CLOCK_BASE_HZ = 90, DRIVE_CLOCK_LEVEL = 1, DRIVE_CLOCK_TURBO_LEVEL = 3 };

typedef struct {
    int32_t long_ticks;     /* 004fcd88 */
    int32_t short_ticks;    /* 004fcd8c */
} DriveClock;

DriveClock drive_clock_of_level(int32_t level);
/* the steps due `ms` after the start */
int32_t drive_clock_steps(const DriveClock *c, uint32_t ms);
/* a round's base ticks (three steps): 9 on level 1, 7 on Turbo's */
int32_t drive_clock_round_ticks(const DriveClock *c);
/* the first ms at which `steps` are due */
uint32_t drive_clock_ms_of(const DriveClock *c, int32_t steps);

#endif
