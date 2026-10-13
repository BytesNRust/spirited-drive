/* The race's recording for the replay (race_replay.h). */
#include "game/race/race_replay.h"

enum { CLOCK_STEP = 4, BYTE = 8 };

static void put(uint8_t *recording, size_t size, int64_t at, uint8_t value)
{
    if (at >= 0 && (uint64_t)at < size) recording[at] = value;  /* port guard: the original writes unchecked */
}

void race_replay_record(const RaceReplayWords *w, const uint32_t *inputs, uint8_t *recording, size_t size)
{
    if (w->w4fbb0c == 0 || w->playback == 1) return;
    const int32_t slot = w->clock / CLOCK_STEP;
    for (int32_t i = 0; i < w->humans; i++) {
        const int64_t at = (int64_t)i * REPLAY_SLOTS + slot;
        put(recording, size, at, (uint8_t)inputs[i]);
        put(recording, size, at + REPLAY_HIGH, (uint8_t)(inputs[i] >> BYTE));
    }
}
