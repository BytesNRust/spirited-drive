/* The replay file's blocks (0044d420 writes them, 0044d514 reads them back) as the original's memory holds them,
 * to and from the port's RaceState and replay director: the setup 004fb7bc (0x348), the cuts 00512d94 (0x9a4), the
 * seek block 005120c4 (0xcc8) and the records 0051cc5c (9 x 0x1d2, which start 8 bytes into the first player
 * record 0051cc54 and end 8 bytes past the ninth). Bytes the port has no word for are written 0 and skipped on
 * reading. Port glue around ported functions: no oracle of its own. */
#ifndef TNFS_RACE_REPLAY_IMAGE_H
#define TNFS_RACE_REPLAY_IMAGE_H

#include "formats/car_piece.h"
#include "formats/mod_ini.h"
#include "game/race/race_replay_file.h"
#include "game/race/race_replay_seek.h"
#include "game/race/race_sim.h"
#include "game/race/race_state.h"
#include "game/sys/track_registry.h"

#include <stdint.h>

enum {
    REPLAY_TRACK_NAME = 10,  /* 004fb7c6..004fb7cf: the track's stem ("tr6"), NUL padded */
    REPLAY_RECORDING_BYTES = 0x15e00,
    REPLAY_FILE_SIZE = REPLAY_SETUP_SIZE + REPLAY_CUTS_SIZE + REPLAY_RECORDING_BYTES + REPLAY_SEEK_SIZE +
                       REPLAY_RECORDS * REPLAY_RECORD_SIZE
};

/* port (replay colours): the colours the race's cars wore, after the exe's blocks: the 8 bytes "TNFSPNT1" and a paint a slot (PAINT_OWN 0, PAINT_SECOND 1, PAINT_CATALOGUE + k; REPLAY_PAINT_NONE: not
 * recorded). The exe reads its blocks one at a time and never looks past the records, so it plays such a file as before; a file without them (the CD's, an older port's) has the exact size of the
 * exe's blocks and its cars take the colours the race rules give. */
enum { REPLAY_PAINT_TRAILER_SIZE = 16, REPLAY_PAINT_NONE = 0xff };
void race_replay_paint_put(const uint8_t paint[RACE_PLAYERS], uint8_t out[REPLAY_PAINT_TRAILER_SIZE]);
/* 1 when `file` (size bytes) is a replay's with its colours: `paint` gets them; 0 and REPLAY_PAINT_NONE everywhere otherwise */
int race_replay_paint_get(const uint8_t *file, size_t size, uint8_t paint[RACE_PLAYERS]);

/* port (snow_weather): after the paints, the 8 bytes "TNFSSNW1", 1 when the race ran on snow (grip and all: weather_snow.h) else 0, and padding. A file without it (the CD's, an older port's) has no snow. */
enum { REPLAY_SNOW_TRAILER_SIZE = 16 };
void race_replay_snow_put(int snow, uint8_t out[REPLAY_SNOW_TRAILER_SIZE]);
/* 1 when `file` has the snow record (after the paint trailer): `*snow` is it; else 0 and `*snow` 0 */
int race_replay_snow_get(const uint8_t *file, size_t size, int *snow);

/* port (replay conditions): after the snow record, the 8 bytes "TNFSCND1", the time of day (port_light_time's 0..3), the weather (0..3) and whether the track ran reversed (0/1), and padding. A file without it (the
 * CD's demos, an older port's) was recorded as the original plays: day, clear, forward. */
enum { REPLAY_CONDITIONS_TRAILER_SIZE = 16 };
/* port (replay extras): in the record's padding, a marker (1) and the moon's textures (0/1, the mode's file's lunar_springs the race
 * had), the moon's gravity (0/1, its port_lunar_gravity) and the rally's gravel (its port_rally_gravel, RALLY_GRAVEL_*): a replay plays
 * with them. An older port's record has 0 there: extras_known 0, and the replay takes the switches as they are now, as before. */
typedef struct { int light_time, weather, reverse, extras_known, moon, lunar_gravity, gravel; } ReplayConditions;
void race_replay_conditions_put(const ReplayConditions *c, uint8_t out[REPLAY_CONDITIONS_TRAILER_SIZE]);
/* 1 when `file` has the conditions record (after the snow trailer): `*c` is it; else 0 and `*c` is day, clear, forward */
int race_replay_conditions_get(const uint8_t *file, size_t size, ReplayConditions *c);

