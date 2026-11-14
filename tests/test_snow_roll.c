/* port (snow): the roll on snow (snow_roll.h): silent standing, rising with speed more steeply than the wet roll, 6 dB lower in the cockpit; the sample and its licence entry. */
#include "game/race/snow_roll.h"

#include "game/race/rain_roll.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t metres(int m) { return m * 0x10000; }

enum { OPEN = 16 };

static void test_volume_curve(void)
{
    CHECK_EQ_INT(snow_roll_volume(1, OPEN, 0, 1, 0), 0);                  /* standing: silent */
    CHECK_EQ_INT(snow_roll_volume(1, OPEN, SNOW_ROLL_MOVING, 1, 0), 0);   /* creeping */
    int last = 0;
    for (int m = 3; m <= 60; m++) {   /* rises with speed, never falls */
        const int v = snow_roll_volume(1, OPEN, metres(m), 1, 0);
        CHECK(v > 0 && v >= last);
        last = v;
    }
    CHECK_EQ_INT(snow_roll_volume(1, OPEN, SNOW_ROLL_FULL, 1, 0), SNOW_ROLL_MAX_PERCENT);
    CHECK_EQ_INT(snow_roll_volume(1, OPEN, metres(100), 1, 0), SNOW_ROLL_MAX_PERCENT);   /* and no louder beyond it */
    CHECK_EQ_INT(snow_roll_volume(1, OPEN, -metres(20), 1, 0), snow_roll_volume(1, OPEN, metres(20), 1, 0));   /* reversing counts as speed */
    CHECK_EQ_INT(snow_roll_volume(1, OPEN, metres(20), 0, 0), 0);   /* in the air: nothing */
    CHECK_EQ_INT(snow_roll_volume(0, OPEN, metres(20), 1, 0), 0);   /* not on snow */
    CHECK_EQ_INT(snow_roll_volume(1, 0, metres(20), 1, 0), 0);      /* a closed zone */
    CHECK_EQ_INT(snow_roll_volume(1, OPEN / 2, metres(40), 1, 0), snow_roll_volume(1, OPEN, metres(40), 1, 0) / 2);
}

/* louder outside, 6 dB lower in the cockpit */
static void test_cockpit_is_quieter(void)
{
    for (int m = 5; m <= 60; m += 5) {
        const int out = snow_roll_volume(1, OPEN, metres(m), 1, 0), in = snow_roll_volume(1, OPEN, metres(m), 1, 1);
        CHECK(in == out / 2 && in > 0);
    }
}

/* a friction rising with speed: the level climbs from nothing and the pitch spans more than the wet roll's (rain_roll.h) */
static void test_rises_more_than_the_wet_roll(void)
{
    CHECK(SNOW_ROLL_PITCH_FAST - SNOW_ROLL_PITCH_SLOW > RAIN_ROLL_PITCH_FAST - RAIN_ROLL_PITCH_SLOW);
    const int slow = snow_roll_volume(1, OPEN, metres(15), 1, 0), fast = snow_roll_volume(1, OPEN, metres(50), 1, 0);
    const int wet_slow = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, metres(15), RAIN_ROLL_ASPHALT, 0).spray, wet_fast = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, metres(50), RAIN_ROLL_ASPHALT, 0).spray;
    CHECK(fast * wet_slow > wet_fast * slow);   /* fast / slow above the wet roll's, from 54 to 180 km/h */
}

static void test_pitch(void)
{
    CHECK_EQ_INT(snow_roll_pitch(0), SNOW_ROLL_PITCH_SLOW);
    CHECK_EQ_INT(snow_roll_pitch(SNOW_ROLL_FULL), SNOW_ROLL_PITCH_FAST);
    CHECK_EQ_INT(snow_roll_pitch(metres(100)), SNOW_ROLL_PITCH_FAST);
    CHECK(snow_roll_pitch(metres(20)) > snow_roll_pitch(metres(10)) && snow_roll_pitch(-metres(20)) == snow_roll_pitch(metres(20)));
}

/* the sample and its licence entry: only CC0 or public domain goes in data/sounds, with what, how it was made, the source, the author, the licence and the check date */
static void test_licence_entry(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/data/sounds/LICENSES.md", TNFS_SOURCE_DIR);
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f == NULL) return;
    char *text = calloc(1, 1 << 16);
    const size_t n = fread(text, 1, (1 << 16) - 1, f);
    fclose(f);
    text[n] = '\0';
    const char *entry = strstr(text, "## snow_roll.wav");
    CHECK(entry != NULL);
    if (entry != NULL) {
        const char *next = strstr(entry + 3, "\n## ");
        char *part = strndup(entry, next != NULL ? (size_t)(next - entry) : strlen(entry));
        CHECK(strstr(part, "- What:") && strstr(part, "- Made from:") && strstr(part, "- Source: https://") && strstr(part, "- Author:") && strstr(part, "- Licence:") && strstr(part, "Checked on the page on 20"));
        CHECK(strstr(part, "Creative Commons 0") != NULL && strstr(part, "creativecommons.org/publicdomain/zero/1.0") != NULL);
        free(part);
    }
    free(text);
    snprintf(path, sizeof path, "%s/data/sounds/snow_roll.wav", TNFS_SOURCE_DIR);   /* and the file is a 16-bit mono WAV at 22050 Hz */
    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f == NULL) return;
    unsigned char head[44];
    CHECK_EQ_INT((int)fread(head, 1, sizeof head, f), 44);
    fclose(f);
    CHECK(memcmp(head, "RIFF", 4) == 0 && memcmp(head + 8, "WAVEfmt ", 8) == 0 && head[22] == 1 && (head[24] | head[25] << 8) == 22050 && head[34] == 16);
}

int main(void)
{
    test_volume_curve();
    test_cockpit_is_quieter();
    test_rises_more_than_the_wet_roll();
    test_pitch();
    test_licence_entry();
    return TEST_REPORT();
}
