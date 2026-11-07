/* the wet-tyre roll's level and pitch (rain_roll.h) and the sample's licence entry */
#include "tests/test.h"

#include "game/race/rain_roll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OPEN = 16 };

static void test_volume_curve(void)
{
    const int32_t kmh = 0x10000 * 1000 / 3600;   /* 1 km/h in 16.16 m/s, close enough for a curve */
    CHECK_EQ_INT(rain_roll_volume(1, OPEN, 0, 1), 0);                 /* stopped */
    CHECK_EQ_INT(rain_roll_volume(1, OPEN, RAIN_ROLL_MOVING, 1), 0);  /* creeping */
    CHECK_EQ_INT(rain_roll_volume(0, OPEN, 40 * 0x10000, 1), 0);      /* clear weather */
    CHECK_EQ_INT(rain_roll_volume(1, 0, 40 * 0x10000, 1), 0);         /* inside a closed zone */
    CHECK_EQ_INT(rain_roll_volume(1, OPEN, 40 * 0x10000, 0), 0);      /* gravel, grass, the air */
    int last = 0;
    for (int v = 10; v <= 250; v += 10) {   /* rises with speed, never past the cap */
        const int level = rain_roll_volume(1, OPEN, v * kmh, 1);
        CHECK(level >= last && level <= RAIN_ROLL_MAX_PERCENT);
        last = level;
    }
    CHECK(rain_roll_volume(1, OPEN, 100 * kmh, 1) > 0 && rain_roll_volume(1, OPEN, 100 * kmh, 1) < rain_roll_volume(1, OPEN, 200 * kmh, 1));
    CHECK_EQ_INT(rain_roll_volume(1, OPEN, RAIN_ROLL_FULL, 1), RAIN_ROLL_MAX_PERCENT);
    CHECK_EQ_INT(rain_roll_volume(1, OPEN, RAIN_ROLL_FULL * 2, 1), RAIN_ROLL_MAX_PERCENT);   /* the top holds */
    CHECK_EQ_INT(rain_roll_volume(1, OPEN, -RAIN_ROLL_FULL, 1), RAIN_ROLL_MAX_PERCENT);      /* reversing is as loud */
    CHECK(RAIN_ROLL_MAX_PERCENT <= 50);                                                      /* low: under the engine */
    /* the mouth of a closed zone: the openness fades it, 0 is silent, in between is between */
    const int full = rain_roll_volume(1, OPEN, 40 * 0x10000, 1), half = rain_roll_volume(1, OPEN / 2, 40 * 0x10000, 1), mouth = rain_roll_volume(1, 1, 40 * 0x10000, 1);
    CHECK(full > half && half > mouth && mouth >= 0 && half == full / 2);
}

static void test_pitch(void)
{
    CHECK_EQ_INT(rain_roll_pitch(0), RAIN_ROLL_PITCH_SLOW);
    CHECK_EQ_INT(rain_roll_pitch(RAIN_ROLL_FULL), RAIN_ROLL_PITCH_FAST);
    CHECK_EQ_INT(rain_roll_pitch(RAIN_ROLL_FULL * 3), RAIN_ROLL_PITCH_FAST);
    CHECK(rain_roll_pitch(20 * 0x10000) > rain_roll_pitch(10 * 0x10000) && rain_roll_pitch(-30 * 0x10000) == rain_roll_pitch(30 * 0x10000));
}

/* the sample and its licence entry: Only CC0 or public domain goes in data/sounds, with what, how it was made, the source, the author, the licence and the check date */
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
    const char *entry = strstr(text, "## rain_tyre_roll.wav");
    CHECK(entry != NULL);
    if (entry != NULL) {
        const char *next = strstr(entry + 3, "\n## ");
        char *part = strndup(entry, next != NULL ? (size_t)(next - entry) : strlen(entry));
        CHECK(strstr(part, "- What:") && strstr(part, "- Made from:") && strstr(part, "- Source: https://") && strstr(part, "- Author:") && strstr(part, "- Licence:") && strstr(part, "Checked on the page on 20"));
        CHECK(strstr(part, "Creative Commons 0") != NULL && strstr(part, "creativecommons.org/publicdomain/zero/1.0") != NULL);
        free(part);
    }
    free(text);
    snprintf(path, sizeof path, "%s/data/sounds/rain_tyre_roll.wav", TNFS_SOURCE_DIR);   /* and the file is a 16-bit mono WAV at 22050 Hz */
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
    test_pitch();
    test_licence_entry();
    return TEST_REPORT();
}
