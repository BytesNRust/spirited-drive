/* The track light file, one a track (track_light_track.h): the facts, the time's block, the strips, the tunnel look and its lamps, the rows overrides, the sun, the bad lines. Made-up numbers, no game data. */
#include "game/race/track_light_track.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 2000 };

static const char FILE_TEXT[] =
    "# tnfs track light 2\n"
    "track CY1\n"
    "tunnel 1064 1171\n"
    "tunnel 1 62 lit\n"
    "snow 1\n"
    "snow_map 3 4\n"
    "\n"
    "time A\n"
    "  sun 150 20 55\n"
    "  tunnel_look 104 ff5a0d\n"
    "  tunnel_look 1064 1171 90 lamps 220 reach 2\n"
    "time N\n"
    "  sun - 10 -\n"
    "  tunnel_look 115\n"
    "  lamp 130 150 ff2000:80 8\n"
    "  brightness 852 943 60 left\n"
    "  brightness 10 20 50 p2 p4\n"
    "  tone 30 40 road ffe080:50\n"
    "  rows 5 6 1 2 3 4 5 6 7 8 9 10 11\n"
    "time B\n";

static int parse(const char *text, char letter, TrackLightFile *out, int *bad)
{
    memset(out, 0, sizeof *out);
    return track_light_track_parse(text, strlen(text), "CY1", letter, NODES, out, bad);
}

static void test_the_facts_and_a_time(void)
{
    TrackLightFile f;
    int bad = -1;
    CHECK(parse(FILE_TEXT, 'A', &f, &bad));
    CHECK_EQ_INT(bad, 0);
    CHECK(f.light.tunnel != NULL && f.light.tunnel[1064] == 1 && f.light.tunnel[1171] == 1 && f.light.tunnel[1063] == 0 && f.light.tunnel[1172] == 0);
    CHECK(f.light.tunnel[1] == 0);   /* a hall is not a tunnel flag: it is a lit zone of every time */
    CHECK_EQ_INT(f.light.snow, 1);
    CHECK_EQ_INT(f.light.snow_map[3], 5);
    CHECK(f.light.from_places);
    CHECK(f.sun.has && f.sun.has_azimuth && f.sun.has_elevation && f.sun.has_shadow);
    CHECK(f.sun.azimuth == 150.0 && f.sun.elevation == 20.0 && f.sun.shadow == 55.0);
    CHECK(f.tweaks.look_has_light && f.tweaks.look_light == 104.0 && f.tweaks.look_has_colour);
    int lit = 0, own = 0;
    for (int i = 0; i < f.tweaks.zone_count; i++) {
        lit += f.tweaks.zones[i].lit && f.tweaks.zones[i].from == 1 && f.tweaks.zones[i].to == 62;
        own += !f.tweaks.zones[i].lit && f.tweaks.zones[i].from == 1064 && f.tweaks.zones[i].has_light && f.tweaks.zones[i].light == 90.0;
    }
    CHECK_EQ_INT(lit, 1);
    CHECK_EQ_INT(own, 1);
    double strength, reach;
    float cast[3];
    CHECK_EQ_INT(track_light_tunnel_lamp_at(&f.tweaks, 1100, &strength, cast, &reach), TRACK_LIGHT_TUNNEL_LAMP_STRENGTH | TRACK_LIGHT_TUNNEL_LAMP_REACH);
    CHECK(strength == 220.0 && reach == 2.0);
    CHECK_EQ_INT(f.tweaks.lamp_count, 0);   /* the night's lamp is not here */
    CHECK_EQ_INT(f.tweaks.brightness_count, 0);
    track_light_file_free(&f);
}

static void test_the_night_block(void)
{
    TrackLightFile f;
    CHECK(parse(FILE_TEXT, 'N', &f, NULL));
    CHECK(f.sun.has && !f.sun.has_azimuth && f.sun.has_elevation && f.sun.elevation == 10.0 && !f.sun.has_shadow);
    CHECK(f.tweaks.look_has_light && f.tweaks.look_light == 115.0 && !f.tweaks.look_has_colour);
    CHECK_EQ_INT(f.tweaks.lamp_count, 1);
    CHECK_EQ_INT(f.tweaks.brightness_count, 2);
    CHECK_EQ_INT(f.tweaks.brightness[0].mask, 0x7c0);   /* left: p6..p10 */
    CHECK_EQ_INT(f.tweaks.brightness[1].mask, 0x01c);   /* p2 p4: the points 2..4 */
    CHECK_EQ_INT(f.tweaks.tone_run_count, 1);
    CHECK_EQ_INT(f.tweaks.tone_runs[0].mask, (1 << 0) | (1 << 1) | (1 << 6));   /* road */
    CHECK(f.row_set[5] && f.row_set[6] && !f.row_set[4] && !f.row_set[7]);
    CHECK_EQ_INT(f.light.data[5 * TRACK_NODE_LIGHT_POINTS + 10], 11);
    track_light_file_free(&f);
}

