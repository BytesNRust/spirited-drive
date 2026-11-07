/* port (snow crunch): the snowy rally's roll (snow_roll.h): its level follows the gravel roll's curve and only the snow's rally makes it; the sample and its licence entry. */
#include "game/race/snow_roll.h"

#include "game/race/rally_gravel.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t metres(int m) { return m * 0x10000; }

static void test_volume_curve(void)
{
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, 0, 1), 0);                   /* standing */
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, metres(1), 1), 0);           /* under RALLY_TYRE_MOVING */
    CHECK(snow_roll_volume(RALLY_GRAVEL_SNOW, metres(RALLY_TYRE_MOVING), 1) > 0);
    int last = 0;
    for (int m = RALLY_TYRE_MOVING; m <= 40; m++) {   /* rises with speed, never falls */
        const int v = snow_roll_volume(RALLY_GRAVEL_SNOW, metres(m), 1);
        CHECK(v >= last);
        last = v;
    }
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, metres(RALLY_TYRE_TOP * 2 / RALLY_TYRE_PER_METRE_X2), 1), SNOW_ROLL_MAX_PERCENT);   /* the top of the gravel roll's curve */
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, metres(100), 1), SNOW_ROLL_MAX_PERCENT);   /* and no louder beyond it */
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, -metres(20), 1), snow_roll_volume(RALLY_GRAVEL_SNOW, metres(20), 1));   /* reversing counts as speed */
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, metres(20), 0), 0);   /* in the air: nothing */
    /* the others' rallies and the asphalt do not make it */
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_OFF, metres(20), 1), 0);
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_COMPACT, metres(20), 1), 0);
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_DUST, metres(20), 1), 0);
    CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_REGOLITH, metres(20), 1), 0);
}

/* the same curve as the gravel roll's own floor: the level is in proportion to rally_gravel_tyre_skid's on the snow's rally */
static void test_follows_the_gravel_roll(void)
{
    for (int m = 0; m <= 40; m++) {
        const int32_t floor = rally_gravel_tyre_skid(RALLY_GRAVEL_SNOW, metres(m), 0, 1);
        CHECK_EQ_INT(snow_roll_volume(RALLY_GRAVEL_SNOW, metres(m), 1), SNOW_ROLL_MAX_PERCENT * floor / RALLY_TYRE_TOP);
    }
}

static void test_pitch(void)
{
    CHECK_EQ_INT(snow_roll_pitch(0), SNOW_ROLL_PITCH_SLOW);
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
    const char *entry = strstr(text, "## snow_crunch.wav");
    CHECK(entry != NULL);
    if (entry != NULL) {
        const char *next = strstr(entry + 3, "\n## ");
        char *part = strndup(entry, next != NULL ? (size_t)(next - entry) : strlen(entry));
        CHECK(strstr(part, "- What:") && strstr(part, "- Made from:") && strstr(part, "- Source: https://") && strstr(part, "- Author:") && strstr(part, "- Licence:") && strstr(part, "Checked on the page on 20"));
        CHECK(strstr(part, "Creative Commons 0") != NULL && strstr(part, "creativecommons.org/publicdomain/zero/1.0") != NULL);
        free(part);
    }
    free(text);
    snprintf(path, sizeof path, "%s/data/sounds/snow_crunch.wav", TNFS_SOURCE_DIR);   /* and the file is a 16-bit mono WAV at 22050 Hz */
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
    test_follows_the_gravel_roll();
    test_pitch();
    test_licence_entry();
    return TEST_REPORT();
}
