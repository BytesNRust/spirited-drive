/* Lunar Springs (lunar_springs.h): the extras switch's default and file round trip, the FAM the rally on Rusty
 * Springs takes with the switch on (and only then), and the Control Central picture it takes in its place. */
#include "game/race/lunar_springs.h"
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* port_extras.c reads and writes the user file through the platform; these tests never touch the disk */
uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    (void)name, (void)size;
    return NULL;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}

void platform_free_file(uint8_t *data)
{
    free(data);
}

static void test_switch_is_off_by_default(void)
{
    const PortExtras d = port_extras_defaults();
    CHECK_EQ_INT(d.lunar_springs, 0);
}

static void test_switch_round_trips_through_extras_cfg(void)
{
    char text[512];
    PortExtras e = port_extras_defaults();
    e.lunar_springs = 1;
    const size_t n = port_extras_format(&e, text, sizeof text);
    CHECK(n > 0);
    CHECK(strstr(text, "lunar_springs=1\n") != NULL);
    const PortExtras back = port_extras_parse(text, n);
    CHECK_EQ_INT(back.lunar_springs, 1);
    CHECK_EQ_INT(back.start, START_GAME_MODE);   /* the other fields are not disturbed by the new one */
    CHECK_EQ_INT(back.vegas, 0);
}

static void test_switch_reads_only_zero_or_one(void)
{
    const char *const bad = "lunar_springs=2\n";
    const PortExtras e = port_extras_parse(bad, strlen(bad));
    CHECK_EQ_INT(e.lunar_springs, 0);
}

static void test_rusty_rally_takes_the_moons_fam_when_on(void)
{
    char out[256] = "";
    CHECK_EQ_INT(lunar_springs_fam("/data", LUNAR_TRACK_RUSTY, 1, 1, out, sizeof out), 1);
    CHECK(strcmp(out, "/data/SIMDATA/NTRACKFM/TR8_R01.FAM") == 0);
}

static void test_no_remap_when_off_or_not_rusty_rally(void)
{
    char out[256] = "untouched";
    CHECK_EQ_INT(lunar_springs_fam("/data", LUNAR_TRACK_RUSTY, 1, 0, out, sizeof out), 0);   /* switch off */
    CHECK_EQ_INT(lunar_springs_fam("/data", LUNAR_TRACK_RUSTY, 0, 1, out, sizeof out), 0);   /* mirror or plain */
    CHECK_EQ_INT(lunar_springs_fam("/data", 0, 1, 1, out, sizeof out), 0);                   /* another track */
    CHECK_EQ_INT(lunar_springs_fam("/data", 8, 1, 1, out, sizeof out), 0);                   /* Lost Vegas */
    CHECK(strcmp(out, "untouched") == 0);
}

static void test_control_central_shows_the_moon_for_rusty_rally_when_on(void)
{
    char out[256] = "";
    CHECK_EQ_INT(lunar_springs_picture(LUNAR_ART_SPRINGSR, 1, "/data", out, sizeof out), 1);
    CHECK(strcmp(out, "/data/frontend/gart/control/moon.qfs") == 0);
}

static void test_control_central_keeps_its_picture_when_off_or_elsewhere(void)
{
    char out[256] = "untouched";
    CHECK_EQ_INT(lunar_springs_picture(LUNAR_ART_SPRINGSR, 0, "/data", out, sizeof out), 0);
    CHECK_EQ_INT(lunar_springs_picture(19, 1, "/data", out, sizeof out), 0);   /* cityr, the rally's first */
    CHECK_EQ_INT(lunar_springs_picture(3, 1, "/data", out, sizeof out), 0);    /* springs, the plain art */
    CHECK(strcmp(out, "untouched") == 0);
}

int main(void)
{
    test_switch_is_off_by_default();
    test_switch_round_trips_through_extras_cfg();
    test_switch_reads_only_zero_or_one();
    test_rusty_rally_takes_the_moons_fam_when_on();
    test_no_remap_when_off_or_not_rusty_rally();
    test_control_central_shows_the_moon_for_rusty_rally_when_on();
    test_control_central_keeps_its_picture_when_off_or_elsewhere();
    return TEST_REPORT();
}
