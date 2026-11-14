/* the rain's layers (rain_roll.h): stopped and moving, outside and in the cockpit, and a car's spray by where it is heard from; and the samples' licence entries */
#include "tests/test.h"

#include "game/race/rain_roll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OPEN = 16 };

static int32_t ms(int m) { return m * 0x10000; }

static void test_stopped_and_moving_layers(void)
{
    /* stopped: the rain round the car outside, on the roof in the cockpit; nothing moving yet */
    RainRoll r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, 0, RAIN_ROLL_ASPHALT, 0);
    CHECK(r.out == RAIN_ROLL_OUT_PERCENT && r.in == 0 && r.bed == 0 && r.move_in == 0 && r.spray == 0);
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, 0, RAIN_ROLL_ASPHALT, 1);
    CHECK(r.out == 0 && r.in == RAIN_ROLL_IN_PERCENT && r.bed == 0 && r.move_in == 0 && r.spray == 0);
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_MOVING, RAIN_ROLL_ASPHALT, 0);   /* creeping is still stopped */
    CHECK(r.out == RAIN_ROLL_OUT_PERCENT && r.bed == 0 && r.spray == 0);
    /* past the cross-fade outside: the rain round a moving car and the car's spray */
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_CROSSED, RAIN_ROLL_ASPHALT, 0);
    CHECK(r.out == 0 && r.in == 0 && r.move_in == 0 && r.bed == RAIN_ROLL_BED_PERCENT && r.spray == RAIN_ROLL_SPRAY_LOW_PERCENT);
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_FULL, RAIN_ROLL_ASPHALT, 0);
    CHECK(r.bed == RAIN_ROLL_BED_PERCENT && r.spray == RAIN_ROLL_SPRAY_TOP_PERCENT);   /* the rain round the car does not rise: the spray does */
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_FULL * 2, RAIN_ROLL_ASPHALT, 0).spray, RAIN_ROLL_SPRAY_TOP_PERCENT);   /* the top holds */
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, -RAIN_ROLL_FULL, RAIN_ROLL_ASPHALT, 0).spray, RAIN_ROLL_SPRAY_TOP_PERCENT);      /* reversing is as loud */
    /* in the cockpit: the rain on the moving car, its own spray in it (no spray of its own) */
    r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_CROSSED, RAIN_ROLL_ASPHALT, 1);
    CHECK(r.in == 0 && r.out == 0 && r.bed == 0 && r.spray == 0 && r.move_in == RAIN_ROLL_MOVE_IN_LOW_PERCENT);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, RAIN_ROLL_FULL, RAIN_ROLL_ASPHALT, 1).move_in, RAIN_ROLL_MOVE_IN_TOP_PERCENT);
    /* the followed car's spray is any car's */
    for (int m = 0; m <= 70; m += 5) CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_ASPHALT, 0).spray, rain_roll_spray(OPEN, ms(m), RAIN_ROLL_ASPHALT));
}

/* each cross-fade is equal power: the stopped layer falls as its moving one rises, the sum of their squares (each against its own end) stays 1 */
static void test_equal_power_cross_fade(void)
{
    for (int cockpit = 0; cockpit <= 1; cockpit++) {
        int last_stopped = 1000, last_moving = -1;
        for (int32_t v = RAIN_ROLL_MOVING; v <= RAIN_ROLL_CROSSED; v += 0x8000) {
            const RainRoll r = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, v, RAIN_ROLL_ASPHALT, cockpit);
            const int stopped = cockpit ? r.in : r.out, moving = cockpit ? r.move_in : r.bed;
            CHECK(stopped <= last_stopped && moving >= last_moving);
            last_stopped = stopped, last_moving = moving;
            const double a = (double)stopped / (cockpit ? RAIN_ROLL_IN_PERCENT : RAIN_ROLL_OUT_PERCENT), b = (double)moving / (cockpit ? RAIN_ROLL_MOVE_IN_LOW_PERCENT : RAIN_ROLL_BED_PERCENT);
            CHECK(a * a + b * b > 0.9 && a * a + b * b < 1.1);
        }
    }
    int last = 0, last_in = 0;
    for (int m = 15; m <= 70; m++) {   /* past it the spray and the cockpit's layer only rise */
        const int level = rain_roll_spray(OPEN, ms(m), RAIN_ROLL_ASPHALT), in = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_ASPHALT, 1).move_in;
        CHECK(level >= last && level <= RAIN_ROLL_SPRAY_TOP_PERCENT && in >= last_in && in <= RAIN_ROLL_MOVE_IN_TOP_PERCENT);
        last = level, last_in = in;
    }
    CHECK(RAIN_ROLL_SPRAY_TOP_PERCENT <= 600 && RAIN_ROLL_MOVE_IN_TOP_PERCENT <= 600);   /* within what the voice takes (WET_ROLL_MAX_PERCENT) */
}

