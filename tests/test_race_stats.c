/* The race's stats overlay (race_stats.c): F12's edge toggle, the start's seed from extras.cfg, the frame meter on fake
 * times, the text's words, and the write-back of the switch. The drawing itself is checked by the captures. */
#include "viewer/race_stats.h"
#include "test.h"

#include "game/platform/platform.h"

#include <stdlib.h>
#include <string.h>

/* One in-memory user file stands in for the platform's storage, as in test_port_extras.c */
static char stored_name[32];
static uint8_t stored[512];
static size_t stored_size;
static int stored_present;

static const char *mode = "profile=restomod\n";

uint8_t *platform_read_user_file(const char *name, size_t *size)
{
    if (strcmp(name, "port.cfg") == 0) {   /* the mode that leaves everything free; port_profile's test has the others */
        uint8_t *copy_of_mode = malloc(strlen(mode) + 1);
        memcpy(copy_of_mode, mode, strlen(mode) + 1);
        *size = strlen(mode);
        return copy_of_mode;
    }
    if (!stored_present || strcmp(name, stored_name) != 0)
        return NULL;
    uint8_t *copy = malloc(stored_size);
    memcpy(copy, stored, stored_size);
    *size = stored_size;
    return copy;
}

bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    if (strcmp(name, "port.cfg") == 0) return true;   /* the mode stays */
    if (size > sizeof stored)
        return false;
    snprintf(stored_name, sizeof stored_name, "%s", name);
    memcpy(stored, data, size);
    stored_size = size;
    stored_present = 1;
    return true;
}

void platform_free_file(uint8_t *data) { free(data); }

/* F12 is 0x58 and the edge is on the press, not the hold */
static void test_key_is_f12(void) { CHECK_EQ_INT(RACE_STATS_KEY, 0x58); }

static void test_starts_off_and_toggles_on_the_edge(void)
{
    RaceStats s;
    race_stats_start(&s, 0);
    CHECK_EQ_INT(s.on, 0);
    CHECK_EQ_INT(race_stats_key(&s, 1), 1);    /* press: on */
    CHECK_EQ_INT(s.on, 1);
    CHECK_EQ_INT(race_stats_key(&s, 1), 0);    /* held: no change */
    CHECK_EQ_INT(race_stats_key(&s, 1), 0);
    CHECK_EQ_INT(s.on, 1);
    CHECK_EQ_INT(race_stats_key(&s, 0), 0);    /* release: no change */
    CHECK_EQ_INT(race_stats_key(&s, 1), 1);    /* press again: off */
    CHECK_EQ_INT(s.on, 0);
}

/* extras.cfg's value seeds the race: a race starts with what the file says */
static void test_start_seeds_from_the_saved_value(void)
{
    RaceStats s;
    race_stats_start(&s, 1);
    CHECK_EQ_INT(s.on, 1);
    CHECK_EQ_INT(race_stats_key(&s, 1), 1);
    CHECK_EQ_INT(s.on, 0);
}

/* F12 has no effect in Stock, which holds it */
static void test_stock_holds_the_key(void)
{
    RaceStats s;
    stored_present = 0;
    mode = "profile=stock\n";
    race_stats_start(&s, 0);
    CHECK_EQ_INT(race_stats_key_save(&s, 1), 0);
    CHECK_EQ_INT(s.on, 0);
    CHECK(!stored_present);
    mode = "profile=restored\n";
    race_stats_start(&s, 0);
    CHECK_EQ_INT(race_stats_key_save(&s, 1), 1);
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_STATS), 1);
    stored_present = 0;
    mode = "profile=restomod\n";
}

/* F12 writes the new value to the mode's file, and only on a toggle */
static void test_key_writes_back_the_switch(void)
{
    RaceStats s;
    stored_present = 0;
    race_stats_start(&s, 0);
    CHECK_EQ_INT(race_stats_key_save(&s, 1), 1);
    CHECK_EQ_INT(race_stats_key_save(&s, 1), 0);
    CHECK(stored_present);
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_STATS), 1);
    CHECK_EQ_INT(race_stats_key_save(&s, 0), 0);
    CHECK_EQ_INT(race_stats_key_save(&s, 1), 1);
    CHECK_EQ_INT(port_extras_race_on(RACE_SWITCH_STATS), 0);
}

