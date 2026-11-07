/* The replay file's blocks (0044d420 writes them, 0044d514 reads them back) as the original's memory holds them,
 * to and from the port's RaceState and replay director: the setup 004fb7bc (0x348), the cuts 00512d94 (0x9a4), the
 * seek block 005120c4 (0xcc8) and the records 0051cc5c (9 x 0x1d2, which start 8 bytes into the first player
 * record 0051cc54 and end 8 bytes past the ninth). Bytes the port has no word for are written 0 and skipped on
 * reading. Port glue around ported functions: no oracle of its own. */
#ifndef TNFS_RACE_REPLAY_IMAGE_H
#define TNFS_RACE_REPLAY_IMAGE_H

#include "game/race/race_replay_file.h"
#include "game/race/race_replay_seek.h"
#include "game/race/race_sim.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    REPLAY_TRACK_NAME = 10,  /* 004fb7c6..004fb7cf: the track's stem ("tr6"), NUL padded */
    REPLAY_RECORDING_BYTES = 0x15e00,
    REPLAY_FILE_SIZE = REPLAY_SETUP_SIZE + REPLAY_CUTS_SIZE + REPLAY_RECORDING_BYTES + REPLAY_SEEK_SIZE +
                       REPLAY_RECORDS * REPLAY_RECORD_SIZE
};

/* port (replay colours): the colours the race's cars wore, after the exe's blocks: the 8 bytes "TNFSPNT1" and a paint a slot (PAINT_OWN 0, PAINT_PS1 1, PAINT_CATALOGUE + k; REPLAY_PAINT_NONE: not
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
    int snow, snow_known;              /* port (snow_weather): the race ran on snow (race_replay_snow_get), when the file says */
    int paint_known;                   /* ... 1 when the file has them (race_replay_paint_get) */
    int explode;                       /* port: 004c5cd4, the race's EXPLODE cheat, stamped in the setup's two bytes at
                                        * +0x3c (0 in the CD's replays and the exe's own); the horn then spins out cars */
} ReplayRace;

/* The setup's race; 0 when the setup does not hold one (no stem, counts out of range, a slot's car not a car) */
int race_replay_image_race(const uint8_t setup[REPLAY_SETUP_SIZE], ReplayRace *out);

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
