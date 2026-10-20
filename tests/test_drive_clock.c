/* The race's steps by real time (drive_clock.h): 0043b9e8's countdown at the 90 Hz base, 3 then (short, short,
 * long) ticks a step: 30 a second on level 1, 9/7 of that on Turbo's level 3. */
#include "game/viewer/drive_clock.h"
#include "test.h"

int main(void)
{
    const DriveClock normal = drive_clock_of_level(1), turbo = drive_clock_of_level(3);
    CHECK_EQ_INT(normal.long_ticks, 3);
    CHECK_EQ_INT(normal.short_ticks, 3);
    CHECK_EQ_INT(turbo.long_ticks, 3);
    CHECK_EQ_INT(turbo.short_ticks, 2);
    /* level 1 is the port's 30 Hz: ms x 30 / 1000 */
    for (uint32_t ms = 0; ms < 20000; ms += 7) CHECK_EQ_INT(drive_clock_steps(&normal, ms), (int32_t)(ms * 30 / 1000));
    /* Turbo: steps end at base ticks 3, 5, 7, 10, 12, 14, 17 */
    const uint32_t ends[] = { 3, 5, 7, 10, 12, 14, 17 };
    for (int i = 0; i < 7; i++) {
        const uint32_t ms = (ends[i] * 1000 + 89) / 90;   /* the first ms at that base tick */
        CHECK_EQ_INT(drive_clock_steps(&turbo, ms), i + 1);
        CHECK_EQ_INT(drive_clock_steps(&turbo, ms - 1), i);
        CHECK_EQ_INT((int32_t)drive_clock_ms_of(&turbo, i + 1), (int32_t)ms);
    }
    /* a minute of Turbo is 9/7 of a normal minute's steps */
    CHECK_EQ_INT(drive_clock_steps(&turbo, 60000), 2314);
    CHECK_EQ_INT(drive_clock_ms_of(&normal, 30), 1000);
    /* other levels as 0043bb44 leaves them; an unknown one is level 1's */
    CHECK_EQ_INT(drive_clock_of_level(0).long_ticks, 4);
    CHECK_EQ_INT(drive_clock_of_level(9).short_ticks, 3);
    return TEST_REPORT();
}
