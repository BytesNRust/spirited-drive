/* the rain's three layers (rain_roll.h): stopped outside, stopped in the cockpit, the wet road moving; and the samples' licence entries */
#include "tests/test.h"

#include "game/race/rain_roll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OPEN = 16 };

static int32_t ms(int m) { return m * 0x10000; }

static void test_stopped_and_moving_layers(void)
{
    /* stopped: the rain round the car outside, on the roof in the cockpit; no wet road yet */
    RainRoll r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, 0, RAIN_ROLL_ASPHALT, 0);
    CHECK(r.out == RAIN_ROLL_OUT_PERCENT && r.in == 0 && r.moving == 0);
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, 0, RAIN_ROLL_ASPHALT, 1);
    CHECK(r.out == 0 && r.in == RAIN_ROLL_IN_PERCENT && r.moving == 0);
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_MOVING, RAIN_ROLL_ASPHALT, 0);   /* creeping is still stopped */
    CHECK(r.out == RAIN_ROLL_OUT_PERCENT && r.moving == 0);
    /* past the cross-fade: the moving layer alone, 6 dB lower in the cockpit */
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_CROSSED, RAIN_ROLL_ASPHALT, 0);
    CHECK(r.out == 0 && r.in == 0 && r.moving == RAIN_ROLL_MOVE_LOW_PERCENT);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_CROSSED, RAIN_ROLL_ASPHALT, 1).moving, RAIN_ROLL_MOVE_LOW_PERCENT / 2);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_FULL, RAIN_ROLL_ASPHALT, 0).moving, RAIN_ROLL_MOVE_TOP_PERCENT);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_FULL * 2, RAIN_ROLL_ASPHALT, 0).moving, RAIN_ROLL_MOVE_TOP_PERCENT);   /* the top holds */
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, -RAIN_ROLL_FULL, RAIN_ROLL_ASPHALT, 0).moving, RAIN_ROLL_MOVE_TOP_PERCENT);      /* reversing is as loud */
}

/* the cross-fade is equal power: the stopped layer falls as the moving one rises, the sum of their squares (each against its own end) stays 1 */
static void test_equal_power_cross_fade(void)
{
    int last_out = RAIN_ROLL_OUT_PERCENT + 1, last_moving = -1;
    for (int32_t v = RAIN_ROLL_MOVING; v <= RAIN_ROLL_CROSSED; v += 0x8000) {
        const RainRoll r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, v, RAIN_ROLL_ASPHALT, 0);
        CHECK(r.out <= last_out && r.moving >= last_moving);
        last_out = r.out, last_moving = r.moving;
        const double a = (double)r.out / RAIN_ROLL_OUT_PERCENT, b = (double)r.moving / RAIN_ROLL_MOVE_LOW_PERCENT, sum = a * a + b * b;
        CHECK(sum > 0.9 && sum < 1.1);
    }
    int last = 0;
    for (int m = 15; m <= 70; m++) {   /* past it the moving layer only rises */
        const int level = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_ASPHALT, 0).moving;
        CHECK(level >= last && level <= RAIN_ROLL_MOVE_TOP_PERCENT);
        last = level;
    }
}

static void test_silent_and_scaled(void)
{
    for (int cockpit = 0; cockpit <= 1; cockpit++)
        for (int m = 0; m <= 60; m += 10) {
            RainRoll r = rain_roll_levels(RAIN_ROLL_DRY, OPEN, ms(m), RAIN_ROLL_ASPHALT, cockpit);   /* dry (clear, fog) */
            CHECK(r.out == 0 && r.in == 0 && r.moving == 0);
            r = rain_roll_levels(RAIN_ROLL_RAIN, 0, ms(m), RAIN_ROLL_ASPHALT, cockpit);               /* inside a closed zone */
            CHECK(r.out == 0 && r.in == 0 && r.moving == 0);
        }
    /* off the asphalt (gravel, grass, the air) the wet road is silent, the rain still falls */
    const RainRoll off = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(5), RAIN_ROLL_OFF, 0);
    CHECK(off.moving == 0 && off.out > 0);
    /* falling snow is silent standing, but melts on the road: the wet road as in rain, no stopped layer */
    for (int cockpit = 0; cockpit <= 1; cockpit++) {
        const RainRoll still = rain_roll_levels(RAIN_ROLL_SNOW, OPEN, 0, RAIN_ROLL_ASPHALT, cockpit), moving = rain_roll_levels(RAIN_ROLL_SNOW, OPEN, ms(40), RAIN_ROLL_ASPHALT, cockpit);
        CHECK(still.out == 0 && still.in == 0 && still.moving == 0);
        CHECK(moving.out == 0 && moving.in == 0 && moving.moving == rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(40), RAIN_ROLL_ASPHALT, cockpit).moving);
    }
    /* the mouth of a closed zone: the openness scales every layer */
    const RainRoll full = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(40), RAIN_ROLL_ASPHALT, 0), half = rain_roll_levels(RAIN_ROLL_RAIN, OPEN / 2, ms(40), RAIN_ROLL_ASPHALT, 0);
    CHECK(half.moving == (full.moving + 1) / 2 || half.moving == full.moving / 2);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN / 2, 0, RAIN_ROLL_ASPHALT, 1).in, RAIN_ROLL_IN_PERCENT / 2);
}

