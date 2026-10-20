/* The camera that follows a car (004046b0 and the calls under it). */
#ifndef TNFS_RACE_CHASE_H
#define TNFS_RACE_CHASE_H

#include "game/race/car.h"
#include "game/race/race_state.h"
#include "game/race/race_view.h"

#include <stdint.h>

/* The kinds' rows and the cameras as the exe starts them (004b6bb0,
 * 004b78a8); the record is left to race_chase_follow, 004b7a28 (chase_auto)
 * to race_sim, which keeps it from the race before. */
void race_chase_init(RaceState *race);

/* 00404530: chase follows `car` (nothing when NULL) from its position and
 * angles, taking `kind` (00404450: the eye at the car, offset and turn
 * cleared, node the car's, flags 1; yaw the car's node heading when the car
 * is a new one; then 00405b00). */
void race_chase_follow(RaceState *race, RaceChase *chase, Car *car, int32_t kind);

/* 0040594c (eax = the record, 004f801c), the view key: next_kind becomes
 * the kind's cycle entry: racing (phase 1), CYCLE_OWN when there is no
 * replay, the player's car is viewed and 00513e6c is 0, else CYCLE_RACING;
 * not racing, 7 (police) when the kind is not 7, there are cops, slot 2 is
 * ahead (0043f8f8 (-1, 12)) and chase_auto is set; 6 (the other car) when
 * the kind is not 6 or 10, slot 1 is ahead (0043f948 (-10, 12)) and
 * chase_auto is set (either clears it); else CYCLE_IDLE, 0 for a 7 without
 * cops. Then 6 follows the car of slot (player == 0), 7 with 004fbab8 set
 * slot 2's car, each only while still ahead (else the kind becomes it and
 * the choice runs again); kinds 0..3, 10 and 11 with camera_hold left keep
 * the next_kind there was and stop; the rest follow the viewed car
 * (00404530). Taking 5 sets chase_auto; last 00405b00 on next_kind. */
void race_chase_cycle(RaceState *race, RaceChase *chase);

/* 0040594c's choice of the next kind alone (above, before the cars ahead are looked at); with the port's
 * port_replay_cameras the player's live car takes CYCLE_RACING, as in a replay. */
int32_t race_chase_pick(RaceState *race, const RaceChase *chase);

/* 00405b00: chase takes view kind `kind`. Kinds 0..3, 10 and 11 become 13
 * when the car does not face the road (00405de8); during a replay (0052b316
 * or 00513e6c) 0 and 10 become 1 and 11. The row sets the camera, the
 * glide's ticks and the target (alt_target when first taken with alt_flag,
 * no camera hold, the followed car facing the road and the last kind below
 * 4 or this one 8); then 004046b0, then 00405210 on the camera's view. */
void race_chase_take(RaceState *race, RaceChase *chase, int32_t kind);

/* 004046b0: places the eye for this tick. Kinds 0 and 1 sit in the car (its
 * tilt and suspension in the turn), 9 at the row's target; the others look
 * ahead along the road from behind the car, glide while gliding (taking the
 * row's next kind at the end), keep above the road at physics model 4
 * (00405c9c) and inside its barriers. Kinds 6 and 7 fall back to 13 when
 * the other car is not just behind (0043f948, 0043f8f8) or does not face
 * the road. */
void race_chase_tick(RaceState *race, RaceChase *chase);

/* 004043d4 (watcall (), from 0044e700 and 0044f090): the record's yaw
 * (+0x78) the heading of its eye's road node (+0x38 & 00504208) x 0x400. */
void race_chase_road_yaw(const RaceState *race, RaceChase *chase);

/* 00404630: the step that takes target to `to` in `ticks` ticks (divide
 * set) or by a 2^ticks-th of the way; nothing when ticks is 0. */
void race_chase_glide(RaceChase *chase, const int32_t to[3], int32_t divide, int32_t ticks);

/* 00404368: the distance between two points across x and z (the squares
 * rounded, 0042eb54's root with 8 bits, the sum its first guess). */
int32_t race_chase_distance(const int32_t a[3], const int32_t b[3]);

/* 00404544: `from` turned back by the angles (the turns about x, z and y
 * by -pitch, -roll, -yaw, multiplied in that order), at 8 bits less
 * precision: out = ((from >> 8) x M) << 8. */
void race_chase_unturn(const int32_t from[3], int32_t out[3], int32_t pitch, int32_t roll, int32_t yaw);

/* 0043f8f8: 004fbab8 is set, slot 2's car has a flags_174 bit of 0x42400
 * and the followed car (race->player) is more than `after` and at most
 * `upto` nodes ahead of it. */
int race_chase_ahead_of_slot2(const RaceState *race, int32_t after, int32_t upto);

/* 0043f948: slot 1's entry is not car 9 and the followed car is more than
 * `after` and at most `upto` nodes ahead of the other of slots 0 and 1. */
int race_chase_ahead_of_other(const RaceState *race, int32_t after, int32_t upto);

/* 00405c9c: the height of the road's plane under `point` at `node`: the
 * plane through the next node with the normal along x side, where along
 * runs from this node to the next and side is the node's +0x1c..+0x20
 * shorts x 2 (fx_cross, 0042ec50). */
int32_t race_chase_road_height(const RaceState *race, int32_t node, const int32_t point[3]);

typedef struct {
    void *ctx;
    void (*views)(void *ctx);                                       /* 00405630 */
    void (*layout)(void *ctx, RaceCamera *camera, int32_t layout);  /* 00405154 */
    void (*view)(void *ctx, int32_t rear);                          /* 0040586c(004f801c, rear) */
} RaceChaseStartOps;

/* 00404404 (watcall (), from 0044ca3c): the record's +0x78 = the road
 * node's (00505038, node +0x38 & 00504208, 36 bytes a node) dword at +0x16
 * >> 16, << 10; 00405630; 00405154 on the record's camera (+0x88) with
 * layout 0 (edx, zeroed before 00405630, which keeps it); 0040586c(record,
 * 0). */
void race_chase_start_view(const RaceChaseStartOps *ops, RaceChase *chase, const uint8_t *nodes, int32_t mask);

#endif
