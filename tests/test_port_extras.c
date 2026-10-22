/* The port's extras file (extras.cfg): defaults, text round trip, what a damaged file leaves alone, and the
 * cheat marks a race is filed under. */
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* One in-memory user file stands in for the platform's storage. */
static char stored_name[32];
static uint8_t stored[256];
static size_t stored_size;
static int stored_present;

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    if (!stored_present || strcmp(name, stored_name) != 0)
        return NULL;
    uint8_t *copy = malloc(stored_size);
    memcpy(copy, stored, stored_size);
    *size = stored_size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (size > sizeof stored)
        return false;
    snprintf(stored_name, sizeof stored_name, "%s", name);
    memcpy(stored, data, size);
    stored_size = size;
    stored_present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

static void check_same(PortExtras a, PortExtras b)
{
    CHECK_EQ_INT(a.mirror, b.mirror);
    CHECK_EQ_INT(a.rally, b.rally);
    CHECK_EQ_INT(a.warrior, b.warrior);
    CHECK_EQ_INT(a.vegas, b.vegas);
    CHECK_EQ_INT(a.explode, b.explode);
    CHECK_EQ_INT(a.nitro, b.nitro);
    CHECK_EQ_INT(a.turbo, b.turbo);
    CHECK_EQ_INT(a.replay_cameras, b.replay_cameras);
    CHECK_EQ_INT(a.warrior_rival, b.warrior_rival);
    CHECK_EQ_INT(a.no_movies, b.no_movies);
    CHECK_EQ_INT(a.no_assists, b.no_assists);
    CHECK_EQ_INT(a.modern_3d, b.modern_3d);
    CHECK_EQ_INT(a.texture_filter, b.texture_filter);
    CHECK_EQ_INT(a.fog, b.fog);
    CHECK_EQ_INT(a.bumper_mirror, b.bumper_mirror);
    CHECK_EQ_INT(a.bumper_radar, b.bumper_radar);
    CHECK_EQ_INT(a.variant_time, b.variant_time);
    CHECK_EQ_INT(a.start, b.start);
}

static void test_defaults(void)
{
    const PortExtras d = port_extras_defaults();
    check_same(d, (PortExtras){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, START_GAME_MODE });
    CHECK_EQ_INT(d.start, START_GAME_MODE);
}

static void test_round_trip(void)
{
    const PortExtras e = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, START_CENTRAL };
    char text[256];
    const size_t n = port_extras_format(&e, text, sizeof text);
    CHECK(n > 0 && n < sizeof text);
    check_same(port_extras_parse(text, n), e);
    CHECK_EQ_INT(port_extras_parse(text, n).start, 2);   /* START_CENTRAL is 2 */
}

static void test_parse_keys(void)
{
    const char *text = "mirror=1\nturbo=1\nmodern_3d=1\ntexture_filter=1\nfog=1\nbumper_radar=1\nstart=network\n";
    const PortExtras e = port_extras_parse(text, strlen(text));
    CHECK_EQ_INT(e.texture_filter, 1);
    CHECK_EQ_INT(e.fog, 1);
    CHECK_EQ_INT(e.bumper_radar, 1);
    CHECK_EQ_INT(e.bumper_mirror, 0);
    CHECK_EQ_INT(e.variant_time, 0);
    CHECK_EQ_INT(port_extras_parse("variant_time=1\n", 15).variant_time, 1);
    CHECK_EQ_INT(e.mirror, 1);
    CHECK_EQ_INT(e.turbo, 1);
    CHECK_EQ_INT(e.modern_3d, 1);
    CHECK_EQ_INT(e.rally, 0);          /* untouched keys keep the default */
    CHECK_EQ_INT(e.start, START_NETWORK);
}

static void test_bad_input_keeps_defaults(void)
{
    const char *text = "start=7\nno equals sign\nwarp=9\nmirror=2\n";
    check_same(port_extras_parse(text, strlen(text)), port_extras_defaults());
    check_same(port_extras_parse("", 0), port_extras_defaults());
}

static void test_cheat_marks(void)
{
    PortExtras e = port_extras_defaults();
    CHECK_EQ_INT(port_extras_cheat_marks(&e), 0);
    e.nitro = 1;
    CHECK_EQ_INT(port_extras_cheat_marks(&e), EXTRAS_MARK_NITRO);
    e.turbo = 1;
    CHECK_EQ_INT(port_extras_cheat_marks(&e), EXTRAS_MARK_NITRO | EXTRAS_MARK_TURBO);
    CHECK_EQ_INT(port_extras_cheat_marks(&e), 5);
    e.explode = 1;
    CHECK_EQ_INT(port_extras_cheat_marks(&e), 7);
    e.nitro = e.turbo = 0;             /* the switches without an announced effect are not marks */
    CHECK_EQ_INT(port_extras_cheat_marks(&e), EXTRAS_MARK_EXPLODE);
}

static void test_load_save(void)
{
    check_same(port_extras_load(), port_extras_defaults());   /* no file yet */
    const PortExtras e = { 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, START_NETWORK };
    CHECK(port_extras_save(&e));
    CHECK(strcmp(stored_name, "extras.cfg") == 0);
    check_same(port_extras_load(), e);
}

int main(void)
{
    test_defaults();
    test_round_trip();
    test_parse_keys();
    test_bad_input_keeps_defaults();
    test_cheat_marks();
    test_load_save();
    return TEST_REPORT();
}