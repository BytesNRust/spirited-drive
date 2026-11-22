/* The track light file's retouches (track_light_file.h): the lines of a time's block (the grammar track_light_track.c hands to track_light_tweak_line) and how they go over a light: tones,
 * tunnel looks, lamps, brightness. A bad line is refused with its number. Made-up rows, no game data. */
#include "game/race/track_fence.h"
#include "game/race/terrain_light.h"
#include "game/race/track_light_file.h"
#include "game/race/track_light_track.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 40 };

/* the lines of a time's block as the retouches they make: the header and `rows` lines (the first two lines of a test's text) are skipped, every other line goes to track_light_tweak_line, a
 * bad one is refused with its line number */
static int parse_old(const char *text, size_t size, TrackLightFile *f, int *bad)
{
    memset(f, 0, sizeof *f);
    if (bad != NULL) *bad = 0;
    int line_no = 0, ok = 1;
    for (size_t at = 0; at < size && ok;) {
        char line[512];
        size_t n = 0;
        while (at < size && text[at] != '\n') {
            if (n + 1 < sizeof line) line[n++] = text[at];
            at++;
        }
        at++;
        line[n] = '\0';
        line_no++;
        if (strncmp(line, "track ", 6) == 0 || strncmp(line, "rows ", 5) == 0) continue;
        const int zones = f->tweaks.zone_count;
        if (!track_light_tweak_line(line, NODES, &f->tweaks)) {
            if (bad != NULL) *bad = line_no;
            ok = 0;
        } else if (f->tweaks.zone_count > zones) {
            f->tweaks.zones[f->tweaks.zone_count - 1].line = line_no;   /* the zone's line, as the track file's parse says it */
        }
    }
    if (ok && !track_light_tweaks_finish(&f->tweaks, bad)) ok = 0;
    if (!ok) track_light_file_free(f);
    return ok;
}


