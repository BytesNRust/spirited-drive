/* A car's finish (00440c30), run for each car racing once a tick. */
#ifndef TNFS_RACE_FINISH_H
#define TNFS_RACE_FINISH_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00440c30: once the car's record holds no finish yet, by the race window
 * (00440afc): out before the start, a player's lap mark and finish become
 * -999; past the finish, both become the tick - 300 and, unless a time
 * trial, the place is said: the player's (the racers that finished before
 * it, kept + 1 one record on) or, in a replay, the viewed car's kept one. */
void race_car_finish(Car *car, RaceState *race);

#endif
