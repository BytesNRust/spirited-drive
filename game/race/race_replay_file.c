/* The replay's file (race_replay_file.h). */
#include "game/race/race_replay_file.h"

#include "game/race/race_replay.h"

#include <stdio.h>
#include <string.h>

enum {
    SETUP_LOCAL = 0x310,   /* 004fbacc */
    SETUP_ENTRIES = 0x3e,  /* 004fb7fa */
    ENTRY_SIZE = 0x4b,
    ENTRY_SOUND_VOLUME = 0x1d, ENTRY_MUSIC_VOLUME = 0x21, ENTRY_SOUND_ON = 0x29
};

static const char READ_BINARY[] = "rb";  /* 004b06ac */
static const char WRITE_BINARY[] = "wb"; /* 004b06c4 */
static const char REPLAY_NAME[] = "Replay.rpl";  /* 004b06b0 */

static int32_t setup_word(const uint8_t *setup, uint32_t at)
{
    int32_t v;
    memcpy(&v, setup + at, sizeof v);
    return v;
}

static void setup_put(uint8_t *setup, uint32_t at, int32_t v) { memcpy(setup + at, &v, sizeof v); }

/* The followed player's entry takes the game's sound settings, not the file's */
static void keep_sound(const RaceReplayLoad *in)
{
    const int32_t player = setup_word(in->setup, SETUP_LOCAL);
    *in->followed = player;
    const int64_t at = SETUP_ENTRIES + (int64_t)player * ENTRY_SIZE;
    if (at < 0 || at + ENTRY_SOUND_ON + 4 > REPLAY_SETUP_SIZE) return;
    const uint32_t entry = (uint32_t)at;
    setup_put(in->setup, entry + ENTRY_SOUND_VOLUME, in->sound_open != 0 ? in->sound_volume : 0);
    setup_put(in->setup, entry + ENTRY_MUSIC_VOLUME, in->sound_open != 0 ? in->music_volume : 0);
    setup_put(in->setup, entry + ENTRY_SOUND_ON, in->sound_open);
}

int32_t race_replay_load(const RaceReplayLoad *in, const RaceReplayFileOps *ops)
{
    void *file = ops->open(ops->ctx, in->name, READ_BINARY);
    ops->read(ops->ctx, in->setup, REPLAY_SETUP_SIZE, 1, file);
    keep_sound(in);
    ops->read(ops->ctx, in->cuts, REPLAY_CUTS_SIZE, 1, file);
    ops->read(ops->ctx, in->recording, REPLAY_RECORDING_SIZE, 1, file);
    ops->read(ops->ctx, in->seek, REPLAY_SEEK_SIZE, 1, file);
    for (int k = 0; k < REPLAY_RECORDS; k++) ops->read(ops->ctx, in->records + k * REPLAY_RECORD_SIZE, REPLAY_RECORD_SIZE, 1, file);
    const int32_t closed = ops->close(ops->ctx, file);
    *in->loaded = 1;
    return closed;
}

void race_replay_save(const RaceReplaySave *in, const RaceReplayFileOps *ops)
{
    if (in->w4fbb0c == 0 || in->playback == 1) return;
    char path[REPLAY_PATH_SIZE];
    snprintf(path, sizeof path, "%s%s", in->dir, REPLAY_NAME);
    void *file = ops->open(ops->ctx, path, WRITE_BINARY);
    if (file == NULL) return;
    setup_put(in->setup, SETUP_LOCAL, in->followed);
    ops->write(ops->ctx, in->setup, REPLAY_SETUP_SIZE, 1, file);
    ops->write(ops->ctx, in->cuts, REPLAY_CUTS_SIZE, 1, file);
    ops->write(ops->ctx, in->recording, REPLAY_RECORDING_SIZE, 1, file);
    ops->write(ops->ctx, in->seek, REPLAY_SEEK_SIZE, 1, file);
    for (int k = 0; k < REPLAY_RECORDS; k++) ops->write(ops->ctx, in->records + k * REPLAY_RECORD_SIZE, REPLAY_RECORD_SIZE, 1, file);
    ops->close(ops->ctx, file);
}

enum { SETUP_READ_FAILED = 6, SETUP_READ_STATE = 0x20, SETUP_FOLLOWED = 0x310 };

int32_t race_replay_setup_read(const RaceReplaySetupRead *in, const RaceReplayFileOps *ops)
{
    void *file = ops->open(ops->ctx, in->name, "rb");
    if (file == NULL) {
        *in->request = SETUP_READ_FAILED;
        *in->unknown_509d70 = SETUP_READ_STATE;
    } else {
        ops->read(ops->ctx, in->setup, REPLAY_SETUP_SIZE, 1, file);
    }
    const uint8_t *f = in->setup + SETUP_FOLLOWED;
    *in->followed = (int32_t)((uint32_t)f[0] | (uint32_t)f[1] << 8 | (uint32_t)f[2] << 16 | (uint32_t)f[3] << 24);
    const int32_t closed = ops->close(ops->ctx, file);
    *in->loaded = 1;
    return closed;
}
