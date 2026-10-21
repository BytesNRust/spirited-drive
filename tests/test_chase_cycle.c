/* The view key's next kind in a race (race_chase.h): the original's own-car cycle, and the replay's with the port's
 * replay cameras extra. */
#include "game/race/race_chase.h"
#include "test.h"

static RaceState race;

static int32_t next_of(int32_t kind)
{
    RaceChase chase = { 0 };
    chase.kind = kind;
    return race_chase_pick(&race, &chase);
}

int main(void)
{
    race_chase_init(&race);
    race.phase = 1;
    /* the player's live car: 0 1 2 3 10 0 */
    const int32_t own[] = { 1, 2, 3, 10 };
    int32_t k = 0;
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(k = next_of(k), own[i]);
    CHECK_EQ_INT(next_of(10), 0);
    /* with the extra, the replay's and then the own car's two near ones back to the dash: 0 1 2 3 4 5 11 10 0 */
    race.port_replay_cameras = 1;
    const int32_t replay[] = { 1, 2, 3, 4, 5, 11, 10, 0 };
    k = 0;
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(k = next_of(k), replay[i]);
    /* another car shown with the extra: the replay's as it is */
    race.viewed = race.player + 1;
    CHECK_EQ_INT(next_of(11), 1);
    race.viewed = race.player;
    /* the same as a replay gives without the extra */
    race.port_replay_cameras = 0;
    race.replay = 1;
    CHECK_EQ_INT(next_of(3), 4);
    return TEST_REPORT();
}