/* port (dep59 step 3): after the cars' block, the looks the added cars wore: the 8 bytes "TNFSLOK1" and, for each of the RACE_PLAYERS
 * entries, its skin's name (REPLAY_LOOK_SKIN bytes, NUL-padded; "" the skin as it comes) and its paint's (REPLAY_LOOK_PAINT; "" its own
 * colour); an original car's entry is all zeros. A Replay.rpx without it (dep59 1b's) shows each added car as it is picked now. */
enum { REPLAY_LOOK_SKIN = CAR_PIECE_SKIN_NAME_MAX + 1, REPLAY_LOOK_PAINT = MOD_INI_PAINT_NAME_MAX + 1,
       REPLAY_LOOKS_BLOCK = 8 + RACE_PLAYERS * (REPLAY_LOOK_SKIN + REPLAY_LOOK_PAINT) };
typedef struct {
    char skin[REPLAY_LOOK_SKIN];
    char paint[REPLAY_LOOK_PAINT];
} ReplayLook;
void race_replay_looks_put(const ReplayLook looks[RACE_PLAYERS], uint8_t out[REPLAY_LOOKS_BLOCK]);
/* 1 with `looks` from `block`; 0 and all "" for a block that is not one of ours (a name with no end, or a byte that is not printable
 * ASCII) */
int race_replay_looks_get(const uint8_t block[REPLAY_LOOKS_BLOCK], ReplayLook looks[RACE_PLAYERS]);

/* A replay's race as its setup tells it */
typedef struct {
    char stem[REPLAY_TRACK_NAME + 1];  /* upper case, as the CD's names */
    RaceField field;                   /* racers, cops, traffic, the seed 004fb7f4, the race type, the entries */
    int laps;                          /* 004fbac4 */
    int followed;                      /* 004fbacc: the car the camera follows */
    uint32_t track_flags;              /* 004fb7d8 */
    int arcade;                        /* 004fbadc (the field holds 004fbad8) */
    int nitro;                         /* port: 004faa50, the NITRO cheat, stamped beside it: the start the recording holds launches */
    uint8_t paint[RACE_PLAYERS];       /* port (replay colours): each slot's paint as the race wore it, REPLAY_PAINT_NONE where the file has none */
    ReplayConditions conditions;       /* port (replay conditions): the time of day, weather and direction it was recorded with; day, clear, forward without the record */
    int conditions_known;              /* ... 1 when the file has them */
    int snow, snow_known;              /* port (snow_weather): the race ran on snow (race_replay_snow_get), when the file says */
    int paint_known;                   /* ... 1 when the file has them (race_replay_paint_get) */
    ReplayLook looks[RACE_PLAYERS];    /* port (dep59 step 3): each added car's skin and paint as the race wore them */
    int looks_known;                   /* ... 1 when the Replay.rpx has them (race_replay_looks_get) */
    int added_track;                   /* port (dep59 step 5, T5): today's number of the added track it ran on (TNFSTRK1), 0 a CD's */
    int explode;                       /* port: 004c5cd4, the race's EXPLODE cheat, stamped in the setup's two bytes at
                                        * +0x3c (0 in the CD's replays and the exe's own); the horn then spins out cars */
} ReplayRace;

/* The setup's race; 0 when the setup does not hold one (no stem, counts out of range, a slot's car not a car) */
int race_replay_image_race(const uint8_t setup[REPLAY_SETUP_SIZE], ReplayRace *out);

/* port (dep59 1b): a race with an added car is kept as Replay.rpx: the same blocks and trailers, then this one, last: the 8 bytes
 * "TNFSCAR1" and, for each of the RACE_PLAYERS entries, the id of its added car (NUL-padded) and that car's fingerprint (u32 LE,
 * car_registry.h); an original car's entry is all zeros. The setup holds the added car's number of the run it was made in; reading puts
 * today's number of each id there. */
enum { REPLAY_CARS_MAGIC = 8, REPLAY_CARS_ID = 33, REPLAY_CARS_BLOCK = REPLAY_CARS_MAGIC + RACE_PLAYERS * (REPLAY_CARS_ID + 4),
       REPLAY_CARS_AT = REPLAY_FILE_SIZE + REPLAY_PAINT_TRAILER_SIZE + REPLAY_SNOW_TRAILER_SIZE + REPLAY_CONDITIONS_TRAILER_SIZE };