static void test_a_brightness_strip_scales_its_points(void)
{
    TrackLightFile f;
    CHECK(parse(FILE_TEXT, 'N', &f, NULL));
    TrackNodeLight light = { 0 };
    light.nodes = NODES;
    light.data = malloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    memset(light.data, 200, (size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    track_light_apply(&f.tweaks, &light, 0, 0);
    for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) CHECK_EQ_INT(light.data[15 * TRACK_NODE_LIGHT_POINTS + p], p >= 2 && p <= 4 ? 100 : 200);
    CHECK_EQ_INT(light.data[900 * TRACK_NODE_LIGHT_POINTS + 0], 200);   /* left is p6..p10: not the centre */
    CHECK_EQ_INT(light.data[900 * TRACK_NODE_LIGHT_POINTS + 7], 120);
    free(light.data);
    track_light_file_free(&f);
}

static void test_a_time_without_a_block_and_another_track(void)
{    {   /* no tunnel fact: no tunnel array (the rows decide) */
        TrackLightFile n;
        CHECK(parse("track CY1\ntunnel 5 9 lit\ntime A\n", 'A', &n, NULL));
        CHECK(n.light.tunnel == NULL);
        track_light_file_free(&n);
    }
    TrackLightFile f;
    CHECK(parse(FILE_TEXT, 'C', &f, NULL));   /* no block: the facts only */
    CHECK(!f.sun.has && f.tweaks.lamp_count == 0 && f.light.tunnel[1100]);
    track_light_file_free(&f);
    int bad = -1;
    memset(&f, 0, sizeof f);
    CHECK(!track_light_track_parse(FILE_TEXT, strlen(FILE_TEXT), "AL1", 'A', NODES, &f, &bad));
    CHECK_EQ_INT(bad, 0);   /* another track's file: no line is bad */
    CHECK(!track_light_track_parse(FILE_TEXT, strlen(FILE_TEXT), "CY1", 'X', NODES, &f, &bad));
}

/* each of these is a bad line, and its number is reported */
static void test_bad_lines(void)
{
    static const struct { const char *text; int line; } BAD[] = {
        { "track CY1\ntime A\n  tunnel_look 1064 1171 90\n", 3 },                     /* a time block cannot make a tunnel */
        { "track CY1\ntunnel 1064 1171\ntime A\n  tunnel_look 1000 1100 90\n", 4 },   /* part of it is not a tunnel */
        { "track CY1\ntime A\n  brightness 1 2 50 shade\n", 3 },                       /* no `shade` */
        { "track CY1\ntime A\n  brightness 1 2 50 p5 p3\n", 3 },                       /* a strip backwards */
        { "track CY1\ntime A\n  brightness 1 2 50 p11\n", 3 },                         /* a point out of the row */
        { "track CY1\ntime A\n  lamp 5 100 right\n", 3 },                              /* a lamp's strip is not read yet */
        { "track CY1\ntime A\n  canyon 1 5\n", 3 },                                    /* the old words are gone */
        { "track CY1\ntime A\n  car_tone ff0000:50\n", 3 },
        { "track CY1\ntime A\ntime A\n", 3 },                                          /* a time once */
        { "track CY1\ntime A\ntunnel 1 5\n", 3 },                                      /* a fact in a block */
        { "track CY1\ntime E\n", 2 },
        { "track CY1\ntime A\n  sun 10 20\n", 3 },
        { "track CY1\ntime A\n  sun 10 20 150\n", 3 },
        { "track CY1\ntime A\n  rows 1 2 1 2 3\n", 3 },
        { "track CY1\ntime A\n  tunnel_look\n", 3 },
        { "track CY1\ntunnel 5 3\n", 2 },
        { "track CY1\nsnow 2\n", 2 },
        { "tunnel 1 5\ntrack CY1\n", 1 },                                              /* the track first */
    };
    for (size_t i = 0; i < sizeof BAD / sizeof *BAD; i++) {
        TrackLightFile f;
        int bad = 0;
        CHECK(!parse(BAD[i].text, 'A', &f, &bad));
        CHECK_EQ_INT(bad, BAD[i].line);
    }
    TrackLightFile f;   /* the same line in a time that is not asked for is checked all the same */
    int bad = 0;
    CHECK(!parse("track CY1\ntime B\n  brightness 1 2 50 shade\ntime A\n", 'A', &f, &bad));
    CHECK_EQ_INT(bad, 3);
}

static void test_a_look_in_its_forms(void)
{
    static const char TEXT[] = "track CY1\ntunnel 10 20\ntunnel 30 40 lit\ntime N\n  tunnel_look 10 20 lamps 300 ff8000 reach 1.5\n  tunnel_look 30 40 lamps 120\n  tunnel_look lamps 250\n  tunnel_look 67.5 1/0.8/0.15\n";
    TrackLightFile f;
    int bad = 0;
    CHECK(parse(TEXT, 'N', &f, &bad));
    CHECK_EQ_INT(bad, 0);
    double strength, reach;
    float cast[3];
    CHECK(track_light_tunnel_lamp_at(&f.tweaks, 15, &strength, cast, &reach) == (TRACK_LIGHT_TUNNEL_LAMP_STRENGTH | TRACK_LIGHT_TUNNEL_LAMP_COLOUR | TRACK_LIGHT_TUNNEL_LAMP_REACH) && strength == 300.0 && reach == 1.5);
    CHECK(f.tweaks.look_has_light && f.tweaks.look_light == 67.5 && f.tweaks.look_has_colour);
    track_light_file_free(&f);
}

int main(void)
{
    test_the_facts_and_a_time();
    test_the_night_block();
    test_a_brightness_strip_scales_its_points();
    test_a_time_without_a_block_and_another_track();
    test_bad_lines();
    test_a_look_in_its_forms();
    return TEST_REPORT();
}
