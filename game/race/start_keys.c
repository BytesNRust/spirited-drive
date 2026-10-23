/* port: the NITRO cheat's start keys from the key flags (start_keys.h). */
#include "game/race/start_keys.h"

#include "game/race/race_keys.h"

void start_keys_fill(uint8_t out[4], const uint8_t *keys, int32_t start_input, int16_t playback, int32_t port)
{
    if (start_input == 0 || playback != 0) return;
    out[0] = keys[START_KEY_N] != 0 || (port != 0 && keys[RACE_KEYS_PAD_NITRO] != 0);
    out[1] = keys[START_KEY_LSHIFT] != 0;
    out[2] = keys[START_KEY_RSHIFT] != 0;
    out[3] = keys[START_KEY_B] != 0;
}

void start_clock_fill(int32_t *clock, int32_t ticks)
{
    *clock = ticks;
}