static void test_tweaks_apply_over_a_light(void)
{
    /* a time's retouches apply over a light made some other way */
    const char *text = "track CY1\nbrightness 10 19 50\nbrightness 30 30 300\ntunnel_tone 130 ff8040\n";
    TrackLightFile f = { 0 };
    CHECK(parse_old(text, strlen(text), &f, NULL));
    TrackNodeLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    memset(l.data, 100, (size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    track_light_apply(&f.tweaks, &l, 0, 0);
    CHECK(l.data[9 * TRACK_NODE_LIGHT_POINTS] == 100 && l.data[10 * TRACK_NODE_LIGHT_POINTS] == 50 && l.data[19 * TRACK_NODE_LIGHT_POINTS + 10] == 50 && l.data[20 * TRACK_NODE_LIGHT_POINTS] == 100);
    CHECK(l.data[30 * TRACK_NODE_LIGHT_POINTS] == 255);   /* clamped */
    CHECK(l.look_set && l.look_light == 130.0 && l.look_cast[0] == 1.0f && l.look_cast[1] == (float)(128.0 / 255.0) && l.look_cast[2] == (float)(64.0 / 255.0));   /* the time's look */
    track_node_light_free(&l);
    track_light_file_free(&f);
}

/* the scene tones: a stretch's, a side strip's, the 16 distinct a track has at most, and the class a polygon takes from its four corners */
static void test_tones(void)
{
    const char *text = "track CY1 time N nodes 40 flags 1\nrows 0 39 200 200 200 200 200 200 200 200 200 200 200\ntone 10 19 ffe080:50\ntone 15 25 p6 p7 ff2020:60\ntone 30 31 ffe080:50\n";
    TrackLightFile f = { 0 };
    CHECK(parse_old(text, strlen(text), &f, NULL));
    CHECK(f.tweaks.tone_count == 2 && f.tweaks.tone_run_count == 3);   /* ffe080:50 twice is one tone */
    TrackNodeLight l = { 0 };
    l.nodes = NODES;
    l.data = calloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS, 1);
    track_light_apply(&f.tweaks, &l, 0, 0);
    CHECK(l.tone_count == 2 && l.tone[0].rgb[0] == 0xff && l.tone[0].rgb[1] == 0xe0 && l.tone[0].percent == 50 && l.tone[1].rgb[1] == 0x20);
    CHECK(l.tone_id[9] == 0 && l.tone_id[10] == 1 && l.tone_mask[10] == 0x7ff);          /* a stretch: every point */
    CHECK(l.tone_id[17] == 2 && l.tone_mask[17] == ((1u << 6) | (1u << 7)));             /* the later line over the same nodes: its strip only */
    CHECK(l.tone_id[30] == 1 && l.tone_id[32] == 0);
    CHECK(track_node_light_vertex_class(&l, 12, 3) == (1 << TRACK_NODE_LIGHT_TONE_BASE_BITS) && track_node_light_vertex_class(&l, 9, 3) == 0);
    CHECK(track_node_light_vertex_class(&l, 17, 6) == (2 << TRACK_NODE_LIGHT_TONE_BASE_BITS) && track_node_light_vertex_class(&l, 17, 5) == 0);   /* the strip's points p6, p7 only */
    /* a polygon takes a tone when its four corners have the same one; the class of the road is kept under it */
    uint8_t cls[8] = { 0 }, now[6] = { 0 }, light_row[8] = { 0 };
    TrackFenceInputs in = { .tunnel = cls, .light = light_row, .light_now = now };
    const uint8_t corner[4] = { 0, 1, 2, 3 };
    cls[0] = cls[1] = cls[2] = cls[3] = (uint8_t)(1 << TRACK_NODE_LIGHT_TONE_BASE_BITS);
    track_light_set(&in, 0, corner);
    CHECK(now[5] == (1 << TRACK_NODE_LIGHT_TONE_BASE_BITS));
    cls[3] = 0;
    track_light_set(&in, 0, corner);
    CHECK(now[5] == 0);   /* one corner out of the tone: the polygon is the road's */
    cls[0] = cls[1] = cls[2] = cls[3] = (uint8_t)(1 | 3 << TRACK_NODE_LIGHT_TONE_BASE_BITS);   /* a tunnel's polygon with a tone */
    track_light_set(&in, 0, corner);
    CHECK(now[5] == (1 | 3 << TRACK_NODE_LIGHT_TONE_BASE_BITS));
    track_node_light_free(&l);
    track_light_file_free(&f);
    /* bad lines: a point past p10, a 17th distinct tone, nodes past the track */
    TrackLightFile g = { 0 };
    int bad = 0;
    { const char *t = "track CY1 time N nodes 40 flags 1\nrows 0 39 1 1 1 1 1 1 1 1 1 1 1\ntone 1 2 p11 ff0000:10\n"; CHECK(!parse_old(t, strlen(t), &g, &bad) && bad == 3); }
    char many[1024] = "track CY1 time N nodes 40 flags 1\nrows 0 39 1 1 1 1 1 1 1 1 1 1 1\n";
    for (int i = 0; i < 17; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "tone %d %d %02x0000:%d\n", i, i, i + 1, 10 + i);
    CHECK(!parse_old(many, strlen(many), &g, &bad) && bad == 2 + 17);
    { const char *t = "track CY1 time N nodes 40 flags 1\nrows 0 39 1 1 1 1 1 1 1 1 1 1 1\ntone 38 40 ff0000:10\n"; CHECK(!parse_old(t, strlen(t), &g, &bad) && bad == 3); }
}

/* the track file's tunnel facts and the time's halls make the closed zones for the weather: a tunnel and a hall (a tunnel with a lot of light) are closed, the open road is not */
static void test_weather_closed_zones(void)
{
    const char *text = "track AL1\ntunnel 100 140\ntunnel 300 330 lit\ntunnel 650 660\ntime B\n  tunnel_look 60\n";
    TrackLightFile f;
    CHECK(track_light_track_parse(text, strlen(text), "AL1", 'B', 800, &f, NULL));
    track_light_apply(&f.tweaks, &f.light, 0, 0);
    const TrackNodeLight *l = &f.light;
    CHECK_EQ_INT(track_node_light_weather_closed(l, 602), 0);   /* open road */
    CHECK_EQ_INT(track_node_light_weather_closed(l, 597), 0);
    CHECK_EQ_INT(track_node_light_weather_closed(l, 120), 1);   /* a tunnel: closed */
    CHECK_EQ_INT(track_node_light_weather_closed(l, 100), 1);
    CHECK_EQ_INT(track_node_light_weather_closed(l, 141), 0);
    CHECK_EQ_INT(track_node_light_weather_closed(l, 310), 1);   /* a hall (TR7's steelworks) is a tunnel with a lot of light: it has a roof */
    CHECK(l->hall != NULL && l->hall[310] && l->tunnel[310] == 0 && l->tunnel[120] == 1);
    CHECK_EQ_INT(track_node_light_weather_closed(l, 655), 1);
    CHECK_EQ_INT(track_node_light_weather_closed(NULL, 5), 0);
    track_light_file_free(&f);
}

/* `sun AZIMUTH ELEVATION SHADOW`: degrees, degrees, percent; `-` leaves a word to the generator; one a file */
static void test_the_sun_line(void)
{
    const char *head = "track CY1 time A nodes 40 flags 1\nrows 0 39 200 200 200 200 200 200 200 200 200 200 200\n";
    char text[512];
    TrackLightFile f;
    int bad = 0;
    snprintf(text, sizeof text, "%ssun 150 20 55\n", head);
    CHECK(parse_old(text, strlen(text), &f, &bad));
    CHECK(f.tweaks.sun_set == (TRACK_LIGHT_SUN_AZIMUTH | TRACK_LIGHT_SUN_ELEVATION | TRACK_LIGHT_SUN_SHADOW));
    CHECK(f.tweaks.sun_azimuth == 150.0f && f.tweaks.sun_elevation == 20.0f && f.tweaks.sun_shadow == 55.0f);
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%ssun - - 60\n", head);   /* only the shadow: the direction and the height are the generator's */
    CHECK(parse_old(text, strlen(text), &f, &bad));
    CHECK(f.tweaks.sun_set == TRACK_LIGHT_SUN_SHADOW && f.tweaks.sun_shadow == 60.0f);
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%ssun - - - 800\n", head);   /* only the reach (a fourth word, units) */
    CHECK(track_light_parse(text, strlen(text), "CY1", 'A', NODES, 1, &f, &bad));
    CHECK(f.tweaks.sun_set == TRACK_LIGHT_SUN_REACH && f.tweaks.sun_reach == 800.0f);
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%ssun 150 20 55 -\n", head);   /* a `-` reach is the generator's */
    CHECK(track_light_parse(text, strlen(text), "CY1", 'A', NODES, 1, &f, &bad) && !(f.tweaks.sun_set & TRACK_LIGHT_SUN_REACH));
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%sbrightness 1 2 90\n", head);   /* no line: nothing set */
    CHECK(parse_old(text, strlen(text), &f, &bad) && f.tweaks.sun_set == 0);
    track_light_file_free(&f);
    const char *bads[] = { "sun - - -\n", "sun 150 20\n", "sun 150 95 50\n", "sun 0 0 50\n", "sun 0 20 120\n", "sun 0 20 50 0\n", "sun 0 20 50 2001\n", "sun 0 20 50 100 1\n", "sun 150 20 55\nsun 10 20 30\n", "sun x 20 50\n", "sun 15o 20 50\n" };
    for (size_t i = 0; i < sizeof bads / sizeof bads[0]; i++) {
        snprintf(text, sizeof text, "%s%s", head, bads[i]);
        bad = 0;
        CHECK(!parse_old(text, strlen(text), &f, &bad) && bad == 3 + (i == 8));
    }
}

/* the new lines: tunnel [light] [colour | lit], tunnel_tone, the colours' three spellings, one look a time, and what is no longer read */
static void test_the_tunnel_lines(void)
{
    const char *head = "track CY1 time N nodes 40 flags 1\nrows 0 39 200 200 200 200 200 200 200 200 200 200 200\n";
    char text[512];
    TrackLightFile f;
    TrackNodeLight l = { 0 };
    l.nodes = NODES;
    l.data = calloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS, 1);
    snprintf(text, sizeof text, "%stunnel_tone 115 ffe080\n", head);   /* the track's look this time: a light (100 is full) and a colour */
    CHECK(parse_old(text, strlen(text), &f, NULL));
    track_light_apply(&f.tweaks, &l, 0, 0);
    CHECK(l.look_set && l.look_light == 115.0 && l.look_cast[0] == 1.0f && l.look_cast[1] == (float)(224.0 / 255.0) && l.look_cast[2] == (float)(128.0 / 255.0));
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%stunnel 5 9 60\ntunnel 15 19 60 ff0000:50\n", head);   /* zones with a look of their own: a tone slot each (the colour at half strength is half way to no change) */
    CHECK(parse_old(text, strlen(text), &f, NULL));
    CHECK(f.tweaks.zone_count == 2 && !f.tweaks.look_has_light && f.tweaks.tone_count == 2 && f.tweaks.zones[0].tone == 0 && f.tweaks.zones[1].tone == 1);
    track_light_apply(&f.tweaks, &l, 0, 0);
    double zone_light = 0;
    float zone_cast[3];
    CHECK(track_node_light_zone_look_at(&l, 7, &zone_light, zone_cast) && zone_light == 60.0 && zone_cast[0] == 1.0f && zone_cast[1] == 1.0f);   /* no colour: the time's (none: no change) */
    CHECK(track_node_light_zone_look_at(&l, 17, &zone_light, zone_cast) && zone_light == 60.0 && zone_cast[0] == 1.0f && zone_cast[1] == 0.5f && zone_cast[2] == 0.5f);
    CHECK(track_node_light_zone_look_at(&l, 30, &zone_light, zone_cast) && zone_light == 10.0);   /* outside the zones: the time's look, the dark default */
    CHECK(l.tone_id[7] == 1 && l.tone_id[17] == 2 && l.tone[1].look == 1 && l.tone_mask[17] == 0x7ff && l.tone_id[12] == 0);
    track_light_file_free(&f);
    /* a zone's look is the time's `tunnel_tone` where it leaves a part out, and no slot where it is the same; zones with one look share a slot */
    snprintf(text, sizeof text, "%stunnel_tone 60 ff0000\ntunnel 5 9 60 ff0000\ntunnel 11 12 60\ntunnel 15 16 80\ntunnel 20 21 80 ff0000\ntunnel 25 26 1/0.5/0.25\ntunnel 30 31 1/0.5/0.25\n", head);
    CHECK(parse_old(text, strlen(text), &f, NULL));
    CHECK(f.tweaks.tone_count == 2 && f.tweaks.zones[0].tone == -1 && f.tweaks.zones[1].tone == -1 && f.tweaks.zones[2].tone == 0 && f.tweaks.zones[3].tone == 0 && f.tweaks.zones[4].tone == 1 && f.tweaks.zones[5].tone == 1);
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%stunnel 5 9\n", head);   /* no look at all: the default, dark (10 %) and without a colour */
    CHECK(parse_old(text, strlen(text), &f, NULL));
    TrackNodeLight d = { 0 };
    d.nodes = NODES, d.data = calloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS, 1);
    track_light_apply(&f.tweaks, &d, 0, 0);
    CHECK(d.look_set && d.look_light == 10.0 && d.look_cast[0] == 1.0f && d.look_cast[1] == 1.0f && d.look_cast[2] == 1.0f && d.tunnel[7] == 1 && d.tunnel[4] == 0);
    track_light_file_free(&f);
    track_node_light_free(&d);
    int bad = 0;
    const char *refused[] = { "tunnel_tone 60\ntunnel_tone 70\n",            /* one default look a time: a second tunnel_tone that differs */
                              "tunnel_tone 60 ff0000\ntunnel_tone 60 00ff00\n",   /* ... or another colour */
                              "car_disc 1 0x1p-1 0x1p-1 0x1p-1 0x1p-1 0x1p-1 0x1p-1\n",   /* gone: the car takes its zone's look */
                              "tunnel_tone\n", "tunnel_tone day 1 0.5 0.25 130\n", "tunnel_tone night 1 0.5 0.25 130\n",   /* no light, or the old form */
                              "hall 5 9\n", "cast neutral 1 city 0\n", "canyon 1 2\n", "car_dim 1 2\n", "present tunnel\n",   /* no longer read here: places are the track's file */
                              "tunnel 5 9 lit 60\n", "tunnel 5 9 60 lit\n", "tunnel_tone 60 lit\n", "tunnel 5 9 ffffff ffffff\n", "tunnel 5 9 60 ff00\n", "tunnel 5 9 60 ff0000:101\n", "tunnel 5 9 60 2/0/0\n" };
    for (size_t i = 0; i < sizeof refused / sizeof *refused; i++) {
        snprintf(text, sizeof text, "%s%s", head, refused[i]);
        CHECK(!parse_old(text, strlen(text), &f, &bad) && bad == (i < 2 ? 4 : 3));   /* the line that disagrees */
    }
    snprintf(text, sizeof text, "%stunnel 5 9 60 1/0.5/0.25\ntunnel 11 12 60 ff8040\n", head);   /* the written-out factors, as the export writes the made looks: ff8040 is 1, 0.50196, 0.251, not 1/0.5/0.25: two slots */
    CHECK(parse_old(text, strlen(text), &f, &bad) && f.tweaks.tone_count == 2);
    track_light_file_free(&f);
    /* the slots are shared with the `tone` lines: 16 in all, the 17th is a bad line, a zone's or a tone's, whichever comes last in the file */
    char many[2048];
    snprintf(many, sizeof many, "%s", head);
    for (int i = 0; i < 17; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "tunnel %d %d %d\n", i, i, 20 + i);
    CHECK(!parse_old(many, strlen(many), &f, &bad) && bad == 2 + 17);
    snprintf(many, sizeof many, "%s", head);
    for (int i = 0; i < 15; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "tone %d %d %02x0000:%d\n", i, i, i + 1, 10 + i);
    snprintf(many + strlen(many), sizeof many - strlen(many), "tunnel 20 21 70\ntunnel 22 23 71\n");
    CHECK(!parse_old(many, strlen(many), &f, &bad) && bad == 2 + 15 + 2);
    track_node_light_free(&l);
}

