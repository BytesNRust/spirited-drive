/* dep59 step 3 (Task 11): what the menus' replay list says of a Replay.rpx (game/frontend/replay_label.h): its name when its added cars
 * are here as they were, which mod it needs, or which one changed; cut to the room given, never past it. */
#include "game/frontend/replay_label.h"
#include "test.h"

#include <string.h>

int main(void)
{
    char out[64];
    replay_label("RACE1.RPX", REPLAY_CARS_OK, "", out, sizeof out);
    CHECK(strcmp(out, "RACE1") == 0);
    replay_label("RACE1.RPX", REPLAY_CARS_MISSING, "test-car", out, sizeof out);
    CHECK(strcmp(out, "RACE1 - needs test-car") == 0);
    replay_label("RACE1.RPX", REPLAY_CARS_CHANGED, "test-car", out, sizeof out);
    CHECK(strcmp(out, "RACE1 - test-car changed") == 0);
    replay_label("RACE1.RPX", REPLAY_CARS_BAD, "", out, sizeof out);
    CHECK(strcmp(out, "RACE1 - not a replay") == 0);
    replay_label("NONAME", REPLAY_CARS_OK, "", out, sizeof out);   /* a name without an extension: as it is */
    CHECK(strcmp(out, "NONAME") == 0);
    char small[12];
    memset(small, 'x', sizeof small);
    replay_label("RACE1.RPX", REPLAY_CARS_MISSING, "test-car", small, sizeof small);   /* cut to the room */
    CHECK(strcmp(small, "RACE1 - nee") == 0);
    replay_label("RACE1.RPX", REPLAY_CARS_OK, "", small, 1);
    CHECK(small[0] == '\0');
    CHECK(replay_label_plays(REPLAY_CARS_OK) && !replay_label_plays(REPLAY_CARS_MISSING) && !replay_label_plays(REPLAY_CARS_CHANGED) &&
          !replay_label_plays(REPLAY_CARS_BAD));
    CHECK(replay_label_added("RACE1.RPX") && replay_label_added("race1.rpx") && !replay_label_added("RACE1.RPL") && !replay_label_added("RPX"));
    return TEST_REPORT();
}
