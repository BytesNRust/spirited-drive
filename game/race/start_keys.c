/* port: the NITRO cheat's start keys from the key flags (start_keys.h). */
#include "game/race/start_keys.h"

#include "game/race/race_keys.h"

void start_keys_fill_key(uint8_t out[4], const uint8_t *keys, int32_t start_input, int16_t playback, int32_t port, int32_t nitro_key)
{
    if (start_input == 0 || playback != 0) return;
    const int32_t key = port != 0 ? nitro_key : START_KEY_N;
    out[0] = (key != 0 && keys[key] != 0) || (port != 0 && keys[RACE_KEYS_PAD_NITRO] != 0);
    out[1] = keys[START_KEY_LSHIFT] != 0;
    out[2] = keys[START_KEY_RSHIFT] != 0;
    out[3] = keys[START_KEY_B] != 0;
}

void start_keys_fill(uint8_t out[4], const uint8_t *keys, int32_t start_input, int16_t playback, int32_t port)
{
    start_keys_fill_key(out, keys, start_input, playback, port, START_KEY_N);
}

void start_clock_fill(int32_t *clock, int32_t ticks)
{
    *clock = ticks;
}