static void test_silent_and_scaled(void)
{
    for (int cockpit = 0; cockpit <= 1; cockpit++)
        for (int m = 0; m <= 60; m += 10) {
            RainRoll r = rain_roll_levels(RAIN_ROLL_DRY, OPEN, ms(m), RAIN_ROLL_ASPHALT, cockpit);   /* dry (clear, fog) */
            CHECK(r.out == 0 && r.in == 0 && r.bed == 0 && r.move_in == 0 && r.spray == 0);
            r = rain_roll_levels(RAIN_ROLL_RAIN, 0, ms(m), RAIN_ROLL_ASPHALT, cockpit);               /* inside a closed zone */
            CHECK(r.out == 0 && r.in == 0 && r.bed == 0 && r.move_in == 0 && r.spray == 0);
            CHECK_EQ_INT(rain_roll_spray(0, ms(m), RAIN_ROLL_ASPHALT), 0);
        }
    /* off the asphalt (grass, a shoulder, the air) the spray is silent; the rain still falls, on a moving car too */
    const RainRoll off = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(40), RAIN_ROLL_OFF, 0);
    CHECK(off.spray == 0 && off.bed == RAIN_ROLL_BED_PERCENT);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(40), RAIN_ROLL_OFF, 1).move_in, rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(40), RAIN_ROLL_ASPHALT, 1).move_in);
    /* falling snow is silent standing, but melts on the road: the spray as in rain, in the cockpit too, no rain layer */
    for (int cockpit = 0; cockpit <= 1; cockpit++) {
        const RainRoll still = rain_roll_levels(RAIN_ROLL_SNOW, OPEN, 0, RAIN_ROLL_ASPHALT, cockpit), moving = rain_roll_levels(RAIN_ROLL_SNOW, OPEN, ms(40), RAIN_ROLL_ASPHALT, cockpit);
        CHECK(still.out == 0 && still.in == 0 && still.spray == 0);
        CHECK(moving.out == 0 && moving.in == 0 && moving.bed == 0 && moving.move_in == 0 && moving.spray == rain_roll_spray(OPEN, ms(40), RAIN_ROLL_ASPHALT) && moving.spray > 0);
    }
    /* the mouth of a closed zone: the openness scales every layer */
    const RainRoll full = rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(40), RAIN_ROLL_ASPHALT, 0), half = rain_roll_levels(RAIN_ROLL_RAIN, OPEN / 2, ms(40), RAIN_ROLL_ASPHALT, 0);
    CHECK(half.spray == (full.spray + 1) / 2 || half.spray == full.spray / 2);
    CHECK(half.bed == (full.bed + 1) / 2 || half.bed == full.bed / 2);
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN / 2, 0, RAIN_ROLL_ASPHALT, 1).in, RAIN_ROLL_IN_PERCENT / 2);
}

/* on the rally's gravel the spray is a layer under the gravel roll: RAIN_ROLL_GRAVEL_PERCENT of the asphalt's, the rain's layers as on asphalt */
static void test_gravel_is_under_the_roll(void)
{
    for (int m = 10; m <= 60; m += 10) {
        const int asphalt = rain_roll_spray(OPEN, ms(m), RAIN_ROLL_ASPHALT), gravel = rain_roll_spray(OPEN, ms(m), RAIN_ROLL_GRAVEL);
        CHECK(gravel > 0 && gravel < asphalt && abs(gravel - asphalt * RAIN_ROLL_GRAVEL_PERCENT / 100) <= 1);
        CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_SNOW, OPEN, ms(m), RAIN_ROLL_GRAVEL, 0).spray, gravel);   /* snow falling on the rally: the same */
        CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_GRAVEL, 0).bed, rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(m), RAIN_ROLL_ASPHALT, 0).bed);
    }
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, 0, RAIN_ROLL_GRAVEL, 0).out, RAIN_ROLL_OUT_PERCENT);
}

/* the hiss already sounds at a middle speed: its own rate at RAIN_ROLL_PITCH_MID_SPEED (25 m/s), lower below it (0.8 standing), a little higher above (at most 1.1) */
static void test_pitch(void)
{
    CHECK_EQ_INT(rain_roll_pitch(0), 800);
    CHECK_EQ_INT(rain_roll_pitch(ms(25)), 1000);
    CHECK_EQ_INT(rain_roll_pitch(RAIN_ROLL_FULL), 1100);
    CHECK_EQ_INT(rain_roll_pitch(RAIN_ROLL_FULL * 3), 1100);
    CHECK_EQ_INT(rain_roll_pitch(-ms(25)), 1000);   /* reversing as forwards */
    int last = 0;
    for (int m = 0; m <= 80; m++) {   /* rises, steeply to 25 m/s, gently after */
        const int p = rain_roll_pitch(ms(m));
        CHECK(p >= last && p >= 800 && p <= 1100);
        last = p;
    }
    CHECK(rain_roll_pitch(ms(12)) - rain_roll_pitch(ms(2)) > rain_roll_pitch(ms(50)) - rain_roll_pitch(ms(40)));
    CHECK_EQ_INT(rain_roll_levels(RAIN_ROLL_RAIN, OPEN, ms(30), RAIN_ROLL_ASPHALT, 0).pitch, rain_roll_pitch(ms(30)));
}

