/* port: the NITRO cheat's start keys (race_state.h start_keys), filled from the key flags each step. The exe's writer is
 * the keyboard handler (004c693c by scancode: 0049b954 sets a key down, 0049ba1a up); its start reads four of them. */
#ifndef TNFS_START_KEYS_H
#define TNFS_START_KEYS_H

#include <stdint.h>

/* the scancodes of start_keys[0..3]: N (004c696d), LShift (004c6966), RShift (004c6972), B (004c696c) */
enum { START_KEY_N = 0x31, START_KEY_LSHIFT = 0x2a, START_KEY_RSHIFT = 0x36, START_KEY_B = 0x30 };

/* Fills out from keys (one flag per key code, drive_input's keys) while the NITRO cheat is on (start_input) and the
 * race is not a playback; nothing is written otherwise. port 0 is the exe's start: N, and a shift or B with it (the
 * car takes the launch on those). port 1 (the port's): N alone, or the pad's nitro key (RACE_KEYS_PAD_NITRO), which
 * the exe's rule has no pad for. */
void start_keys_fill(uint8_t out[4], const uint8_t *keys, int32_t start_input, int16_t playback, int32_t port);

/* The start clock (race_state.h start_clock, 004bb8e0, the timer the exe's start counts on) is the input's clock, ticks,
 * under the same gate as the keys; without the cheat or in a playback the clock is left as it is. */
void start_clock_fill(int32_t *clock, int32_t ticks, int32_t start_input, int16_t playback);

#endif
