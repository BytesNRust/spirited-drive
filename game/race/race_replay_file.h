/* The replay's file: written at the race's end (0044d420) and read back (0044d514). */
#ifndef TNFS_RACE_REPLAY_FILE_H
#define TNFS_RACE_REPLAY_FILE_H

#include <stdint.h>

/* The C library's file calls (watcall): fopen 004821ce (name, mode), fread
 * 00487fe1 / fwrite 0048d5b2 (buffer, size, count, file), fclose 00482335. */
typedef struct {
    void *ctx;
    void *(*open)(void *ctx, const char *name, const char *mode);
    void (*read)(void *ctx, void *buffer, uint32_t size, uint32_t count, void *file);
    void (*write)(void *ctx, const void *buffer, uint32_t size, uint32_t count, void *file);
    int32_t (*close)(void *ctx, void *file);
} RaceReplayFileOps;

enum {
    REPLAY_SETUP_SIZE = 0x348,   /* 004fb7bc..004fbb04: the race's setup, its entries at +0x3e (004fb7fa) */
    REPLAY_CUTS_SIZE = 0x9a4,    /* 00512d94 */
    REPLAY_SEEK_SIZE = 0xcc8,    /* 005120c4 */
    REPLAY_RECORDS = 9,          /* 0051cc5c..0051dcbe, 0x1d2 each */
    REPLAY_RECORD_SIZE = 0x1d2
};

typedef struct {
    const char *name;     /* 00513744 */
    uint8_t *setup;       /* 004fb7bc, REPLAY_SETUP_SIZE */
    int32_t *followed;    /* 004fbb08 */
    int16_t sound_open;   /* 0052b30e (0052b30c's high word) */
    int16_t music_volume; /* 00524e65 (00524e63's high word) */
    int16_t sound_volume; /* 00524e69 (00524e67's high word) */
    uint8_t *cuts;        /* 00512d94, REPLAY_CUTS_SIZE */
    uint8_t *recording;   /* [004c53b4], 0x15e00 */
    uint8_t *seek;        /* 005120c4, REPLAY_SEEK_SIZE */
    uint8_t *records;     /* 0051cc5c, REPLAY_RECORDS x REPLAY_RECORD_SIZE */
    int32_t *loaded;      /* 004c53b8 */
} RaceReplayLoad;

/* 0044d514 (from 0044f9b8, request 4 / 5 / 8): the file `name` opened "rb"
 * (its handle not checked) and read in order: the setup (0x348); then the
 * followed player 004fbb08 = 004fbacc (setup +0x310) and that entry's sound
 * volume / music volume (+0x1d / +0x21) the game's own when the sound is
 * open, else 0, and its sound_on (+0x29) the sound's open word; the cuts
 * (0x9a4), the recording (0x15e00), the seek block (0xcc8) and the nine
 * records, one 0x1d2 read each; closed; 004c53b8 = 1. Returns fclose's.
 * An entry past the setup's end (the original writes wherever it falls) is
 * left alone. */
int32_t race_replay_load(const RaceReplayLoad *in, const RaceReplayFileOps *ops);

typedef struct {
    const char *name;         /* 00513744 */
    uint8_t *setup;           /* 004fb7bc, REPLAY_SETUP_SIZE */
    int32_t *followed;        /* 004fbb08 */
    int32_t *request;         /* 00513738 */
    int32_t *unknown_509d70;  /* 00509d70 */
    int32_t *loaded;          /* 004c53b8 */
} RaceReplaySetupRead;

/* 0044d3b0 (watcall (), from 004275f8 for a replay): the file `name` opened
 * "rb"; none: the request 00513738 = 6 and 00509d70 = 0x20; else its first
 * 0x348 bytes read into the setup. Then 004fbb08 = 004fbacc (setup +0x310),
 * fclose (on the handle even when none came), 004c53b8 = 1. Returns
 * fclose's. */
int32_t race_replay_setup_read(const RaceReplaySetupRead *in, const RaceReplayFileOps *ops);

enum { REPLAY_PATH_SIZE = 0x50 };  /* 0044d420's buffer, as each of the paths 00524328.. */

typedef struct {
    int32_t w4fbb0c;           /* 004fbb0c */
    int16_t playback;          /* 0052b312 */
    const char *dir;           /* 00524418: the replays' directory */
    uint8_t *setup;            /* 004fb7bc, REPLAY_SETUP_SIZE */
    int32_t followed;          /* 004fbb08 */
    const uint8_t *cuts;       /* 00512d94 */
    const uint8_t *recording;  /* [004c53b4] */
    const uint8_t *seek;       /* 005120c4 */
    const uint8_t *records;    /* 0051cc5c */
} RaceReplaySave;

/* 0044d420 (watcall (), from 0044eef8): with 004fbb0c set and playback not
 * 1, the path 00524418 + "Replay.rpl" (sprintf "%s%s" into 0x50 bytes; a
 * longer one runs past the original's buffer, the port cuts it) opened
 * "wb"; opened: 004fbacc (setup +0x310) = 004fbb08, then written as
 * 0044d514 reads them back (the setup, the cuts, the recording, the seek
 * block, the nine records one at a time), closed. */
void race_replay_save(const RaceReplaySave *in, const RaceReplayFileOps *ops);

#endif