#define LAMP_HEAD "track CY1 time N nodes 40 flags 1\nrows 0 39 100 100 100 100 100 100 100 100 100 100 100\n"

static int parse_lamp_lines(const char *lines, TrackLightFile *f, int *bad)
{
    char text[2048];
    snprintf(text, sizeof text, "%s%s", LAMP_HEAD, lines);
    return parse_old(text, strlen(text), f, bad);
}

/* `lamp <node> <light%> [RRGGBB[:strength]] [radius]`: the colour is optional (no colour is the warm street lamp), the radius is optional (TRACK_NODE_LIGHT_LAMP_RADIUS) */
static void test_lamp_lines_parse(void)
{
    TrackLightFile f = { 0 };
    int bad = 0;
    CHECK(parse_lamp_lines("lamp 10 80\nlamp 11 100 ff2000\nlamp 12 150 8\nlamp 13 30 00ff80:50 12\nlamp 14 100 ff2000 3\nlamp 17 100 ff2000:100\nlamp 18 100 ff2000:0\n", &f, &bad));
    const TrackLightTweaks *t = &f.tweaks;
    CHECK(t->lamp_count == 7);
    CHECK(t->lamps[0].node == 10 && t->lamps[0].percent == 80 && !t->lamps[0].coloured && t->lamps[0].radius == TRACK_NODE_LIGHT_LAMP_RADIUS);   /* no colour: warm; no radius: the default */
    CHECK(t->lamps[1].coloured && t->lamps[1].strength == 100 && t->lamps[1].rgb[0] == 0xff && t->lamps[1].rgb[1] == 0x20 && t->lamps[1].rgb[2] == 0 && t->lamps[1].radius == TRACK_NODE_LIGHT_LAMP_RADIUS);
    CHECK(!t->lamps[2].coloured && t->lamps[2].radius == 8 && t->lamps[2].percent == 150);   /* a radius with no colour, above 100 percent */
    CHECK(t->lamps[3].coloured && t->lamps[3].strength == 50 && t->lamps[3].radius == 12 && t->lamps[3].percent == 30);   /* the Spanish word, a colour with a strength */
    CHECK(t->lamps[4].coloured && t->lamps[4].radius == 3 && t->lamps[5].strength == 100 && t->lamps[6].strength == 0);
    track_light_file_free(&f);
    /* bad lines: 0 percent or past the cap, no light, node past the track, a colour that is not hex, a strength past 100, a radius of 0 or 65, two radii, something after the radius, `lift` (gone) and the
     * pool lines the lamp replaced */
    const char *bads[] = { "lamp 10 0\n", "lamp 10 1001\n", "lamp 10\n", "lamp 40 50\n", "lamp 10 50 gg0000\n", "lamp 10 50 ff0000:101\n", "lamp 10 50 ff0000:\n", "lamp 10 50 ff0000x5\n", "lamp 10 50 0\n", "lamp 10 50 65\n",
                           "lamp 10 50 5 6\n", "lamp 10 50 5 ff0000\n", "lamp 10 50 ff0000 5 7\n", "lamp 10 50 ff00000\n", "lamp x 50\n", "lamp 10 50 lift\n", "lamp 10 50 ff0000 lift\n",
                           "pool 10 12 3\n", "car_pool 10 12 3\n", "present pool\n", "pozo 10 12 3\n" };
    for (size_t i = 0; i < sizeof bads / sizeof *bads; i++) CHECK(!parse_lamp_lines(bads[i], &f, &bad) && bad == 3);
}

