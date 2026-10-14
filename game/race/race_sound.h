/* The race's sound levels: how far left or right a sound sits and how loud
 * it is from where it is heard (00443eac and the followed car's engine,
 * 004444b8, use them). */
#ifndef TNFS_RACE_SOUND_H
#define TNFS_RACE_SOUND_H

#include "game/race/car_specs.h"
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

/* 00444230: a countdown (race->engine_sound.pulse_*) taken from the 7-word table
 * 00443710 (1, 20, 15, 10, 5, 3, 1) by 004b92d4 when armed, cut to the
 * table's word when that is lower; at 0 sound 0x61 starts (pan 0x40, pitch
 * 0x40, the followed slot's sound volume's low byte, 0 when silent) and the
 * countdown is disarmed, else it goes down by one. With 004b92d4 outside
 * 0..6 (the original reads its stack) or the followed slot outside the
 * entries, the port returns. */
void race_sound_pulse(RaceState *race);

/* 00444394 (), the followed car's engine sounds started again: the engine's
 * second channel (004bcb70's low 16 bits) starts sound 1 (pan 0x40,
 * volume 0, pitch 0x40) unless it holds sound 1 already; the road noise
 * (channel 5) starts sound 0x29 the same way, always; 004bc6f0 = 1; with
 * the wall tones on (0052b320) 0043d120 (0). */
void race_sound_start_engine(RaceState *race);

/* 00443820 (no arguments; from the race's start and its returns from the
 * menus and the replay): 0052b31e 0 or 0052b30e set, and the voice on the
 * channel at 004bcb70 negative (none): that channel starts sound 1 (pan
 * 0x40, volume 0, pitch 0x40), voices[1] sound 2, voices[5] sound 0x29,
 * each voice's sound noted. */
void race_sound_start_voices(RaceState *race);

/* 004444b8: the followed car's sounds each tick (car the followed one,
 * angle and distance from the listener as 00472018 gives them, specs the
 * followed slot's): nothing when its entry's sound volume is 0; 00444230
 * while 004b92d4 is set; the node sound on channel 0xb; the road noise
 * (voices[5]) from the speed, with gravel (channel 0xf) and rumble (4) on
 * track 5; the engine's pitch from the viewed car's revs over the specs'
 * +0xac + 2000; its two voices (voices[1] and the channel at 004bcb70)
 * crossfaded by the eased throttle through 004bc76c, louder near, panned
 * by the angle (unpanned for an entry's sound mode 2); voices[14] when it
 * plays. Where the original divides by 0 or reads past its curve the port
 * returns. */
void race_sound_engine(RaceState *race, const Car *car, int32_t angle, int32_t distance, const CarSpecs *specs);

/* 004723b8: 004444b8 on the viewed car (004fbb04; the original picks
 * 004fbb08 only when the two are the same), heard from the follow camera
 * (00472018) when its kind is above 1, else at angle 0, distance 1 (the
 * in-car views). `specs` is the followed slot's (004444b8 reads them from
 * cars[004fbb08]). */
void race_sound_follow(RaceState *race, const CarSpecs *specs);

/* 00472348 (watcall (), from 00440ef0 with two humans or more): the first
 * human (a 16-bit count up to 004fb7e0) other than the viewed one whose car
 * looks back (+0x4ba) and has no finish (its record +0x1c4) holds the car
 * sound: slot it, 999 ticks, kind 1; none, the ticks 0. 004fbb04 is written
 * back as read. */
void race_sound_other_human(RaceState *race);

#endif
