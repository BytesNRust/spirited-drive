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
    /* with the extra, the replay's and then the own car's fly-in back to the dash: 0 1 2 3 4 5 10 0; a press during
     * 11's glide (a replay's) goes to 10 as well */
    race.port_replay_cameras = 1;
    const int32_t replay[] = { 1, 2, 3, 4, 5, 10, 0 };
    k = 0;
    CHECK_EQ_INT(next_of(11), 10);
    for (int i = 0; i < 7; i++) CHECK_EQ_INT(k = next_of(k), replay[i]);
    /* in play the fly-ins (10, 11) hand over to their row's next when the glide ends (race_chase_tick): the views a
     * press lands on are 1 2 3 4 5 and the dash, the dash reached through 10 */
    const int32_t rest[] = { 1, 2, 3, 4, 5, 0, 1 };
    k = 0;
    for (int i = 0; i < 7; i++) {
        k = next_of(k);
        if (k == 10 || k == 11) k = race.chase_kinds[k].next;
        CHECK_EQ_INT(k, rest[i]);
    }
    /* another car shown with the extra: the replay's as it is */
    race.viewed = race.player + 1;
    CHECK_EQ_INT(next_of(11), 1);
    race.viewed = race.player;
    /* the same as a replay gives without the extra */
    race.port_replay_cameras = 0;
    race.replay = 1;
    CHECK_EQ_INT(next_of(3), 4);
    /* port (last_camera): the intro's fly-over (the row with the alt target, kind 8) lands on the last camera used, nothing else does */
    race.replay = 0;
    const ChaseKind *const intro = &race.chase_kinds[8], *const other = &race.chase_kinds[11];
    CHECK_EQ_INT(intro->next, 0);   /* the exe: it lands on the dash */
    CHECK_EQ_INT(race_chase_start_kind(&race, intro, intro->next), 0);   /* flag off */
    race.port_start_camera = 4;     /* kind 3 + 1 */
    CHECK_EQ_INT(race_chase_start_kind(&race, intro, intro->next), 0);   /* still off */
    race.port_last_camera = 1;
    CHECK_EQ_INT(race_chase_start_kind(&race, intro, intro->next), 3);
    CHECK_EQ_INT(race_chase_start_kind(&race, other, other->next), other->next);   /* the other fly-ins keep their own */
    race.port_start_camera = 0;
    CHECK_EQ_INT(race_chase_start_kind(&race, intro, intro->next), 0);   /* nothing used yet */
    race.port_start_camera = 6;
    race.replay = 1;
    CHECK_EQ_INT(race_chase_start_kind(&race, intro, intro->next), 0);   /* a replay is not a race start */
    return TEST_REPORT();
}