/* the falloff: full under the lamp, (1 - d / reach)^2, nothing at the reach and past it, smooth between */
static void test_lamp_falloff(void)
{
    CHECK_EQ_INT(track_node_light_lamp_falloff(0.0f, 30.0f) == 1.0f, 1);
    CHECK_EQ_INT(track_node_light_lamp_falloff(15.0f, 30.0f) == 0.25f, 1);
    CHECK_EQ_INT(track_node_light_lamp_falloff(30.0f, 30.0f) == 0.0f && track_node_light_lamp_falloff(31.0f, 30.0f) == 0.0f, 1);
    CHECK_EQ_INT(track_node_light_lamp_falloff(5.0f, 0.0f) == 0.0f, 1);
    float last = 2.0f;
    for (int d = 0; d <= 30; d++) {   /* never rises, steps are small */
        const float f = track_node_light_lamp_falloff((float)d, 30.0f);
        CHECK(f <= last && last - f < 0.1f + (d == 0 ? 2.0f : 0.0f));
        last = f;
    }
}

static int near_f(float a, float b) { return fabsf(a - b) < 1e-4f; }

static TrackNodeLight lamp_light(const char *lines, TrackLightFile *f, int lamps, int ring)
{
    CHECK(parse_lamp_lines(lines, f, NULL));
    TrackNodeLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    memset(l.data, 100, (size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    l.pool_class = calloc(NODES, 1), l.car_pool = calloc(NODES, 1);   /* what the code made: a lamp line replaces it */
    track_light_apply(&f->tweaks, &l, lamps, ring);
    return l;
}

/* a lamp line is the light's lamp list (by node, the colour its strength): warm with no colour, the line's colour mixed toward warm by its strength; the old pools go; with the flag off or no lamp
 * line nothing changes */
static void test_lamp_apply(void)
{
    TrackLightFile f = { 0 };
    TrackNodeLight on = lamp_light("lamp 30 55 ff2000 3\nlamp 20 100\nlamp 25 200 ff0000:0\n", &f, 1, 0);
    CHECK(on.lamps != NULL && on.lamp_n == 3 && on.lamp_reach_nodes == TRACK_NODE_LIGHT_LAMP_RADIUS && !on.lamp_placed);
    CHECK(on.lamps[0].node == 20 && on.lamps[1].node == 25 && on.lamps[2].node == 30);   /* by node */
    CHECK(near_f(on.lamps[0].rgb[0], 1.0f) && near_f(on.lamps[0].rgb[1], 0.9f) && near_f(on.lamps[0].rgb[2], 0.55f));   /* the warm lamp at 100 percent */
    CHECK(near_f(on.lamps[1].rgb[0], 2.0f) && near_f(on.lamps[1].rgb[1], 1.8f));   /* strength 0: the warm colour again, at 200 percent */
    CHECK(on.lamps[2].rgb[0] > 0.54f && on.lamps[2].rgb[0] < 0.56f && on.lamps[2].rgb[1] < 0.2f && near_f(on.lamps[2].rgb[2], 0.0f));   /* a colour at 55 percent: red, little green */
    CHECK(on.lamps[0].reach == (float)TRACK_NODE_LIGHT_LAMP_RADIUS && on.lamps[2].reach == 3.0f);   /* nodes until it is placed */
    CHECK(on.pool_class == NULL && on.car_pool == NULL);   /* the old pools are gone */
    CHECK(memcmp(on.data + 20 * TRACK_NODE_LIGHT_POINTS, on.data + 5 * TRACK_NODE_LIGHT_POINTS, TRACK_NODE_LIGHT_POINTS) == 0);   /* the rows are not touched at load */
    track_node_light_free(&on);
    track_light_file_free(&f);
    /* the port flag off: the lines are read and laid nowhere; the pools stay */
    TrackLightFile off = { 0 };
    TrackNodeLight none = lamp_light("lamp 20 100 ff0000\n", &off, 0, 0);
    CHECK(none.lamps == NULL && none.lamp_n == 0 && none.pool_class != NULL && none.car_pool != NULL);
    track_node_light_free(&none);
    track_light_file_free(&off);
    /* a file with no lamp line, the flag on: the light is the same as with it off */
    TrackLightFile plain = { 0 };
    TrackNodeLight a = lamp_light("brightness 5 6 50\n", &plain, 1, 0), b = lamp_light("brightness 5 6 50\n", &plain, 0, 0);
    CHECK(memcmp(a.data, b.data, (size_t)NODES * TRACK_NODE_LIGHT_POINTS) == 0 && a.lamps == NULL && a.pool_class != NULL);
    track_node_light_free(&a), track_node_light_free(&b);
    track_light_file_free(&plain);
}

/* `brightness FROM TO PERCENT [left|right] [shade]`: the side and Only shadows words, either one or both; the line of before is as it was */
static void test_brightness_words_parse(void)
{
    TrackLightFile f = { 0 };
    int bad = 0;
    CHECK(parse_lamp_lines("brightness 5 6 50\nbrightness 5 6 50 left\nbrightness 5 6 50 right\nbrightness 5 6 50 shade\nbrightness 5 6 50 left shade\nbrightness 5 6 70 shade right\n", &f, &bad));
    const TrackLightTweaks *t = &f.tweaks;
    CHECK_EQ_INT(t->brightness_count, 6);
    CHECK(t->brightness[0].side == TRACK_LIGHT_SIDE_BOTH && !t->brightness[0].shade && t->brightness[0].percent == 50);   /* no words: today's line */
    CHECK(t->brightness[1].side == TRACK_LIGHT_SIDE_LEFT && !t->brightness[1].shade);
    CHECK(t->brightness[2].side == TRACK_LIGHT_SIDE_RIGHT && !t->brightness[2].shade);
    CHECK(t->brightness[3].side == TRACK_LIGHT_SIDE_BOTH && t->brightness[3].shade);
    CHECK(t->brightness[4].side == TRACK_LIGHT_SIDE_LEFT && t->brightness[4].shade);
    CHECK(t->brightness[5].side == TRACK_LIGHT_SIDE_RIGHT && t->brightness[5].shade && t->brightness[5].percent == 70);
    track_light_file_free(&f);
    const char *bad_lines[] = { "brightness 5 6 50 middle\n", "brightness 5 6 50 left right\n", "brightness 5 6 50 shade shade\n", "brightness 5 6 50 left left\n", "brightness 5 6 50 shade extra\n", "brightness 5 6 50 Left\n", "brightness 5 6 50left\n", "brightness 5 6 50shade\n", "brightness 5 6 50 note\n" };
    for (size_t i = 0; i < sizeof bad_lines / sizeof bad_lines[0]; i++) {
        TrackLightFile g = { 0 };
        bad = 0;
        CHECK(!parse_lamp_lines(bad_lines[i], &g, &bad));   /* a word that is not one of those: a bad line, as any bad line */
        track_light_file_free(&g);
    }
}

/* a light whose every node has the same row: middle 255, right 255 200 150 100 50, left 255 200 150 100 50 */
static TrackNodeLight shade_light(const char *lines, TrackLightFile *f)
{
    static const uint8_t ROW[TRACK_NODE_LIGHT_POINTS] = { 255, 255, 200, 150, 100, 50, 255, 200, 150, 100, 50 };
    CHECK(parse_lamp_lines(lines, f, NULL));
    TrackNodeLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS);
    for (int n = 0; n < NODES; n++) memcpy(l.data + (size_t)n * TRACK_NODE_LIGHT_POINTS, ROW, sizeof ROW);
    track_light_apply(&f->tweaks, &l, 0, 0);
    return l;
}

static const uint8_t *row_at(const TrackNodeLight *l, int node)
{
    return l->data + (size_t)node * TRACK_NODE_LIGHT_POINTS;
}

/* a side: the middle point counts for both, the right is points 1..5, the left 6..10 */
static void test_brightness_side(void)
{
    TrackLightFile f = { 0 };
    TrackNodeLight l = shade_light("brightness 5 6 50 left\n", &f);
    const uint8_t *r = row_at(&l, 5);
    CHECK(r[0] == 127 && r[1] == 255 && r[2] == 200 && r[5] == 50 && r[6] == 127 && r[7] == 100 && r[10] == 25);   /* the left half and the middle; the right as it was */
    CHECK(memcmp(row_at(&l, 4), row_at(&l, 7), TRACK_NODE_LIGHT_POINTS) == 0 && row_at(&l, 4)[7] == 200);   /* outside the run: untouched */
    track_node_light_free(&l), track_light_file_free(&f);
    TrackLightFile g = { 0 };
    l = shade_light("brightness 5 6 50 right\n", &g);
    r = row_at(&l, 6);
    CHECK(r[0] == 127 && r[1] == 127 && r[2] == 100 && r[5] == 25 && r[6] == 255 && r[7] == 200 && r[10] == 50);
    track_node_light_free(&l), track_light_file_free(&g);
}

/* port (cars read the row): the lane's light is the row's by the car's side: centre, right edge, left edge, linear between, held past the edges */
static void test_car_lane(void)
{
    TrackLightFile f = { 0 };
    TrackNodeLight l = shade_light("brightness 5 6 50 right\n", &f);   /* nodes 5, 6: middle 127, right edge 127, left edge 255 */
    l.car_row = 1;   /* the light's switch (drive_light.c sets it) */
    CHECK(fabsf(track_node_light_lane(&l, 6, 0.0, 6.0, 6.0) - 127.0f / 255.0f) < 1e-4f);   /* centre */
    CHECK(fabsf(track_node_light_lane(&l, 6, 6.0, 6.0, 6.0) - 127.0f / 255.0f) < 1e-4f);   /* right edge */
    CHECK(fabsf(track_node_light_lane(&l, 6, -6.0, 6.0, 6.0) - 1.0f) < 1e-4f);           /* left edge */
    CHECK(fabsf(track_node_light_lane(&l, 6, -3.0, 6.0, 6.0) - (127.0f + 64.0f) / 255.0f) < 1.5f / 255.0f);   /* half way to the left */
    CHECK(fabsf(track_node_light_lane(&l, 6, -60.0, 6.0, 6.0) - 1.0f) < 1e-4f);          /* past the edge: held */
    CHECK(track_node_light_lane(&l, 4, 6.0, 6.0, 6.0) == 1.0f);                            /* outside the run: full */
    CHECK(track_node_light_lane(NULL, 6, 0.0, 6.0, 6.0) == 1.0f);
    uint8_t tunnel[NODES] = { 0 };
    tunnel[6] = 1;
    l.tunnel = tunnel;   /* in a tunnel the road's mean is the look's already */
    CHECK(track_node_light_lane(&l, 6, 6.0, 6.0, 6.0) == 1.0f);   /* a tunnel's road is capped alike across (light_at): no lateral difference left, and no second darkening */
    CHECK(track_node_light_lane(&l, 6, -6.0, 6.0, 6.0) == 1.0f);
    l.tunnel = NULL;
    track_node_light_free(&l), track_light_file_free(&f);
    TrackLightFile g = { 0 };
    l = shade_light("brightness 5 6 50 right\n", &g);
    CHECK(l.car_row == 0);   /* nothing in a file's lines sets it */
    track_node_light_free(&l), track_light_file_free(&g);
}

/* port (cars read the row): the side of the road and the lane's light go together: a left strip darkens a car on the left, not the right (the sign was seen in a real run, TNFS_START_SIDE) */
static void test_car_lane_side(void)
{
    CHECK(fabs(track_node_light_side(0, 0, 0, 6, 5, 1) - 5.0) < 1e-9);    /* the road runs on +z: +x is its right */
    CHECK(fabs(track_node_light_side(0, 0, 0, 6, -4, 2) + 4.0) < 1e-9);
    CHECK(fabs(track_node_light_side(0, 0, 6, 0, 1, 3) + 3.0) < 1e-9);     /* running on +x: +z is its left */
    CHECK(track_node_light_side(1, 1, 1, 1, 5, 5) == 0.0);
    TrackLightFile f = { 0 };
    TrackNodeLight l = shade_light("brightness 5 6 30 left\n", &f);       /* the left side's points 30 %, the right's 100: the middle point counts for both */
    const float left = track_node_light_lane(&l, 5, track_node_light_side(0, 0, 0, 6, -7, 1), 5.0, 9.0);
    const float right = track_node_light_lane(&l, 5, track_node_light_side(0, 0, 0, 6, 7, 1), 5.0, 9.0);
    CHECK(fabsf(left - 0.3f) < 0.01f && right > 0.8f && right < 0.9f);    /* the left edge's 30 %; 7 of 9 m toward the right edge's full light from the middle's 30 */
    track_node_light_free(&l), track_light_file_free(&f);
}

/* Only shadows: the darkness the sun left (255 - v) is scaled by 100 / percent: a point in full light never changes, 100 changes nothing, less than 100 darkens, more lightens */
static void test_brightness_only_shadows(void)
{
    TrackLightFile f = { 0 };
    TrackNodeLight l = shade_light("brightness 5 6 50 left shade\n", &f);
    const uint8_t *r = row_at(&l, 5);
    CHECK(r[0] == 255 && r[1] == 255 && r[2] == 200 && r[3] == 150 && r[5] == 50);   /* the middle and the right: as they were */
    CHECK(r[6] == 255 && r[7] == 145 && r[8] == 45 && r[9] == 0 && r[10] == 0);        /* the left, darker the more it was in shade */
    track_node_light_free(&l), track_light_file_free(&f);
    TrackLightFile g = { 0 };
    l = shade_light("brightness 0 39 100 shade\n", &g);
    CHECK(row_at(&l, 5)[7] == 200 && row_at(&l, 5)[10] == 50);   /* 100: nothing changes */
    track_node_light_free(&l), track_light_file_free(&g);
    TrackLightFile h = { 0 };
    l = shade_light("brightness 5 5 200 shade\n", &h);
    CHECK(row_at(&l, 5)[2] == 227 && row_at(&l, 5)[5] == 152 && row_at(&l, 5)[0] == 255);   /* more than 100 lightens: half the darkness */
    track_node_light_free(&l), track_light_file_free(&h);
    /* a point at 255 is never changed, at any percent; and the curve has no step: a darker point never comes out lighter than a lighter one */
    static const int PERCENTS[] = { 0, 1, 10, 50, 99, 100, 101, 250, 1000 };
    for (size_t k = 0; k < sizeof PERCENTS / sizeof PERCENTS[0]; k++) {
        char line[64];
        snprintf(line, sizeof line, "brightness 0 0 %d shade\n", PERCENTS[k]);
        TrackLightFile p = { 0 };
        CHECK(parse_lamp_lines(line, &p, NULL));
        TrackNodeLight q = { 0 };
        q.nodes = NODES, q.data = calloc((size_t)NODES * TRACK_NODE_LIGHT_POINTS, 1);
        int previous = -1, steps_ok = 1;
        for (int v = 0; v < 256; v++) {
            memset(q.data, v, TRACK_NODE_LIGHT_POINTS);   /* node 0: every point at v */
            track_light_apply(&p.tweaks, &q, 0, 0);
            const int out = q.data[0];
            if (v == 255 && out != 255) steps_ok = 0;
            if (out < previous) steps_ok = 0;                                          /* lighter in, never darker out */
            if (PERCENTS[k] >= 50 && v > 0 && out - previous > 3) steps_ok = 0;       /* no step between a point at 255 and its neighbour at 254 */
            previous = out;
            if (PERCENTS[k] > 0 && PERCENTS[k] <= 100 && out > v) steps_ok = 0;      /* 100 or less never lightens */
            if (PERCENTS[k] >= 100 && out < v) steps_ok = 0;                          /* 100 or more never darkens */
        }
        CHECK(steps_ok);
        track_node_light_free(&q), track_light_file_free(&p);
    }
}

/* the shade lines act on the rows as the file gives them, then the plain lines in file order, whatever order they are written in */
static void test_brightness_shade_before_plain(void)
{
    TrackLightFile f = { 0 }, g = { 0 };
    TrackNodeLight a = shade_light("brightness 5 6 50\nbrightness 5 6 50 shade\n", &f), b = shade_light("brightness 5 6 50 shade\nbrightness 5 6 50\n", &g);
    CHECK(row_at(&a, 5)[2] == 72 && row_at(&a, 5)[4] == 0);   /* 200: shaded to 145, then halved; 100 was shaded to 0 */
    CHECK(memcmp(a.data, b.data, (size_t)NODES * TRACK_NODE_LIGHT_POINTS) == 0);
    track_node_light_free(&a), track_node_light_free(&b), track_light_file_free(&f), track_light_file_free(&g);
}

/* `tunnel_lamp [FROM TO] <strength> [colour] [reach]`: a general line and ranges, what a line leaves out inherited, the last line over a node replacing the earlier, the limits, and no line at all
 * giving nothing (the code's values stay) */
static void test_tunnel_lamp_lines(void)
{
    TrackLightFile f = { 0 };
    int bad = 0;
    double strength = -1.0, reach = -1.0;
    float cast[3] = { 9, 9, 9 };
    CHECK(parse_lamp_lines("tunnel 5 9\n", &f, &bad));
    CHECK(track_light_tunnel_lamp_at(&f.tweaks, 7, &strength, cast, &reach) == 0 && strength == -1.0 && cast[0] == 9.0f && reach == -1.0);   /* no line: nothing set, the caller keeps the code's */
    track_light_file_free(&f);
    CHECK(parse_lamp_lines("tunnel_lamp 300 ffc880 1.8\ntunnel_lamp 10 20 220 ffffff 1.4\ntunnel_lamp 15 17 50\ntunnel_lamp 30 31 120 00ff00\n", &f, &bad));
    const TrackLightTweaks *t = &f.tweaks;
    CHECK(t->tunnel_lamp_count == 4 && t->tunnel_lamps[0].general && !t->tunnel_lamps[1].general && t->tunnel_lamps[1].from == 10 && t->tunnel_lamps[1].to == 20);
    CHECK(track_light_tunnel_lamp_at(t, 2, &strength, cast, &reach) == 7 && strength == 300.0 && reach == 1.8 && cast[0] == 1.0f && fabsf(cast[1] - 200.0f / 255.0f) < 1e-6f);   /* the general line */
    CHECK(track_light_tunnel_lamp_at(t, 12, &strength, cast, &reach) == 7 && strength == 220.0 && reach == 1.4 && cast[1] == 1.0f && cast[2] == 1.0f);   /* a range: all three */
    CHECK(track_light_tunnel_lamp_at(t, 16, &strength, cast, &reach) == 7 && strength == 50.0 && reach == 1.8 && cast[0] == 1.0f && fabsf(cast[2] - 128.0f / 255.0f) < 1e-6f);   /* a later line replaces the earlier one on its nodes: 50, the colour and reach the general line's, not the range 10..20's */
    CHECK(track_light_tunnel_lamp_at(t, 30, &strength, cast, &reach) == 7 && strength == 120.0 && cast[1] == 1.0f && cast[0] == 0.0f && reach == 1.8);
    track_light_file_free(&f);
    CHECK(parse_lamp_lines("tunnel_lamp 15 17 50\n", &f, &bad));   /* a range with no general line: the rest is the code's */
    CHECK(track_light_tunnel_lamp_at(&f.tweaks, 16, &strength, cast, &reach) == TRACK_LIGHT_TUNNEL_LAMP_STRENGTH && strength == 50.0);
    CHECK(track_light_tunnel_lamp_at(&f.tweaks, 14, &strength, cast, &reach) == 0);
    track_light_file_free(&f);
    CHECK(parse_lamp_lines("tunnel_lamp 300 123456 4\ntunnel_lamp 0\ntunnel_lamp 1000 0.5/0.25/1:50 0.5\n", &f, &bad) && f.tweaks.tunnel_lamp_count == 3);   /* a colour of 6 digits is a colour, not nodes; the limits; r/g/b */
    CHECK(f.tweaks.tunnel_lamps[0].general && f.tweaks.tunnel_lamps[0].has_colour && f.tweaks.tunnel_lamps[0].reach == 4.0);
    track_light_file_free(&f);
    /* bad lines: nothing, a strength past 1000 or below 0, a reach of 0.4 or 4.1, two reaches, a colour after the reach, nodes the wrong way round or past the track, nodes with no strength, a word that is nothing, more words than the line has */
    const char *bads[] = { "tunnel_lamp\n", "tunnel_lamp 1001\n", "tunnel_lamp -1\n", "tunnel_lamp 300 ffc880 0.4\n", "tunnel_lamp 300 ffc880 4.1\n", "tunnel_lamp 300 1.5 1.5\n", "tunnel_lamp 300 1.5 ffc880\n",
                           "tunnel_lamp 20 10 100\n", "tunnel_lamp 10 40 100\n", "tunnel_lamp 10 20\n", "tunnel_lamp x\n", "tunnel_lamp 300 ffc880 1.8 7\n", "tunnel_lamp 10 20 100 ffc880 1.5 9\n", "tunnel_lamp 300 gg0000\n" };
    for (size_t i = 0; i < sizeof bads / sizeof *bads; i++) CHECK(!parse_lamp_lines(bads[i], &f, &bad) && bad == 3);
    /* the 65th line is refused */
    char text[8192];
    int n = snprintf(text, sizeof text, "%s", LAMP_HEAD);
    for (int i = 0; i <= TRACK_LIGHT_MAX_TUNNEL_LAMPS; i++) n += snprintf(text + n, sizeof text - (size_t)n, "tunnel_lamp %d %d 100\n", i % 30, i % 30 + 1);
    CHECK(!parse_old(text, (size_t)n, &f, &bad) && bad == 3 + TRACK_LIGHT_MAX_TUNNEL_LAMPS);
}

int main(void)
{
    test_weather_closed_zones();
    test_the_tunnel_lines();
    test_the_sun_line();
    test_tones();
    test_tweaks_apply_over_a_light();
    test_lamp_lines_parse();
    test_lamp_falloff();
    test_lamp_apply();
    test_brightness_words_parse();
    test_brightness_side();
    test_car_lane();
    test_car_lane_side();
    test_brightness_only_shadows();
    test_brightness_shade_before_plain();
    test_tunnel_lamp_lines();
    return TEST_REPORT();
}
