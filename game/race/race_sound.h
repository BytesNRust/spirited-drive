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

/* 00444204 (watcall channel, sound, pitch, volume; the angle on the stack):
 * 0044404c on the channel with the pan of `angle` (00443c48) and the volume
 * held to 0x7f from above, then eased through 004bc76c. The callers pass
 * volumes of 0 or more (a negative one reads before the table in the
 * original; the port takes 0). */
void race_sound_channel_at(RaceState *race, int32_t channel, int32_t sound, int32_t pitch, int32_t volume, int32_t angle);

/* 00472e34 (watcall (), from 00440ef0): the held car sound (race->car_sound_*)
 * on channel 4. While its ticks run: cars[slot]'s place (00472018) and
 * Doppler factor (00472448), the ticks - 1, then, when the factor is not 0, sound
 * 00471f40[slot] at pitch factor x 00471f68[kind] / 256 and volume
 * (0x200000 - distance) / 256 x 127 / 8192 inside 0x200000 (0 past it),
 * through 00444204 at the angle. With no ticks: channel 4 stopped
 * (0044432c) while it plays one of the car sounds 0x3f..0x41. */
void race_sound_held_car(RaceState *race);

/* 0044432c (watcall channel): the channel's voice stopped (00473c68) when
 * it has one (not negative), then the channel's voice and sound -1. */
void race_sound_channel_stop(RaceState *race, int32_t channel);

/* 00443f9c (watcall a, b, c, d; e, f on the stack, ret 8): 00443eac (the
 * sound hook) with b and c cut to bytes. */
void race_sound_effect(RaceState *race, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);

/* 00444000 (as 00443f9c, from 00472560): 00443eac with b and c cut to bytes
 * and d sar 4. */
void race_sound_effect_scaled(RaceState *race, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);

/* 004442ec (watcall distance, angle; from 0047217c while the viewed car's
 * +0x4ba is set, a horn?): sound 3 on channel 14 at pitch 0x40, as loud as
 * 00471fec makes `distance`, panned by `angle` (00443c48) unless distance
 * is below 2 (0x40). */
void race_sound_signal(RaceState *race, int32_t distance, int32_t angle);

/* 00444324 (falls into 0044432c with 14): channel 14 stopped. */
void race_sound_signal_off(RaceState *race);

/* 0047217c (watcall (), from 00440ef0 every tick): the viewed car's tyres
 * and its +0x4ba signal. In the car's views (kind 1 or less) the distance is
 * 0, the front's angle -lateral_force x 6 (+ 0x1000000 when negative, a byte
 * increment of its top byte) and the rear's 0x800000; else both from
 * 00472018. The sound and its byte: 004c5f98 and 004c5f9c when the first is
 * not negative, else 4 with 1 (8 on an off-road surface, 004fa9bc). With
 * 004bc6f0 set, channel 4 (front) and 12 (rear) get 00443f9c(channel,
 * sound, byte, skid, distance + 1, angle) while that skid is above 0 or the
 * channel still plays 0x26..0x2e; with it clear, 004c5fa8 = 0. Then, while
 * +0x4ba is set and 004fbb04 is a human's (0..004fb7e0), 004442ec(distance,
 * angle) (00472018 first if the distance is still -1), else 00444324. */
void race_sound_tyres(RaceState *race);

/* 00472560 (watcall (), from 00440ef0 every tick): the police siren (with
 * 004fbab8, the police car cars[2] near the heard car: 004b92d4 and the
 * cockpit's lights toggle `police_lights`, 004f9b48; channels 9 and 11) and
 * the two loudest other cars on channels 2 and 3 (their engines, or the
 * skid effect while the heard human has no finish). See race_sound.c. */
void race_sound_others(RaceState *race, int32_t *police_lights);

/* 004720ec (watcall car, angle out; from 00472560): the listener's heading
 * (004f8060, chase angles[1]) taken into 0..0x1000000 in place when
 * negative, less 0042ebbc of the car's x and z from the listener each sar
 * 8 (fx_atan2(dx, dz) << 8), then into 0..0x1000000 (+ 0x1000000 below 0,
 * - 0x1000000 past it). Its y difference is worked out and not used. */
void race_sound_angle(RaceState *race, const Car *car, int32_t *angle);

/* 004724dc (watcall car, from 00472560): how loud another car's engine is,
 * 0..0x40: the larger of its velocity's x / z difference from the followed
 * car's (004f80ac, chase +0x90) plus a quarter (sar 2) of the smaller, or
 * twice the same estimate of its own velocity when that is more, sar 16,
 * held to 0x40 from above (not below). */
int32_t race_sound_speed_level(const Car *car, const Car *followed);

/* 00472448 (watcall car): the car's distance from the listener (chase
 * position x, z: the larger of |dx|, |dz| plus a quarter (sar 2) of the
 * other) kept in +0x3a1; when the view kind is the one kept in +0x3ad and
 * the distance moved by less than 0x96000 either way, the pitch factor
 * 0x1bcccc00 / (0x1bcccc + moved) (0x100 when still), else 0; the view kind
 * kept in +0x3ad. */
int32_t race_sound_doppler(Car *car, const RaceChase *chase);

/* 00472348 (watcall (), from 00440ef0 with two humans or more): the first
 * human (a 16-bit count up to 004fb7e0) other than the viewed one whose car
 * looks back (+0x4ba) and has no finish (its record +0x1c4) holds the car
 * sound: slot it, 999 ticks, kind 1; none, the ticks 0. 004fbb04 is written
 * back as read. */
void race_sound_other_human(RaceState *race);

#endif
