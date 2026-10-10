/* The computer cars' driver (0046f3e8): moves a car's speed towards the one
 * it wants, steers it over the road and pulls it onto the road's plane. */
#ifndef TNFS_AI_DRIVE_H
#define TNFS_AI_DRIVE_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0046f3e8: `countdown` is the car's ai_countdown as the tick began. */
void ai_drive(Car *car, RaceState *race, int32_t countdown);

/* Which of the 8 slots across the road the car's sideways position falls in
 * (0..3 left of the centre, 4..7 right) by the lane width at the node; the
 * width is left in ai.last_lane_width (0046f3e8, 0046d79c). */
int32_t ai_lane_slot(RaceState *race, const Car *car, const TriNode *node);

/* 0046d79c (physics model 3): one tick of a computer car: its zone and
 * place on the road, its turn, the driver (0046f3e8) when due, the road's
 * edge and the step along its speed. */
void ai_model_tick(Car *car, RaceState *race);

/* 00470518 (physics model 6): a traffic car parked high above the track
 * (x and z 0x80000, y by its index) until its countdown (+0x148) runs out
 * with the racers far enough on and more cars parked than the traffic
 * setting allows; then it goes back to model 3 at a zone (0046a18c). */
void ai_parked_tick(Car *car, RaceState *race);

/* 0046d5c8: a computer car past the road's side line (road_lateral, signed)
 * loses the part of its speed carrying it further out (by 1.25) and is
 * nudged back by the side row unless already moving back in fast. */
void ai_keep_off_edge(Car *car);

/* 0046f354: the fastest sideways speed the car may steer at, signed by
 * `error` (the distance still to go), for a forward speed of `speed`. */
int32_t ai_lane_speed(const Car *car, const RaceState *race, int32_t error, int32_t speed);

/* 0046e3e4: a human's car is near `car` in the road's order. */
int ai_human_near(const RaceState *race, const Car *car);

/* 0046e388: `other`'s node lies from 0x14 nodes behind to 0x36 ahead of `human`'s. */
int ai_in_window(const RaceState *race, const Car *human, const Car *other);

#endif
