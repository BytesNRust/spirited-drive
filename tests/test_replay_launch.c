/* The NITRO start in a replay (race_replay.h, port_replay_launch): the player's N rides in the recording as both gear
 * bits at once, the exe's launch signal that input_tick takes out of the live word, and is taken out again when the
 * recording is played back. A recording made without the signal -- every replay the exe wrote, the CD's among them --
 * plays back as it always did: its setup has no nitro stamp, so no word is split (the exe's own words may hold both
 * gear bits) and no launch is asked for.
 *   test_replay_launch [REPLAY.RPL ...] */
#include "game/race/race_replay.h"
#include "game/race/race_replay_image.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BOTH_GEARS = 0x3000, HUMANS = 2 };

static void words(void)
{
    uint32_t w = 0x07a5;
    CHECK_EQ_INT(race_replay_launch_word(w, 0), w);
    CHECK_EQ_INT(race_replay_launch_word(w, 1), w | BOTH_GEARS);
    CHECK_EQ_INT(race_replay_launch_word(0x1020, 1), 0x1020);   /* a gear key in the word keeps it: no signal then */
    CHECK_EQ_INT(race_replay_launch_word(0x2020, 1), 0x2020);
    CHECK(!race_replay_launch_split(&w));
    CHECK_EQ_INT(w, 0x07a5);
    w = race_replay_launch_word(0x47a5, 1);   /* the horn and the pedal stay */
    CHECK(race_replay_launch_split(&w));
    CHECK_EQ_INT(w, 0x47a5);
    w = 0x1000 | 0x20;                        /* one gear bit is a gear, not a launch */
    CHECK(!race_replay_launch_split(&w));
    CHECK_EQ_INT(w, 0x1020);
}

/* recorded with the signal on some ticks and played back: the same ticks give it, the words are the live ones */
static void round_trip(void)
{
    static uint8_t recording[REPLAY_RECORDING_SIZE];
    RaceReplayWords w = { 1, 0, HUMANS, 0 };
    for (int tick = 0; tick < 40; tick += 2) {
        w.clock = tick * 2;
        uint32_t inputs[HUMANS] = { 0x20u + (uint32_t)tick, 0x7a0u };
        inputs[0] = race_replay_launch_word(inputs[0], tick >= 10 && tick < 30);
        race_replay_record(&w, inputs, recording, sizeof recording);
    }
    w.playback = 1;
    for (int tick = 0; tick < 40; tick += 2) {
        w.clock = tick * 2;
        uint32_t inputs[HUMANS] = { 0, 0 };
        race_replay_play(&w, inputs, recording, sizeof recording);
        const int launching = race_replay_launch_split(&inputs[0]);
        CHECK_EQ_INT(launching, tick >= 10 && tick < 30);
        CHECK_EQ_INT(inputs[0], 0x20u + (uint32_t)tick);
        CHECK(!race_replay_launch_split(&inputs[1]));
        CHECK_EQ_INT(inputs[1], 0x7a0);
    }
}

/* a replay file the exe wrote: no nitro stamp in its setup */
static void old_recording(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        CHECK(0);
        return;
    }
    static uint8_t file[REPLAY_FILE_SIZE];
    const size_t n = fread(file, 1, sizeof file, fp);
    fclose(fp);
    CHECK_EQ_INT(n, REPLAY_FILE_SIZE);
    const uint8_t *recording = file + REPLAY_SETUP_SIZE + REPLAY_CUTS_SIZE;
    ReplayRace rr;
    CHECK(race_replay_image_race(file, &rr));
    CHECK(!rr.nitro);   /* no stamp: drive_replay.c leaves every word as it is (start_input stays 0) */
    int both = 0;
    const RaceReplayWords w0 = { 1, 1, HUMANS, 0 };
    for (int slot = 0; slot < REPLAY_SLOTS; slot++) {
        RaceReplayWords w = w0;
        w.clock = slot * 4;
        uint32_t inputs[HUMANS] = { 0 };
        race_replay_play(&w, inputs, recording, REPLAY_RECORDING_SIZE);
        for (int i = 0; i < HUMANS; i++) both += (inputs[i] & BOTH_GEARS) == BOTH_GEARS;
    }
    printf("%s: no nitro stamp, %d words with both gear bits (the exe's own, kept)\n", path, both);
}

int main(int argc, char **argv)
{
    words();
    round_trip();
    for (int i = 1; i < argc; i++) old_recording(argv[i]);
    return TEST_REPORT();
}
