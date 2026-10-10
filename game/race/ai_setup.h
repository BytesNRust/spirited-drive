/* Setting up the computer cars for a race (004702d4 and what it calls):
 * driver profiles, lane widths, zones, tuning and the players' grid places. */
#ifndef TNFS_AI_SETUP_H
#define TNFS_AI_SETUP_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00470238: the row of the driver letter table (004c2850, 8 letters a row)
 * the opponents' profiles come from, by the race type: 7 for type 0, 5 or 6
 * by track flag 4 for type 1, 3 or 4 by whether cars 2 and 3 are the same
 * model for type 2, by the first car's model for type 3; anything else keeps
 * `fallback` (the byte the caller had in al). */
int32_t ai_profile_row(const RaceState *race, int32_t fallback);

/* 0044940c: the profile number of the letter in `row` at grid `place`
 * ('A' = 0); row 7 reads into the first profile's bytes, as the original. */
int32_t ai_driver_letter(int32_t place, int32_t row);

/* 0044941c: the handle (address in the original) of driver profile `index`
 * in the table in use (0050bae0). */
uint32_t ai_profile_handle(const RaceState *race, int32_t index);

/* 00449400: the game's own 9 driver profiles (004c2888) become the table in use. */
void ai_use_game_profiles(RaceState *race);

/* 0046ed88: a car put on the starting grid by its grid place: two abreast
 * (one per row of 4 nodes) up to 2 racers, else one every 2 nodes from node
 * 32, half a lane either side of the centre line; on the open roads (track
 * above 2) at the node's x and heading. */
void ai_grid_place(Car *car, RaceState *race);

/* 00456410: the computer cars' tuning (ai.params) set to the game's defaults;
 * words 9 and 10 are counted in seconds there and kept in ticks (x 60). */
void ai_default_params(int32_t params[107]);

/* 004702d4: the computer cars set up for a race: the game's driver profiles,
 * each opponent's letter (on the first variant of a track) and profile, the
 * lane widths, the zones, the counters, the tuning, the humans on the grid
 * and the cars sorted. `fallback_row` is the letter row when the race type
 * names none (the byte the caller left in al). */
void ai_race_setup(RaceState *race, int32_t fallback_row);

#endif
