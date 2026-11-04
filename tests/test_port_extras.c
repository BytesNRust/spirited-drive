/* The port's extras file (extras.cfg): defaults, text round trip, what a damaged file leaves alone, and the
 * cheat marks a race is filed under. */
#include "game/sys/port_extras.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* One in-memory user file stands in for the platform's storage. */
static char stored_name[32];
static uint8_t stored[512];   /* as port_extras_save's text (512) */
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
    CHECK_EQ_INT(a.follow_camera, b.follow_camera);
    CHECK_EQ_INT(a.stats_overlay, b.stats_overlay);
    CHECK_EQ_INT(a.car_markers, b.car_markers);
    CHECK_EQ_INT(a.start, b.start);
    CHECK_EQ_INT(a.units, b.units);
}

static void test_defaults(void)
{
    const PortExtras d = port_extras_defaults();
    /* port: designated, so the fields added later (units, lunar_springs) keep their place in the struct and the
     * values here stay the ones the defaults have: every switch 0 but car_markers 1, the exe's. */
    check_same(d, (PortExtras){ .car_markers = 1, .start = START_GAME_MODE });
    CHECK_EQ_INT(d.start, START_GAME_MODE);
    CHECK_EQ_INT(d.stats_overlay, 0);   /* the stats overlay starts off */
    CHECK_EQ_INT(d.follow_camera, 0);   /* the exe's cameras to start with */
    CHECK_EQ_INT(port_extras_parse("follow_camera=1\n", 16).follow_camera, 1);
    CHECK_EQ_INT(d.car_markers, 1);     /* the car markers start On: the exe's */
    CHECK_EQ_INT(d.movies_wide, 1);     /* the movies fill a wide screen */
}

/* difficulty: 0 Easy, 1 Original (the default), 2 Hard; written and read back; another value keeps the default */
static void test_difficulty_key(void)
{
    CHECK_EQ_INT(port_extras_defaults().difficulty, 1);
    const char *text = "difficulty=2\n";
    PortExtras e = port_extras_parse(text, strlen(text));
    CHECK_EQ_INT(e.difficulty, 2);
    char out[512];
    CHECK(port_extras_format(&e, out, sizeof out) > 0);
    CHECK(strstr(out, "difficulty=2\n") != NULL);
    CHECK_EQ_INT(port_extras_parse(out, strlen(out)).difficulty, 2);
    text = "difficulty=0\n";
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).difficulty, 0);
    text = "difficulty=3\n";
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).difficulty, 1);
    text = "difficulty=-1\n";
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).difficulty, 1);
}

/* movies_wide is a switch of its own: parsed, written and read back */
static void test_movies_wide(void)
{
    const char *text = "movies_wide=0\n";
    PortExtras e = port_extras_parse(text, strlen(text));
    CHECK_EQ_INT(e.movies_wide, 0);
    char out[512];
    CHECK(port_extras_format(&e, out, sizeof out) > 0);
    CHECK(strstr(out, "movies_wide=0\n") != NULL);
    e = port_extras_parse(out, strlen(out));
    CHECK_EQ_INT(e.movies_wide, 0);
    text = "movies_wide=2\n";   /* not a switch value: the default stays */
    CHECK_EQ_INT(port_extras_parse(text, strlen(text)).movies_wide, 1);
}

/* the two race switches are read and written by their keys; a value other than 0 or 1 keeps the default */
static void test_race_switch_keys_parse(void)
{
    const char *text = "stats_overlay=1\ncar_markers=0\n";
    const PortExtras e = port_extras_parse(text, strlen(text));
    CHECK_EQ_INT(e.stats_overlay, 1);
    CHECK_EQ_INT(e.car_markers, 0);
    const char *bad = "car_markers=2\n";
    CHECK_EQ_INT(port_extras_parse(bad, strlen(bad)).car_markers, 1);
}

/* F12 and F6 keep the switches in extras.cfg: the other switches are kept, the default stands until a race sets it */
static void test_race_switches_are_saved(void)
{
    stored_present = 0;
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_STATS), 0);
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_MARKERS), 1);
    PortExtras e = port_extras_defaults();
    e.nitro = 1;
    CHECK(port_extras_save(&e));
    CHECK(port_extras_set_race_switch(RACE_SWITCH_STATS, 1));
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_STATS), 1);
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_MARKERS), 1);
    CHECK(port_extras_set_race_switch(RACE_SWITCH_MARKERS, 0));
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_MARKERS), 0);
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_STATS), 1);
    CHECK_EQ_INT(port_extras_load().nitro, 1);   /* the rest of the file is as it was */
}

static void test_units_default_to_mph(void)
{
    CHECK_EQ_INT(port_extras_defaults().units, UNITS_MPH);
    CHECK_EQ_INT(port_extras_parse("", 0).units, UNITS_MPH);
}

static void test_units_parse(void)
{
    const char *kmh = "units=kmh\n", *mph = "units=mph\n", *lang = "units=language\n", *bad = "units=knots\n";
    CHECK_EQ_INT(port_extras_parse(kmh, strlen(kmh)).units, UNITS_KMH);
    CHECK_EQ_INT(port_extras_parse(mph, strlen(mph)).units, UNITS_MPH);
    CHECK_EQ_INT(port_extras_parse(lang, strlen(lang)).units, UNITS_MPH);   /* an older file's `language` reads as mph, German or not */
    CHECK_EQ_INT(port_extras_parse(bad, strlen(bad)).units, UNITS_MPH);   /* an unknown name keeps the default */
    const char *lang_after_kmh = "units=kmh\nunits=language\n";
    CHECK_EQ_INT(port_extras_parse(lang_after_kmh, strlen(lang_after_kmh)).units, UNITS_MPH);
}

