/* The race's sound levels: how far left or right a sound sits and how loud
 * it is from where it is heard (00443eac and the followed car's engine,
 * 004444b8, use them). */
#ifndef TNFS_RACE_SOUND_H
#define TNFS_RACE_SOUND_H

#include "game/race/race_state.h"

#include <stdint.h>

/* 00443c48: the pan of a sound at `angle` from the listener's heading
 * (00472018's angle; >> 16 reads 0..0x100 a turn): 0x40 ahead and behind,
 * 0 a quarter turn round, 0x7f three quarters. */
int32_t race_sound_pan(int32_t angle);

/* 00471fec: how loud (0..0x7f) a sound is `distance` away (00472018's
 * estimate): 0x7f at 0, falling straight to 0 at 0x200000 (32 metres). */
int32_t race_sound_loudness(int32_t distance);

/* 0044404c: plays `sound` on `channel` (race->voices), or, when the channel
 * already plays it, sets the voice's volume, pitch and pan. Volume, pitch and
 * pan are clamped to 0x7f from above; the volume is scaled by the followed
 * slot's sound volume. Sounds 0x20..0x25 and 0x50 always start a new voice;
 * any other one waits while the channel's voice is a sound up to 0x25 still
 * playing. */
void race_sound_channel(RaceState *race, int32_t channel, int32_t sound, int32_t pitch, int32_t volume, int32_t pan);

/* 00444230: a countdown (race->sound_pulse) taken from the 7-word table
 * 00443710 (1, 20, 15, 10, 5, 3, 1) by 004b92d4 when armed, cut to the
 * table's word when that is lower; at 0 sound 0x61 starts (pan 0x40, pitch
 * 0x40, the followed slot's sound volume's low byte, 0 when silent) and the
 * countdown is disarmed, else it goes down by one. With 004b92d4 outside
 * 0..6 (the original reads its stack) or the followed slot outside the
 * entries, the port returns. */
void race_sound_pulse(RaceState *race);

#endif
