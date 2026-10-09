/* A race of cars on a track, ticked as the original's fixed step
 * (0045597c -> 00440ef0) runs them. So far one human car: the controller
 * (00401400) and the driving model (004286a0) with the lateral distance
 * (00440a68); the computer cars, the collisions between cars (0040d8e8),
 * the start countdown and the camera, sound and dashboard systems the tick
 * also runs are still to come. */
#ifndef TNFS_RACE_SIM_H
#define TNFS_RACE_SIM_H

#include "formats/tri.h"
#include "game/race/car.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    RACE_SIM_TICKS_PER_SECOND = 30,  /* calls; the race clock counts two per call */
    RACE_SIM_INPUT_REST = 0x7a0      /* steering centred, pedal at rest */
};

typedef struct {
    RaceState race;
    Car cars[RACE_PLAYERS];
    CarSpecs specs[RACE_PLAYERS];
    int count;
} RaceSim;

/* The track index of a TRI stem ("TR1", "cy2"): CY 0, CL 1, AL 2, TR1..7
 * 3..9 (Control Central's order); -1 when unknown. */
int race_sim_track_index(const char *stem);

/* One human car (specs: the decompressed PBS) on `tri`, as the game sets
 * up a single player: slot 0, the given entry, laps on circuits. */
void race_sim_start(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs, const RaceEntry *entry,
                    int laps, int arcade);

/* One fixed step with the human's input word (see RaceState.inputs). */
void race_sim_tick(RaceSim *sim, uint32_t input);

#endif
