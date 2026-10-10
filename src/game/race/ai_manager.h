/* The computer cars' manager, run every 4 ticks (0046c490), and its helpers. */
#ifndef TNFS_AI_MANAGER_H
#define TNFS_AI_MANAGER_H

#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* 0046aed0: the three lanes' steering wish `wish` (16.16 words of the
 * manager) loses weight for each lane whose speeds in `first` and `second`
 * (the two rows 0046b0d8 fills) are behind the car's target speed (+0x160)
 * or its speed (+0x15c), against the direction of travel for an oncoming
 * car; then the target speed is held to the middle lane's `first` speed and
 * kept off -0x14ccc (oncoming: under 0x14ccc). */
void ai_weigh_lanes(Car *car, const int32_t first[3], const int32_t second[3], int32_t wish[3]);

/* 0046a95c: moving from lane slot `from` to `to`, the target speed (+0x160)
 * is held to `row` (the manager's first speed row) at the new lane's side
 * (row[2] for a higher slot, row[0] for a lower one) when there is room
 * past `ahead` (the car ahead, NULL for none; crashed counts as none): its
 * node gap x 6 x the car's sideways speed beats the sideways distance still
 * to go x the speed gap x 1.2. Otherwise, or staying, the middle row[1].
 * Kept off -0x14ccc as 0046aed0 (oncoming: below 0x14ccc). */
void ai_hold_lane_speed(Car *car, const RaceState *race, const Car *ahead, int32_t from, int32_t to,
                        const int32_t row[3]);

/* 0046b0d8: the cars ahead in the lane slots `lane` - 1, `lane` and `lane` + 1
 * (row entries 0, 1, 2), looked for in the road's order from the car
 * (oncoming: backwards, the car's own place first). A car counts for a slot
 * when `occupied` (one mask per place in by_node) has the slot's bit, the slot
 * is not in `found` yet and its node gap is 1..26 (a racer behind an oncoming
 * car: its profile's oncoming_reach; going the way of the first car, a gap
 * under 2 only when it is 5 ahead along the road's forward row). The slot gets
 * the car's speed plus the gap less 2 (oncoming: plus 2) times 5 in `first`
 * and 1.71 in `second`, and the car in `ahead`; three slots end the search.
 * Then traffic close behind the middle car halves the rows, a racer right
 * behind it takes 7/8 of the last car looked at's speed past 20, the road's
 * outermost lanes halve their side, and a fast car that cannot pull out
 * before closing on a racer ahead (as 0046a95c's room) gets 0 in the middle. */
void ai_cars_ahead(const Car *car, const RaceState *race, const uint32_t occupied[RACE_PLAYERS], int32_t lane,
                   uint32_t *found, int32_t first[3], int32_t second[3], const Car *ahead[3]);

/* 0046a7c0: a computer car spins out: it crashes from a hit (0040db84), its
 * body's spin gains `side` along the body's first row and `along` along the
 * third and loses `up` along the second (the manager passes 0x190000 for all
 * three), a racer ends the police chase, and the first-hit kick (0040f8a8). */
void ai_spin_out(Car *car, RaceState *race, int32_t up, int32_t along, int32_t side);

/* 0046b950: once the first car (cars[0]) passes the node it was last
 * looked from, the inactive police car (cars[2]) may be placed on the first
 * node 50 ahead flagged for it (node +0x1a bit 0), by that node's odds
 * (bits 3..4: 32, 64, 96 or 128 in 128): on the barrier side the node's bit 1
 * says, facing the way of node 0's lanes; past the lap's nodes the chase
 * ends first. Off when ai params[13] is 0 or 004fb7d4 is 1. */
void ai_place_police(RaceState *race);

/* 0046d580: the car's speed along its road frame's forward row (road_axes
 * 6..8), its velocity's x negated, every term 8.8 by 8.8. */
int32_t ai_road_speed(const Car *car);

/* 0046bfb0: for each human car: a car at `node` (the car's, from 0046c278)
 * up to the profile's block_reach ahead of it, with `progress` past 0x86,
 * whose human is at least block_gap behind along the road adds 7.0 to the
 * wish of the human's side (0 below `lane`, 1 the lane, 2 above). On a stage,
 * closing on a human ahead in its lane faster than about 0.83 node lengths a
 * tick (the second car's speed against 0042eb40(0x35555, gap x 6)) counts
 * the body's +0x84 up and sounds the horn (00472f20) once a draw of 0..9 is
 * under the count; otherwise the count goes back to 0. A car 5 nodes behind a
 * human, or 2 behind the first car, above 0xf0000 with the human in model 1
 * sounds it too. */
void ai_watch_humans(Car *car, RaceState *race, int32_t lane, int32_t wish[3], int32_t node, int32_t progress);

/* 0046c278: past the finish (00440afc gives 2) a car off the road's edge
 * lanes pulls toward its grid place's side (even places +100 to wish 0, odd
 * to wish 2) from 10 nodes before its place on; crossing up past a gear
 * threshold (+0x308) by under 0x8000 starts the model's coast_timer; the
 * revs (+0x3c1) ease by an eighth toward the speed through the gear ratio
 * (3000 while coasting), at most the redline; then 0046bfb0 with the node
 * and progress taken before the finish check. */
void ai_finish_and_revs(Car *car, RaceState *race, int32_t lane, int32_t wish[3]);

/* 00470a9c: the speed a computer car aims for at its node. Traffic: the
 * track's speed byte for the segment times the first car's model factor,
 * 0 once that car crashed hard. A racer: the byte times its pace, the pace
 * being its +0x170 (by the profile's mass_scale) times the bend pace
 * (+0x168 less the heading change from the node before, by +0x16c, 0.203
 * of it, by bend_scale; left in ai.curve_pace and ai.bend) plus 0.8, by the
 * catch-up scales of the gap to the first car (ai.params and the profile's
 * pace_by_gap, row ai.gap_step); held to 1.2 / 0.8 by a human nearby,
 * raised before progress 0x41, and 0 once past its grid place after the
 * finish. Track flag 4 leaves the profile out. */
int32_t ai_target_pace(Car *car, RaceState *race);

/* 00472f20: unless a car sound is running, car slot `slot` (a racer: slot 1,
 * kind 1) starts one of `kind` for 5 + race_rand_below(15) ticks. */
void race_hold_car_sound(RaceState *race, int32_t slot, int32_t kind);

#endif
