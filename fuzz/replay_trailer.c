/* A replay file's port parts (a player's .RPL, or one shared): its conditions, paints and snow, and the race's setup; and the session
 * rules record (the same reader takes it from the network and from a replay) */
#include "fuzz/fuzz.h"
#include "game/race/race_replay_image.h"
#include "game/race/session_rules.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ReplayConditions c;
    uint8_t paint[RACE_PLAYERS];
    int snow = 0;
    (void)race_replay_conditions_get(data, size, &c);
    (void)race_replay_paint_get(data, size, paint);
    (void)race_replay_snow_get(data, size, &snow);
    SessionRules rules;
    (void)session_rules_get(data, size, &rules);
    uint8_t *setup = calloc(1, REPLAY_SETUP_SIZE);
    if (setup == NULL) return 0;
    memcpy(setup, data, size < REPLAY_SETUP_SIZE ? size : REPLAY_SETUP_SIZE);
    ReplayRace race;
    (void)race_replay_image_race(setup, &race);
    free(setup);
    return 0;
}
