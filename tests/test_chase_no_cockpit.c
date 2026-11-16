/* port (dep59 3b): an added car without dash_from has no cockpit (race_state.h's port_no_cockpit): the chase never takes the cockpit's
 * view (kind 0) for it but the first outside one (kind 1), and the camera kept for the next race (port_start_camera) stays what it was.
 * With the flag off (every original) kind 0 is taken as before. */
#include "formats/tri.h"
#include "game/race/race_chase.h"
#include "test.h"

#include <string.h>

enum { COCKPIT = 0, OUTSIDE = 1, RACING = 1, KEPT = 4 /* kind 3 + 1 */ };

static RaceState race;
static Car car;
static Tri tri;
static uint8_t track[0x98c + 36 * 4];   /* four flat nodes of zeros past the TRI's header: the road the outside views follow */

static void setup(int no_cockpit)
{
    memset(&race, 0, sizeof race);
    memset(&car, 0, sizeof car);
    race_chase_init(&race);
    tri.data = track, tri.size = sizeof track, tri.node_count = 3, tri.ring_nodes = 4;
    race.tri = &tri;
    race.port_follow_camera = 1;   /* faces the road on its tilt alone */
    race.phase = RACING;
    race.player = 0;
    race.cars[0] = &car;
    race.port_last_camera = 1;
    race.port_start_camera = KEPT;
    race.port_no_cockpit = no_cockpit;
    race.chase.car = &car;
    race.chase.kind = 3;
}

/* the player's view key or the intro's landing (race_chase_take) and the race's own setup (race_chase_follow) */
static void test_take(void)
{
    setup(1);
    race_chase_take(&race, &race.chase, COCKPIT);
    CHECK_EQ_INT(race.chase.kind, OUTSIDE);
    CHECK_EQ_INT(race.chase.next_kind, OUTSIDE);   /* race_camera_pick would take the cockpit again on the next tick otherwise */
    CHECK_EQ_INT(race.port_start_camera, KEPT);     /* the next race's camera is the player's, not this car's stand-in */
    race_chase_follow(&race, &race.chase, &car, COCKPIT);
    CHECK_EQ_INT(race.chase.kind, OUTSIDE);
    CHECK_EQ_INT(race.port_start_camera, KEPT);
    race_chase_take(&race, &race.chase, 2);        /* other views: taken and kept as before */
    CHECK_EQ_INT(race.chase.kind, 2);
    CHECK_EQ_INT(race.port_start_camera, 3);

    setup(0);
    race_chase_take(&race, &race.chase, COCKPIT);
    CHECK_EQ_INT(race.chase.kind, COCKPIT);
    CHECK_EQ_INT(race.port_start_camera, COCKPIT + 1);
    setup(0);
    race_chase_follow(&race, &race.chase, &car, COCKPIT);
    CHECK_EQ_INT(race.chase.kind, COCKPIT);
}

/* the view key's cycle with the flag: 0 1 2 3 10 then 10's next, which is the cockpit, lands outside */
static void test_cycle(void)
{
    setup(1);
    race.chase.kind = 10;
    race_chase_take(&race, &race.chase, race_chase_pick(&race, &race.chase));
    CHECK_EQ_INT(race.chase.kind, OUTSIDE);
}

int main(void)
{
    test_take();
    test_cycle();
    return TEST_REPORT();
}