typedef enum { REPLAY_CARS_OK, REPLAY_CARS_BAD, REPLAY_CARS_MISSING, REPLAY_CARS_CHANGED } ReplayCars;
/* the block for `field` */
void race_replay_cars_write(uint8_t block[REPLAY_CARS_BLOCK], const RaceField *field);
/* today's number of each added car `block` names, into `field`'s entries; MISSING (no such car now) or CHANGED (its fingerprint is
 * another) with the id in `id_out` (REPLAY_CARS_ID bytes); BAD for a block that is not exactly one of ours */
ReplayCars race_replay_cars_read(const uint8_t block[REPLAY_CARS_BLOCK], RaceField *field, char *id_out);
/* race_replay_image_race of a setup whose added cars `block` names: BAD when the setup is no race, or names a car past the twelve the
 * block does not */
ReplayCars race_replay_image_race_cars(const uint8_t setup[REPLAY_SETUP_SIZE], const uint8_t block[REPLAY_CARS_BLOCK], ReplayRace *out,
                                       char *id_out);
/* port (dep59 step 3): a whole Replay.rpx of `size` bytes: race_replay_image_race_cars of its setup and its cars' block, and its looks
 * (`out->looks_known` 0 for 1b's files, which have none, or a looks' block that does not read); BAD for any other size. Call it with
 * the added cars of the mods in effect registered. T5: with the track's block past the looks, the track first (race_replay_track_read,
 * `out->added_track`; BAD when the setup's stem is not the piece's and a stage's digit); call it with the added tracks registered too. */
ReplayCars race_replay_file_cars(const uint8_t *file, size_t size, ReplayRace *out, char *id_out);

/* port (dep59 step 5, T5): a race on an added track is kept as Replay.rpx too: the cars' block (all its entries zeros without an added
 * car) and the looks' block, then this one, last: the 8 bytes "TNFSTRK1", the track's id (NUL-padded) and its fingerprint (u32 LE,
 * track_registry.h). The setup's stem is the piece's own and the stage's digit. An older binary knows no Replay.rpx of that size: its list
 * says "not a replay" and it plays nothing. The outcomes are the cars' (ReplayCars), with the track's id: the same "needs" and "changed". */
enum { REPLAY_TRACK_MAGIC = 8, REPLAY_TRACK_ID = 33, REPLAY_TRACK_BLOCK = REPLAY_TRACK_MAGIC + REPLAY_TRACK_ID + 4 };
/* the block for `piece` (an added track's, track_registry_piece) */
void race_replay_track_write(uint8_t block[REPLAY_TRACK_BLOCK], const TrackPiece *piece);
/* today's number of the track `block` names in `*track`; MISSING (not registered now) or CHANGED (its fingerprint is another) with the id
 * in `id_out` (REPLAY_TRACK_ID bytes); BAD for a block that is not exactly one of ours */
ReplayCars race_replay_track_read(const uint8_t block[REPLAY_TRACK_BLOCK], int *track, char *id_out);

/* A race started from a replay's setup, after race_sim_start_field: the cheat words the setup's recording was made with
 * that the exe's setup does not hold (the EXPLODE cheat), put back so the cars play as they did. */
void race_replay_apply_cheats(RaceState *race, const ReplayRace *replay);

/* The setup of `race` (started from the seed `seed`) as 004fb7bc holds it */
void race_replay_image_setup(const RaceState *race, uint32_t seed, uint8_t setup[REPLAY_SETUP_SIZE]);

/* The cuts, the seek block and the records out of the race and the director, and back in */
void race_replay_image_put(const RaceState *race, const RaceReplayDirector *dir, uint8_t cuts[REPLAY_CUTS_SIZE],
                           uint8_t seek[REPLAY_SEEK_SIZE], uint8_t records[REPLAY_RECORDS * REPLAY_RECORD_SIZE]);
void race_replay_image_get(RaceState *race, RaceReplayDirector *dir, const uint8_t cuts[REPLAY_CUTS_SIZE],
                           const uint8_t seek[REPLAY_SEEK_SIZE],
                           const uint8_t records[REPLAY_RECORDS * REPLAY_RECORD_SIZE]);

#endif
