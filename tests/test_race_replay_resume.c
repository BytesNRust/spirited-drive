/* Back from a replay F9 opened (race_replay_seek.c): the director puts the timer back to where the race stood while
 * race_run's count of frames run went on through the replay, so the race waited the replay's length out (a frozen
 * race, then). The port counts from the timer again. */
#include "game/race/race_replay_seek.h"
#include "test.h"

static void back_from_the_replay(void)
{
    /* the race at 1299 frames, the replay 260 more, the timer put back to 1313 */
    CHECK_EQ_INT(race_replay_resumed_done(0, 1, 1313, 1559), 1313);
}

static void everything_else_stands(void)
{
    CHECK_EQ_INT(race_replay_resumed_done(1, 1, 1313, 1559), 1559);   /* racing all along */
    CHECK_EQ_INT(race_replay_resumed_done(0, 0, 1313, 1559), 1559);   /* still in the replay */
    CHECK_EQ_INT(race_replay_resumed_done(0, 1, 1313, 1200), 1200);   /* the timer ahead: frames to run, none lost */
}

int main(void)
{
    back_from_the_replay();
    everything_else_stands();
    return TEST_REPORT();
}