/* on the rally's gravel the wet road is a layer under the gravel roll: RAIN_ROLL_GRAVEL_PERCENT of the asphalt's, the stopped layer as on asphalt */
static void test_gravel_is_under_the_roll(void)
{
    for (int m = 10; m <= 60; m += 10)
        for (int cockpit = 0; cockpit <= 1; cockpit++) {
            const int asphalt = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_ASPHALT, cockpit).moving, gravel = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_GRAVEL, cockpit).moving;
            CHECK(gravel > 0 && gravel < asphalt && abs(gravel - asphalt * RAIN_ROLL_GRAVEL_PERCENT / 100) <= 1);
            CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_SNOW, OPEN, ms(m), RAIN_ROLL_GRAVEL, cockpit).moving, gravel);   /* snow falling on the rally: the same */
        }
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, 0, RAIN_ROLL_GRAVEL, 0).out, RAIN_ROLL_OUT_PERCENT);
}

static void test_pitch(void)
{
    CHECK_EQ_INT(rain_roll_pitch(0), RAIN_ROLL_PITCH_SLOW);
    CHECK_EQ_INT(rain_roll_pitch(RAIN_ROLL_FULL), RAIN_ROLL_PITCH_FAST);
    CHECK_EQ_INT(rain_roll_pitch(RAIN_ROLL_FULL * 3), RAIN_ROLL_PITCH_FAST);
    CHECK(rain_roll_pitch(20 * 0x10000) > rain_roll_pitch(10 * 0x10000) && rain_roll_pitch(-30 * 0x10000) == rain_roll_pitch(30 * 0x10000));
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(30), RAIN_ROLL_ASPHALT, 0).pitch, rain_roll_pitch(ms(30)));
}

/* the sample and its licence entry: Only CC0 or public domain goes in data/sounds, with what, how it was made, the source, the author, the licence and the check date */
static void licence_entry(const char *name)
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
    char heading[64];
    snprintf(heading, sizeof heading, "## %s", name);
    const char *entry = strstr(text, heading);
    CHECK(entry != NULL);
    if (entry != NULL) {
        const char *next = strstr(entry + 3, "\n## ");
        char *part = strndup(entry, next != NULL ? (size_t)(next - entry) : strlen(entry));
        CHECK(strstr(part, "- What:") && strstr(part, "- Made from:") && strstr(part, "- Source: https://") && strstr(part, "- Author:") && strstr(part, "- Licence:") && strstr(part, "Checked on the page on 20"));
        CHECK(strstr(part, "Creative Commons 0") != NULL && strstr(part, "creativecommons.org/publicdomain/zero/1.0") != NULL);
        free(part);
    }
    free(text);
    snprintf(path, sizeof path, "%s/data/sounds/%s", TNFS_SOURCE_DIR, name);   /* and the file is a 16-bit mono WAV at 22050 Hz */
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
    test_stopped_and_moving_layers();
    test_equal_power_cross_fade();
    test_silent_and_scaled();
    test_gravel_is_under_the_roll();
    test_pitch();
    licence_entry("rain_stopped_out.wav");
    licence_entry("rain_stopped_in.wav");
    licence_entry("rain_tyre_roll.wav");
    return TEST_REPORT();
}
