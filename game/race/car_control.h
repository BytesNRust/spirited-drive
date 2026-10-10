/* The player's controller (00401400): steering, pedals and gear keys from
 * the input word, and the autopilot before the start and after the finish. */
#ifndef TNFS_CAR_CONTROL_H
#define TNFS_CAR_CONTROL_H

#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

/* 00401400: the controller 00509dd4 index 1. */
void car_player_control(Car *car, const CarSpecs *specs, RaceState *race);

/* 00440afc: 1 before the start, 2 past the finish, else 0. Before the
 * start of a stage it keeps the car past z 0x120000. */
int car_race_window(Car *car, RaceState *race);

/* 00440bf4: how far past the finish the car is: on a circuit its node once
 * its lap count is past the race's laps, on a stage its node past the lap's
 * nodes; else 0. */
int32_t car_past_finish(const Car *car, const RaceState *race);

#endif
