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

/* 00472f20: unless a car sound is running, car slot `slot` (a racer: slot 1,
 * kind 1) starts one of `kind` for 5 + race_rand_below(15) ticks. */
void race_hold_car_sound(RaceState *race, int32_t slot, int32_t kind);

#endif