/* a spray by where it is heard from: the cockpit 3 dB lower; 0 dB up to the near chase camera's distance and -6 dB at the far one's, in dB between; 1/distance beyond, gone by 60 m */
static void test_view_level(void)
{
    CHECK_EQ_INT(rain_roll_view_permille(1, 0), RAIN_ROLL_VIEW_COCKPIT_PERMILLE);
    CHECK(RAIN_ROLL_VIEW_COCKPIT_PERMILLE >= 700 && RAIN_ROLL_VIEW_COCKPIT_PERMILLE <= 710);             /* -3 dB */
    CHECK_EQ_INT(rain_roll_view_permille(0, 0), 1000);                         /* the bumper, nearer than the near camera */
    CHECK_EQ_INT(rain_roll_view_permille(0, RAIN_ROLL_VIEW_NEAR), 1000);
    CHECK_EQ_INT(rain_roll_view_permille(0, RAIN_ROLL_VIEW_FAR), RAIN_ROLL_VIEW_FAR_PERMILLE);
    CHECK(RAIN_ROLL_VIEW_FAR_PERMILLE >= 498 && RAIN_ROLL_VIEW_FAR_PERMILLE <= 503);                 /* -6 dB */
    const int mid = rain_roll_view_permille(0, (RAIN_ROLL_VIEW_NEAR + RAIN_ROLL_VIEW_FAR) / 2);
    CHECK(mid >= 703 && mid <= 713);   /* halfway in distance, halfway in dB: -3 */
    const int twice = rain_roll_view_permille(0, RAIN_ROLL_VIEW_FAR * 2);
    CHECK(abs(twice - RAIN_ROLL_VIEW_FAR_PERMILLE / 2) <= 1);                  /* twice as far: -6 dB more */
    CHECK(abs(rain_roll_view_permille(1, RAIN_ROLL_VIEW_FAR * 2) - twice * RAIN_ROLL_VIEW_COCKPIT_PERMILLE / 1000) <= 1);   /* another car heard from the cockpit */
    CHECK_EQ_INT(rain_roll_view_permille(0, RAIN_ROLL_HEARD), 0);
    CHECK_EQ_INT(rain_roll_view_permille(0, RAIN_ROLL_HEARD * 3), 0);
    CHECK(rain_roll_view_permille(0, RAIN_ROLL_HEARD_FADE) > 0);
    int last = 1001;
    for (int32_t d = 0; d <= RAIN_ROLL_HEARD + 0x10000; d += 0x4000) {
        const int p = rain_roll_view_permille(0, d);
        CHECK(p <= last);
        last = p;
    }
    CHECK(RAIN_ROLL_VIEW_NEAR > 0x40000 && RAIN_ROLL_VIEW_NEAR < 0x48000 && RAIN_ROLL_VIEW_FAR > 0x6a000 && RAIN_ROLL_VIEW_FAR < 0x70000);   /* 4.35 m, 6.8 m: kinds 2 and 3 */
}

/* a change of view glides: no step, most of the way in RAIN_ROLL_VIEW_GLIDE_TICKS and there within a second */
static void test_view_glide(void)
{
    CHECK_EQ_INT(rain_roll_view_glide(708, 708, 5), 708);
    CHECK_EQ_INT(rain_roll_view_glide(1000, 501, 0), 1000);   /* no tick, no move */
    const int one = rain_roll_view_glide(1000, 501, 1);
    CHECK(one < 1000 && one > 960);                             /* one tick: a small step */
    const int tau = rain_roll_view_glide(1000, 501, RAIN_ROLL_VIEW_GLIDE_TICKS);
    CHECK(tau > 501 + (1000 - 501) * 30 / 100 && tau < 501 + (1000 - 501) * 45 / 100);   /* e^-1 of the way left */
    CHECK_EQ_INT(rain_roll_view_glide(1000, 501, 60), 501);    /* a second on: there */
    CHECK_EQ_INT(rain_roll_view_glide(501, 1000, 600), 1000);
    int g = 501;
    for (int i = 0; i < 120; i++) {   /* tick by tick it gets there too, and never overshoots */
        const int next = rain_roll_view_glide(g, 1000, 1);
        CHECK(next >= g && next <= 1000);
        g = next;
    }
    CHECK_EQ_INT(g, 1000);
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
    test_view_level();
    test_view_glide();
    licence_entry("rain_stopped_out.wav");
    licence_entry("rain_stopped_in.wav");
    licence_entry("rain_moving_out.wav");
    licence_entry("rain_moving_in.wav");
    licence_entry("rain_spray.wav");
    for (int n = 1; n <= 7; n++) {
        char name[32];
        snprintf(name, sizeof name, "rain_puddle_%d.wav", n);
        licence_entry(name);
    }
    char path[1024];   /* the wet-tyre roll is retired: the spray replaced it */
    snprintf(path, sizeof path, "%s/data/sounds/rain_tyre_roll.wav", TNFS_SOURCE_DIR);
    FILE *old = fopen(path, "rb");
    CHECK(old == NULL);
    if (old != NULL) fclose(old);
    return TEST_REPORT();
}