static void test_units_round_trip(void)
{
    const SpeedUnits all[] = { UNITS_MPH, UNITS_KMH };
    for (int i = 0; i < 2; i++) {
        PortExtras e = port_extras_defaults();
        e.units = all[i];
        char text[512];
        const size_t n = port_extras_format(&e, text, sizeof text);
        CHECK(n > 0);
        CHECK_EQ_INT(port_extras_parse(text, n).units, all[i]);
    }
}

static void test_round_trip(void)
{
    const PortExtras e = { .mirror = 1, .rally = 1, .warrior = 1, .vegas = 1, .explode = 1, .nitro = 1, .turbo = 1,
                            .replay_cameras = 1, .warrior_rival = 1, .no_movies = 1, .no_assists = 1, .modern_3d = 1,
                            .texture_filter = 1, .fog = 1, .bumper_mirror = 1, .bumper_radar = 1, .variant_time = 1,
                            .stats_overlay = 1, .car_markers = 1, .follow_camera = 1, .start = START_CENTRAL };
    char text[512];
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
    const PortExtras e = { .mirror = 1, .explode = 1, .nitro = 1, .turbo = 1, .replay_cameras = 1, .no_assists = 1,
                            .modern_3d = 1, .fog = 1, .bumper_mirror = 1, .stats_overlay = 1, .start = START_NETWORK };
    CHECK(port_extras_save(&e));
    CHECK(strcmp(stored_name, "extras.cfg") == 0);
    check_same(port_extras_load(), e);
}

/* the fullest file (every switch on, the longest words, every paint) still saves, and reads back the same: the save buffer has room to grow */
static void test_fullest_file_saves(void)
{
    PortExtras e = port_extras_defaults();
    e.start = START_NETWORK, e.units = UNITS_KMH, e.dash_box = DASH_BOX_HIDDEN, e.difficulty = 2;
    int *const flags[] = { &e.mirror, &e.rally, &e.warrior, &e.vegas, &e.explode, &e.nitro, &e.turbo, &e.replay_cameras, &e.warrior_rival,
                           &e.no_movies, &e.no_assists, &e.modern_3d, &e.texture_filter, &e.fog, &e.bumper_mirror, &e.bumper_radar,
                           &e.variant_time, &e.stats_overlay, &e.car_markers, &e.lunar_springs, &e.movies_wide, &e.follow_camera,
                           &e.psx_liveries, &e.reverse, &e.lunar_gravity };
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++) *flags[i] = 1;
    for (int c = 0; c < CAR_PAINT_SLOTS; c++) e.car_paint[c] = 15;
    char out[1024];
    const size_t n = port_extras_format(&e, out, sizeof out);
    CHECK(n > 0 && n < 512);   /* it fits the old 512-byte buffer today: the test warns when a key more would not */
    CHECK(port_extras_save(&e));
    check_same(port_extras_load(), port_extras_parse(out, n));
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* "Check for updates" (update_check): On by default, an Off sticks and is written */
static void test_update_check(void)
{
    CHECK_EQ_INT(port_extras_defaults().update_check, 1);
    CHECK_EQ_INT(port_extras_parse("update_check=0\n", 15).update_check, 0);
    CHECK_EQ_INT(port_extras_parse("update_check=1\n", 15).update_check, 1);
    CHECK_EQ_INT(port_extras_parse("update_check=2\n", 15).update_check, 1);   /* not a switch value: left as it was */
    CHECK_EQ_INT(port_extras_parse("", 0).update_check, 1);                     /* an old file with no key: On */
    PortExtras e = port_extras_defaults();
    e.update_check = 0;
    char out[1024];
    const size_t n = port_extras_format(&e, out, sizeof out);
    CHECK(n > 0 && strstr(out, "update_check=0") != NULL);
    CHECK_EQ_INT(port_extras_parse(out, n).update_check, 0);
}

/* "Rain with storm" (rain_lightning): off by default and in an old file, an On is written and read back */
static void test_rain_lightning(void)
{
    CHECK_EQ_INT(port_extras_defaults().rain_lightning, 0);
    CHECK_EQ_INT(port_extras_parse("", 0).rain_lightning, 0);
    CHECK_EQ_INT(port_extras_parse("rain_lightning=1\n", 17).rain_lightning, 1);
    CHECK_EQ_INT(port_extras_parse("rain_lightning=2\n", 17).rain_lightning, 0);
    PortExtras e = port_extras_defaults();
    e.rain_lightning = 1;
    char out[1024];
    const size_t n = port_extras_format(&e, out, sizeof out);
    CHECK(n > 0 && strstr(out, "rain_lightning=1") != NULL);
    CHECK_EQ_INT(port_extras_parse(out, n).rain_lightning, 1);
}

int main(void)
{
    test_rain_lightning();
    test_update_check();
    test_defaults();
    test_movies_wide();
    test_difficulty_key();
    test_units_default_to_mph();
    test_units_parse();
    test_units_round_trip();
    test_round_trip();
    test_parse_keys();
    test_bad_input_keeps_defaults();
    test_cheat_marks();
    test_load_save();
    test_fullest_file_saves();
    test_race_switch_keys_parse();
    test_race_switches_are_saved();
    return TEST_REPORT();
}