/* Meter on fake times: a frame every 20 ms, a step every frame (50 steps a second) */
static void test_meter_averages_the_window(void)
{
    RaceStatsMeter m;
    memset(&m, 0, sizeof m);
    for (uint32_t t = 1000; t <= 1500; t += 20) race_stats_meter_frame(&m, t);
    CHECK(m.figures.ready);
    CHECK(m.figures.fps > 49.9 && m.figures.fps < 50.1);
    CHECK(m.figures.frame_avg_ms > 19.9 && m.figures.frame_avg_ms < 20.1);
    CHECK_EQ_INT(m.figures.frame_max_ms, 20);
}

/* Nothing is shown before a window has run; a spike sets the max of the window it falls in */
static void test_meter_waits_for_a_window_and_keeps_the_spike(void)
{
    RaceStatsMeter m;
    memset(&m, 0, sizeof m);
    for (uint32_t t = 1000; t <= 1400; t += 20) race_stats_meter_frame(&m, t);
    CHECK(!m.figures.ready);
    race_stats_meter_frame(&m, 1420);
    race_stats_meter_frame(&m, 1460);   /* a 40 ms frame */
    race_stats_meter_frame(&m, 1480);
    race_stats_meter_frame(&m, 1520);   /* window ends at 1520: 500 ms after 1020 */
    CHECK(m.figures.ready);
    CHECK_EQ_INT(m.figures.frame_max_ms, 40);
}

/* The words: the rendering figures as the overlay writes them, the ratio of the render, the switches as drawn; no
 * simulation figures (the steps, the clock) */
static void test_text_says_the_figures(void)
{
    RaceStatsView v;
    RaceStatsText t;
    memset(&v, 0, sizeof v);
    v.figures.ready = 1;
    v.figures.fps = 59.8;
    v.figures.frame_avg_ms = 16.7;
    v.figures.frame_max_ms = 18;
    v.refresh_hz = 60;
    v.interp = 1;
    v.width = 640;
    v.height = 480;
    v.modern = 1;
    v.filter = 1;
    v.fog = 0;
    v.segments = 28;
    race_stats_format(&v, &t);
    CHECK(strcmp(t.line[0], "FPS 59.8  frame 16.7 ms  max 18 ms") == 0);
    CHECK(strcmp(t.line[1], "Refresh 60 Hz  interp on") == 0);
    CHECK(strcmp(t.line[2], "Render 640x480  4:3") == 0);
    CHECK(strcmp(t.line[3], "Perspective correction on  filter on  fog off") == 0);
    CHECK(strcmp(t.line[4], "View 28 segments") == 0);

    v.figures.ready = 0;
    v.interp = 0;
    v.width = 1120;
    v.modern = 0;
    v.filter = 0;
    v.fog = 0;
    v.segments = 56;
    race_stats_format(&v, &t);
    CHECK(strcmp(t.line[0], "FPS --  frame -- ms") == 0);
    CHECK(strcmp(t.line[1], "Refresh 60 Hz  interp off") == 0);
    CHECK(strcmp(t.line[2], "Render 1120x480  21:9") == 0);
    CHECK(strcmp(t.line[3], "Perspective correction off  filter off  fog off") == 0);
    CHECK(strcmp(t.line[4], "View 56 segments") == 0);
}

int main(void)
{
    test_key_is_f12();
    test_starts_off_and_toggles_on_the_edge();
    test_start_seeds_from_the_saved_value();
    test_stock_holds_the_key();
    test_key_writes_back_the_switch();
    test_meter_averages_the_window();
    test_meter_waits_for_a_window_and_keeps_the_spike();
    test_text_says_the_figures();
    return TEST_REPORT();
}
