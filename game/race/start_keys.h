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
/* The same with the player's nitro key (controls.cfg, 0 none) in place of N under the port's rule; the exe's rule stays N. */
void start_keys_fill_key(uint8_t out[4], const uint8_t *keys, int32_t start_input, int16_t playback, int32_t port, int32_t nitro_key);

/* The start clock (race_state.h start_clock, 004bb8e0) is the timer's: the exe's timer routine (0043ba0d) adds to it on
 * every tick, with no gate, so in every race, cheat or not, and in a replay too. The port's clock is the input clock
 * (drive_input ticks, two per race call, the same count the race clock keeps). The race_jolt window (25 ticks) reads it. */
void start_clock_fill(int32_t *clock, int32_t ticks);

#endif
