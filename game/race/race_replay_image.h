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

/* A replay's race as its setup tells it */
typedef struct {
    char stem[REPLAY_TRACK_NAME + 1];  /* upper case, as the CD's names */
    RaceField field;                   /* racers, cops, traffic, the seed 004fb7f4, the race type, the entries */
    int laps;                          /* 004fbac4 */
    int followed;                      /* 004fbacc: the car the camera follows */
    uint32_t track_flags;              /* 004fb7d8 */
} ReplayRace;

/* The setup's race; 0 when the setup does not hold one (no stem, counts out of range, a slot's car not a car) */
int race_replay_image_race(const uint8_t setup[REPLAY_SETUP_SIZE], ReplayRace *out);

/* The setup of `race` (started from the seed `seed`) as 004fb7bc holds it */
void race_replay_image_setup(const RaceState *race, uint32_t seed, uint8_t setup[REPLAY_SETUP_SIZE]);

/* The cuts, the seek block and the records out of the race and the director, and back in */
void race_replay_image_put(const RaceState *race, const RaceReplayDirector *dir, uint8_t cuts[REPLAY_CUTS_SIZE],
                           uint8_t seek[REPLAY_SEEK_SIZE], uint8_t records[REPLAY_RECORDS * REPLAY_RECORD_SIZE]);
void race_replay_image_get(RaceState *race, RaceReplayDirector *dir, const uint8_t cuts[REPLAY_CUTS_SIZE],
                           const uint8_t seek[REPLAY_SEEK_SIZE],
                           const uint8_t records[REPLAY_RECORDS * REPLAY_RECORD_SIZE]);

#endif
