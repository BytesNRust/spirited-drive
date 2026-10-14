/* A race of cars on a track, ticked as the original's fixed step
 * (0045597c -> 00440ef0) runs them: one human car with the controller
 * (00401400), the computer racers, the physics models 1..6 with the lateral
 * distance (00440a68), and the cars against the scenery and each other
 * (0040d8e8, which also runs the computer cars' manager). The start
 * countdown and the camera, sound and dashboard systems the tick also runs
 * are still to come; so are the police and traffic slots (the port keeps 8
 * cars, the original 9). */
#ifndef TNFS_RACE_SIM_H
#define TNFS_RACE_SIM_H

#include "formats/tri.h"
#include "game/race/car.h"
#include "game/race/car_body.h"
#include "game/race/car_specs.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    RACE_SIM_TICKS_PER_SECOND = 30,  /* calls; the race clock counts two per call */
    RACE_SIM_INPUT_REST = 0x7a0      /* steering centred, pedal at rest */
};

/* A car record (packed, 0x79a bytes) on an 8-byte boundary, so the word
 * fields the code takes pointers to stay aligned in every slot. */
typedef union {
    Car car;
    int64_t align;
} RaceSimSlot;

typedef struct {
    RaceState race;                 /* race.cars[i] points at slots[i].car */
    RaceSimSlot slots[RACE_CARS];
    CarSpecs specs[RACE_CARS];
    int count;
} RaceSim;

/* The track index of a TRI stem ("TR1", "cy2"): CY 0, CL 1, AL 2, TR1..7
 * 3..9 (Control Central's order); -1 when unknown. */
int race_sim_track_index(const char *stem);

/* One human car (specs: the decompressed PBS) on `tri`, as the game sets
 * up a single player: slot 0, the given entry, laps on circuits. */
void race_sim_start(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs, const RaceEntry *entry,
                    int laps, int arcade);

/* The cars of a race as 0042702c sets them up: slot 0 the human, then the
 * computer racers (time trial 1 racer, single race and tournament 8), the
 * police cars and the traffic (race_field_counts). */
typedef struct RaceField RaceField;
struct RaceField {
    int racers;                         /* 004fb7dc */
    int cops;                           /* 004fb7e4: police cars after the racers */
    int traffic;                        /* 004fb7ec: traffic cars after the police */
    uint32_t seed;                      /* 004fb7f4: race_random_seed's (00428fe0) */
    int race_type;                      /* 004fb7d0: 0 time trial, 1 head to head, 2 single race, 3 tournament */
    RaceEntry entries[RACE_PLAYERS];    /* 004fb7fa: each slot's car (the computer racers' too) */
    const CarPdn *pdns[RACE_CARS];   /* 00517580: each car's PDN; NULL takes `load`'s */
    /* 00457d40 where 00440858 runs it, for every slot after its flags and
     * before its body (004407b8): the car's model loaded (car_load_*, by
     * RaceState.models) and its PDN returned, or NULL. NULL: no models. */
    const CarPdn *(*load)(void *user, Car *car, RaceState *race);
    void *user;
};

/* 0042702c's counts for a race type: types 2 and 3 race 8, type 0 one car;
 * type 1 two, with 6 traffic cars and one police car (when 0052b312, the
 * word that also makes the race single-human when 0, is 0) on the tracks
 * below 3 (CY, CL, AL) when the track flags (0052b306) lack bit 2. */
void race_field_counts(RaceField *field, int track, uint32_t track_flags, int flag_52b312);

/* `field`'s cars on `tri`, the human's specs (the decompressed PBS) in slot
 * 0, entered as 00440858 does. */
void race_sim_start_field(RaceSim *sim, const Tri *tri, const char *stem, const CarSpecs *specs,
                          const RaceField *field, int laps, int arcade);

/* One fixed step with the human's input word (see RaceState.inputs). */
void race_sim_tick(RaceSim *sim, uint32_t input);

/* 00440ef0 alone (race_tick on the sim's ops), as 00455a5c's start runs it. */
void race_sim_tick_only(RaceSim *sim);

/* 00455a5c's setup and start, their part on the race (race_run does the
 * same through its ops): the start beeps 1, tick, phase, camera hold,
 * 00513e60 / 64 / 6c 0; a time trial on track 3 starts at node 5 (z
 * 0x1e0000); 004fcdb0 every flag, three ticks (00440ef0) in phase 0, then
 * racing (00513e64 1, 00513e60 0, phase 1). For callers that do not run
 * race_run (--drive's screenshots). */
void race_sim_begin(RaceSim *sim);

/* Whether the race has ended (RaceState.end_request set): the original's
 * race loop then stops and 004275f8 records it. */
int race_sim_over(const RaceSim *sim);

/* The race's end as 004275f8 takes it after the loop: the results and
 * records (0045af80, then 0045b29c). */
void race_sim_end(RaceSim *sim);

#endif
