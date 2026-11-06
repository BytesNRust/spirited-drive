/* port (the mode's file's difficulty): the race's difficulty, three axes (the rivals', the police's, the traffic's), each Easy, Original or
 * Hard. Original, the zero, is the exe's, bit for bit: every function here hands back what it is given. A race's value is fixed at its
 * start (RaceField, RaceState port_difficulty); a replay stores it in its setup and plays it back, a link race and a replay without a
 * stamp (the CD's demos too) run Original. */
#ifndef TNFS_DIFFICULTY_H
#define TNFS_DIFFICULTY_H

#include <stdint.h>

enum { DIFFICULTY_ORIGINAL = 0, DIFFICULTY_EASY = 1, DIFFICULTY_HARD = 2 };

typedef struct {
    int32_t rivals;    /* the racers' pace (ai_setup.c's params, ai_manager.c's profile) */
    int32_t police;    /* where the police car is placed, the stops that count, how many end the race */
    int32_t traffic;   /* how many cars, how fast */
} RaceDifficulty;

/* the mode's file's difficulty=0|1|2 (Easy, Original, Hard; anything else Original) as all three axes */
RaceDifficulty difficulty_from_extras(int level);

/* 1 when every axis is Original: the one question the records' code asks */
int difficulty_is_original(const RaceDifficulty *d);

/* The race now running (set at its start, like reverse_track_set): records_route.c keeps no record in a race that is not Original */
void difficulty_running_set(const RaceDifficulty *d);
int difficulty_running_original(void);

/* The race's difficulty: Original in a link race, the replay's own (NULL: a race, not a replay) in a replay, else the mode's file's level */
RaceDifficulty difficulty_resolve(int extras_level, const RaceDifficulty *replay, int linked);

/* The replay setup's stamp (bits 2..7 of the port's cheat byte: two bits an axis, 0 Original) and its reading back */
unsigned difficulty_stamp(const RaceDifficulty *d);
RaceDifficulty difficulty_unstamp(unsigned stamp);

/* ai_setup.c's params (107 words) after the exe's defaults: Easy's rivals run slower and catch up less, Hard's have no catch-up band;
 * the police's stop window and the traffic's speeds */
void difficulty_apply_params(const RaceDifficulty *d, int32_t params[107]);

/* The traffic cars a race starts with, `count` as the exe has it */
int32_t difficulty_traffic_count(const RaceDifficulty *d, int32_t count);

/* ai_manager.c's POLICE_ODDS[kind] (in 128), and ai_police.c's stops that end the race */
int32_t difficulty_police_odds(const RaceDifficulty *d, int kind, int32_t exe);
int32_t difficulty_stops_to_end(const RaceDifficulty *d, int32_t exe);

/* Hard's rivals: no profile catch-up band (1.0 at every gap), and bends taken faster */
int difficulty_rivals_flat(const RaceDifficulty *d);
int32_t difficulty_bend_scale(const RaceDifficulty *d, int32_t exe);

#endif
