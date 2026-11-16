/* port (dep59, step 1): the cars a mod adds (notes/dep59-content-model-design.md), beside the exe's twelve. An added car has a number for
 * this run only, from CAR_ADDED_FIRST (race_models.c's 100 + car and 200 + car keep it apart from the other ranges below 100); it keeps it
 * while it stays registered, and a number set free goes to a new car only, so a number in another's hand never comes to name another car.
 * Outside the run a car is named by its id. Every race table indexed by car asks here: for an original the answer is what it was. */
#ifndef TNFS_CAR_REGISTRY_H
#define TNFS_CAR_REGISTRY_H

#include "formats/mod_ini.h"
#include "game/race/race_state.h"

#include <stddef.h>
#include <stdint.h>

enum { CAR_ADDED_FIRST = 16, CAR_ADDED_MAX = 64, CAR_ORIGINAL_MODELS = 8 };

typedef struct {
    char id[MOD_INI_ID_MAX + 1], name[MOD_INI_NAME_MAX + 1], stem[MOD_INI_STEM_MAX + 1], short_name[MOD_INI_SHORT_MAX + 1];
    int sound_model, ai_model;   /* the original race models (0..7, sound 0..11) it takes them from while it brings none of its own */
    int dash_model;              /* the same for the cockpit; -1 when not given (then sound_model's: car_registry_dash_model) */
    uint32_t fingerprint;        /* the CRC-32 of its PBS then its PDN as on disk, never 0: what decides how it drives (a replay's check) */
} CarPiece;

/* no added car */
void car_registry_clear(void);
/* `piece` registered (its stem car_piece_stem_ok's and no other added car's): its number, the lowest free one, or -1 with the reason (English, for the log) in `why` */
int car_registry_add(const CarPiece *piece, char *why, size_t why_cap);
/* the registered car of `piece`'s id made `piece`, keeping its number (a reload of a piece that stays: car_pieces.h); its number, or -1
 * with the reason in `why` (then it is as it was) */
int car_registry_update(const CarPiece *piece, char *why, size_t why_cap);
/* the car of id `id` taken out, its number set free; 1 when there was one */
int car_registry_remove(const char *id);
int car_registry_count(void);
/* 1 for a registered added car */
int car_registry_is_added(int car);
/* an added car's piece; NULL for an original or a number not registered */
const CarPiece *car_registry_piece(int car);
/* the registered added cars' numbers, lowest first, into `out` (up to `cap`); how many were put there */
int car_registry_cars(int16_t out[], int cap);
/* the number of the car of id `id`, or -1 */
int car_registry_find(const char *id);
/* the root its files are named by: an original's car_model_names[car], an added car's stem; NULL otherwise */
const char *car_registry_stem(int car);
/* the original whose engine sound it plays: 0..11 for an original (itself), the piece's for an added car; -1 otherwise */
int car_registry_sound_model(int car);
/* the original whose cockpit it shows (SIMDATA/DASH/<stem>DH): itself for an original, an added car's dash_from, or its sound_from
 * when it names none (step 6 gives a piece a cockpit of its own); -1 otherwise */
int car_registry_dash_model(int car);
/* port (dep59 3b): the lamp row of an added car: race_palette_model_light's row 0, (15,3,3) off and (62,5,5) lit (row 13), the red ten of
 * the twelve originals share */
enum { CAR_LAMP_STANDARD = 0 };
/* the row of lamp colours it takes (race_palette_model_light's rows): itself for an original, CAR_LAMP_STANDARD for an added car
 * (whatever its sound_from); -1 otherwise. The exe's quirks of one car (the 512TR's throttle and shift, the Diablo's and 911T's tyres) stay
 * with that car's number: an added car drives as its PBS says */
int car_registry_lamp_model(int car);
/* the original whose pace the AI drives it at: 0..7 for those (itself), the piece's for an added car; -1 otherwise */
int car_registry_ai_model(int car);
/* the name the results and records show for `car` out of `names`, a screen's own table of `count` originals: names[car] for 0..count-1,
 * an added car's short_name, "" for anything else */
const char *car_registry_short_name(int car, const char *const names[], int count);

/* 1 when one of the first `count` entries drives a registered added car */
int car_registry_entries_added(const RaceEntry *entries, int count);
/* The race running has an added car (set at each race's start, track_viewer.c): it keeps no record and no replay in the game's own
 * files, whose formats name a car by its number (plan 1b gives it files of its own) */
void car_registry_race_set(int has_added);
int car_registry_race_has_added(void);
/* the car this machine's player drives in the race running (set at its start with the mark above); -1 before any */
void car_registry_race_player_set(int car);
int car_registry_race_player(void);

#endif
