/* The Track light panel's model of the new file format (editor/light_track.h): a file is held as its lines, the panel's are the facts `tunnel` and `snow` and a time's `sun`, `tunnel_look`,
 * `lamp`, `brightness`, `tone`, `rows`; a file that was not changed is written back as the same bytes; a change the game would refuse is not made. usage: test_editor_light_track [DIR]
 * (DIR: a folder of track light files, each of which must read, round-trip and have no line the game refuses; the repo's, once the converted files are in it). */
#include "light_track.h"
#include "test.h"

#ifndef _WIN32
#include <dirent.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 1200 };

static const char CY1[] =
    "# tnfs track light 2\n"
    "track CY1\n"
    "tunnel 1064 1171\n"
    "tunnel 1 62 lit   # the steelworks\n"
    "snow 0\n"
    "snow_map 3 36\n"
    "\n"
    "time A\n"
    "  sun 150 20 55\n"
    "  tunnel_look 104 ff5a0d\n"
    "  tunnel_look 1064 1171 90 lamps 220 reach 2\n"
    "\n"
    "time N\n"
    "  tunnel_look 115\n"
    "  lamp 130 150 ff2000:80 8\n"
    "  brightness 852 943 60 left\n"
    "  tone 10 20 p2 p4 ff8000:50\n"
    "  rows 5 6 255 255 255 255 255 255 255 255 255 255 255\n";

static int parse(const char *text, unsigned nodes, LightTrack *f)
{
    char why[LT_WHY];
    const int ok = light_track_parse(text, strlen(text), nodes, f, why, sizeof why);
    if (!ok) printf("parse: %s\n", why);
    return ok;
}

static char *text_of(const LightTrack *f)
{
    size_t n;
    char *t = light_track_format(f, &n);
    CHECK(t != NULL && strlen(t) == n);
    return t;
}

static int same(const LightTrack *f, const char *want)
{
    char *t = text_of(f);
    const int eq = t != NULL && strcmp(t, want) == 0;
    if (!eq) printf("--- got\n%s--- want\n%s---\n", t, want);
    free(t);
    return eq;
}

static void reads(void)
{
    LightTrack f;
    CHECK(parse(CY1, NODES, &f));
    CHECK(strcmp(f.track, "CY1") == 0);
    CHECK(same(&f, CY1));   // untouched: the same bytes
    CHECK(light_track_first_bad(&f) == 0);
    CHECK(light_track_count(&f, LT_TIME_FACTS, LT_TUNNEL) == 2);
    CHECK(light_track_count(&f, LT_TIME_A, LT_LOOK) == 2 && light_track_count(&f, LT_TIME_N, LT_LOOK) == 1 && light_track_count(&f, LT_TIME_B, LT_LOOK) == 0);
    CHECK(light_track_has_time(&f, LT_TIME_A) && light_track_has_time(&f, LT_TIME_N) && !light_track_has_time(&f, LT_TIME_B));
    LightTrackTunnel t;
    CHECK(light_track_get_tunnel(&f, 1, &t) && t.from == 1 && t.to == 62 && t.lit);
    CHECK(light_track_get_tunnel(&f, 0, &t) && t.from == 1064 && t.to == 1171 && !t.lit);
    CHECK(!light_track_get_tunnel(&f, 2, &t));
    int snow = 1;
    CHECK(light_track_get_snow(&f, &snow) && snow == 0);
    LightTrackSun sun;
    CHECK(light_track_get_sun(&f, LT_TIME_A, &sun) && sun.has_azimuth && sun.azimuth == 150 && sun.elevation == 20 && sun.shadow == 55);
    CHECK(!light_track_get_sun(&f, LT_TIME_N, &sun));
    LightTrackLook look;
    CHECK(light_track_get_look(&f, LT_TIME_A, 0, &look) && !look.has_nodes && look.has_light && look.light == 104 && look.has_colour == 1 && strcmp(look.colour, "ff5a0d") == 0);
    CHECK(light_track_get_look(&f, LT_TIME_A, 1, &look) && look.from == 1064 && look.to == 1171 && look.light == 90 && look.has_lamps && look.lamps == 220 && look.has_reach && look.reach == 2);
    CHECK(light_track_get_look(&f, LT_TIME_N, 0, &look) && !look.has_nodes && look.light == 115);
    LightTrackLamp lamp;
    CHECK(light_track_get_lamp(&f, LT_TIME_N, 0, &lamp) && lamp.node == 130 && lamp.percent == 150 && lamp.has_colour && strcmp(lamp.colour, "ff2000:80") == 0 && lamp.radius == 8);
    LightTrackBrightness b;
    CHECK(light_track_get_brightness(&f, LT_TIME_N, 0, &b) && b.from == 852 && b.to == 943 && b.percent == 60 && b.mask == LT_STRIP_LEFT);
    LightTrackTone tone;
    CHECK(light_track_get_tone(&f, LT_TIME_N, 0, &tone) && tone.mask == ((1u << 2) | (1u << 3) | (1u << 4)) && tone.percent == 50 && strcmp(tone.colour, "ff8000") == 0);
    LightTrackRows rows;
    CHECK(light_track_get_rows(&f, LT_TIME_N, 0, &rows) && rows.from == 5 && rows.to == 6 && rows.value[10] == 255);
    light_track_free(&f);
}

static void refuses_reading(void)
{
    LightTrack f;
    char why[LT_WHY];
    CHECK(!light_track_parse("tunnel 1 2\n", 11, NODES, &f, why, sizeof why));   // no track line first
    CHECK(!light_track_parse(CY1, strlen(CY1), 0, &f, why, sizeof why));
    // lines the game refuses are kept and listed, never rewritten
    const char *bad = "track X\ntunnel 1 5\nbogus 1\ntime B\nsnow 1\ntunnel_look 9 12 50\ntime B\nlamp 1 50\n";
    CHECK(parse(bad, NODES, &f));
    CHECK(light_track_first_bad(&f) == 3);
    CHECK(same(&f, bad));
    int bads = 0;
    for (int i = 0; i < f.count; i++) bads += f.lines[i].kind == LT_BAD;
    CHECK(bads == 4);   // the unknown word, a fact after time, a look outside the tunnels, a second time B
    CHECK(light_track_count(&f, LT_TIME_B, LT_LAMP) == 1);
    light_track_free(&f);
}

static void edits(void)
{
    LightTrack f;
    char why[LT_WHY];
    CHECK(parse(CY1, NODES, &f));
    // set a value: the line is rewritten in place and keeps its indent
    LightTrackLamp lamp;
    CHECK(light_track_get_lamp(&f, LT_TIME_N, 0, &lamp));
    lamp.percent = 90, lamp.has_radius = 0;
    CHECK(light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why));
    char *t = text_of(&f);
    CHECK(strstr(t, "\n  lamp 130 90 ff2000:80\n") != NULL);
    free(t);
    // a lamp's strip (the last word): none stays none, each form is read and written back as it was, a bad one is refused
    lamp.has_strip = 1, lamp.mask = LT_STRIP_RIGHT, lamp.has_radius = 1, lamp.radius = 8;
    CHECK(light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why));
    t = text_of(&f);
    CHECK(strstr(t, "\n  lamp 130 90 ff2000:80 8 right\n") != NULL);
    free(t);
    LightTrackLamp back;
    CHECK(light_track_get_lamp(&f, LT_TIME_N, 0, &back) && back.has_strip && back.mask == LT_STRIP_RIGHT && back.has_radius && back.radius == 8 && back.has_colour);
    lamp.mask = LT_ALL_POINTS;
    CHECK(light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why));
    t = text_of(&f);
    CHECK(strstr(t, "\n  lamp 130 90 ff2000:80 8 all\n") != NULL);
    free(t);
    CHECK(light_track_get_lamp(&f, LT_TIME_N, 0, &back) && back.has_strip && back.mask == LT_ALL_POINTS);
    lamp.mask = 0x1c;   // points 2..4
    CHECK(light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why));
    t = text_of(&f);
    CHECK(strstr(t, "\n  lamp 130 90 ff2000:80 8 p2 p4\n") != NULL);
    free(t);
    lamp.has_colour = 0, lamp.has_radius = 0, lamp.mask = 0x20;   // a strip with no colour and no radius: the word is the strip, not a radius
    CHECK(light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why));
    CHECK(light_track_get_lamp(&f, LT_TIME_N, 0, &back) && back.has_strip && back.mask == 0x20 && !back.has_colour && !back.has_radius);
    lamp.mask = 0x5;   // points 0 and 2: not a run
    CHECK(!light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why) && why[0] != '\0');
    lamp.has_strip = 0;   // none again: no word
    CHECK(light_track_set_lamp(&f, LT_TIME_N, 0, &lamp, why, sizeof why));
    CHECK(light_track_get_lamp(&f, LT_TIME_N, 0, &back) && !back.has_strip);
    light_track_free(&f);
    CHECK(parse(CY1, NODES, &f));
    // the file's own lamp lines with a strip are read, and a word that is no strip is a bad line the game would refuse
    {
        LightTrack g;
        const char *text = "track X\ntime N\nlamp 5 100 right\nlamp 6 100 ff0000 4 p3\nlamp 7 100 sideways\nlamp 8 100 left 4\n";
        CHECK(light_track_parse(text, strlen(text), 120, &g, why, sizeof why));
        CHECK(light_track_get_lamp(&g, LT_TIME_N, 0, &back) && back.has_strip && back.mask == LT_STRIP_RIGHT && !back.has_radius && !back.has_colour);
        CHECK(light_track_get_lamp(&g, LT_TIME_N, 1, &back) && back.has_strip && back.mask == 0x8 && back.has_colour && back.radius == 4);
        CHECK(light_track_first_bad(&g) == 5);   // line 5: `sideways`; line 6, the strip before the radius, is refused too
        light_track_free(&g);
    }
    // a tunnel fact keeps its comment when it changes
    LightTrackTunnel tun = { 1, 70, 1 };
    CHECK(light_track_set_tunnel(&f, 1, &tun, why, sizeof why));
    t = text_of(&f);
    CHECK(strstr(t, "tunnel 1 70 lit # the steelworks\n") != NULL);
    free(t);
    // add: a lamp goes to the end of its block, a tone in a time with no block makes the block at the end
    lamp = (LightTrackLamp){ .node = 300, .percent = 100 };
    CHECK(light_track_set_lamp(&f, LT_TIME_N, -1, &lamp, why, sizeof why));
    CHECK(light_track_count(&f, LT_TIME_N, LT_LAMP) == 2);
    LightTrackBrightness b = { 10, 20, 50, LT_STRIP_ROAD };
    CHECK(light_track_set_brightness(&f, LT_TIME_B, -1, &b, why, sizeof why));
    t = text_of(&f);
    CHECK(strstr(t, "\n\ntime B\n  brightness 10 20 50 road\n") != NULL);
    CHECK(strlen(t) > 0 && t[strlen(t) - 1] == '\n');
    free(t);
    LightTrackBrightness c = { 30, 40, 120, (1u << 3) | (1u << 4) | (1u << 5) };
    CHECK(light_track_set_brightness(&f, LT_TIME_B, -1, &c, why, sizeof why));
    CHECK(light_track_get_brightness(&f, LT_TIME_B, 1, &b) && b.from == 30 && b.mask == c.mask);
    CHECK(light_track_first_bad(&f) == 0);
    // the sun: a new one in a block, then set again (one line)
    LightTrackSun sun = { .has_azimuth = 0, .has_elevation = 1, .elevation = 12.5, .has_shadow = 1, .shadow = 60 };
    CHECK(light_track_set_sun(&f, LT_TIME_N, &sun, why, sizeof why));
    sun.shadow = 70;
    CHECK(light_track_set_sun(&f, LT_TIME_N, &sun, why, sizeof why));
    CHECK(light_track_count(&f, LT_TIME_N, LT_SUN) == 1);
    t = text_of(&f);
    CHECK(strstr(t, "  sun - 12.5 70\n") != NULL);
    free(t);
    // snow is set in place
    CHECK(light_track_set_snow(&f, 1, why, sizeof why));
    int snow = 0;
    CHECK(light_track_get_snow(&f, &snow) && snow == 1 && light_track_count(&f, LT_TIME_FACTS, LT_SNOW) == 1);
    // remove drops only that line
    CHECK(light_track_remove(&f, LT_TIME_N, LT_LAMP, 1, why, sizeof why));
    CHECK(light_track_count(&f, LT_TIME_N, LT_LAMP) == 1);
    CHECK(!light_track_remove(&f, LT_TIME_N, LT_LAMP, 5, why, sizeof why));
    light_track_free(&f);
}

static void refuses_edits(void)
{
    LightTrack f;
    char why[LT_WHY];
    CHECK(parse(CY1, NODES, &f));
    char *before = text_of(&f);
    // a look outside every tunnel
    LightTrackLook look = { .has_nodes = 1, .from = 200, .to = 210, .has_light = 1, .light = 50 };
    CHECK(!light_track_set_look(&f, LT_TIME_B, -1, &look, why, sizeof why));
    CHECK(strstr(why, "tunnel") != NULL);
    // a look inside the hall (lamps only) is fine; the default look's colour needs its light
    look = (LightTrackLook){ .has_nodes = 1, .from = 1, .to = 62, .has_lamps = 1, .lamps = 300 };
    CHECK(light_track_set_look(&f, LT_TIME_B, -1, &look, why, sizeof why));
    free(before);
    before = text_of(&f);
    look = (LightTrackLook){ .has_colour = 1, .colour = "ff0000" };
    CHECK(!light_track_set_look(&f, LT_TIME_B, -1, &look, why, sizeof why));
    // the tunnel a look needs cannot be dropped or shrunk away from it
    CHECK(!light_track_remove(&f, LT_TIME_FACTS, LT_TUNNEL, 0, why, sizeof why));
    CHECK(strstr(why, "line") != NULL);
    LightTrackTunnel tun = { 1100, 1171, 0 };
    CHECK(!light_track_set_tunnel(&f, 0, &tun, why, sizeof why));
    // ranges and limits
    LightTrackBrightness b = { 5, 4, 50, LT_ALL_POINTS };
    CHECK(!light_track_set_brightness(&f, LT_TIME_B, -1, &b, why, sizeof why));
    b = (LightTrackBrightness){ 5, NODES, 50, LT_ALL_POINTS };
    CHECK(!light_track_set_brightness(&f, LT_TIME_B, -1, &b, why, sizeof why));
    b = (LightTrackBrightness){ 5, 6, 1001, LT_ALL_POINTS };
    CHECK(!light_track_set_brightness(&f, LT_TIME_B, -1, &b, why, sizeof why));
    b = (LightTrackBrightness){ 5, 6, 50, (1u << 1) | (1u << 4) };   // not a run
    CHECK(!light_track_set_brightness(&f, LT_TIME_B, -1, &b, why, sizeof why));
    LightTrackLamp lamp = { .node = NODES, .percent = 50 };
    CHECK(!light_track_set_lamp(&f, LT_TIME_B, -1, &lamp, why, sizeof why));
    lamp = (LightTrackLamp){ .node = 5, .percent = 0 };
    CHECK(!light_track_set_lamp(&f, LT_TIME_B, -1, &lamp, why, sizeof why));
    LightTrackSun sun = { .has_elevation = 1, .elevation = 120 };
    CHECK(!light_track_set_sun(&f, LT_TIME_B, &sun, why, sizeof why));
    CHECK(!light_track_set_sun(&f, 7, &sun, why, sizeof why));
    // 65 brightness lines in a time; 17 different tones
    for (int i = 0; i < 64; i++) {
        b = (LightTrackBrightness){ (unsigned)i, (unsigned)i, 50, LT_ALL_POINTS };
        CHECK(light_track_set_brightness(&f, LT_TIME_C, -1, &b, why, sizeof why));
    }
    CHECK(!light_track_set_brightness(&f, LT_TIME_C, -1, &b, why, sizeof why));
    for (int i = 0; i < 16; i++) {
        LightTrackTone t = { 1, 2, LT_ALL_POINTS, (unsigned)i, "ff8000" };
        CHECK(light_track_set_tone(&f, LT_TIME_C, -1, &t, why, sizeof why));
    }
    LightTrackTone t = { 1, 2, LT_ALL_POINTS, 99, "ff8000" };
    CHECK(!light_track_set_tone(&f, LT_TIME_C, -1, &t, why, sizeof why));
    t.percent = 3;   // an existing tone again is fine
    CHECK(light_track_set_tone(&f, LT_TIME_C, -1, &t, why, sizeof why));
    CHECK(light_track_first_bad(&f) == 0);
    light_track_free(&f);
    CHECK(parse(before, NODES, &f));   // what the refused edits left alone
    CHECK(same(&f, before));
    light_track_free(&f);
    free(before);
}

static void shapes(void)
{
    // CRLF, no final newline, a file with no blocks: edits keep the file's habits
    LightTrack f;
    char why[LT_WHY];
    const char *crlf = "track X\r\ntunnel 1 5\r\ntime N\r\n  lamp 3 50\r\n";
    CHECK(parse(crlf, 100, &f));
    CHECK(same(&f, crlf));
    LightTrackLamp lamp = { .node = 9, .percent = 70 };
    CHECK(light_track_set_lamp(&f, LT_TIME_N, -1, &lamp, why, sizeof why));
    CHECK(same(&f, "track X\r\ntunnel 1 5\r\ntime N\r\n  lamp 3 50\r\n  lamp 9 70\r\n"));
    LightTrackTunnel t = { 20, 30, 0 };
    CHECK(light_track_set_tunnel(&f, -1, &t, why, sizeof why));
    CHECK(same(&f, "track X\r\ntunnel 1 5\r\ntunnel 20 30\r\ntime N\r\n  lamp 3 50\r\n  lamp 9 70\r\n"));
    light_track_free(&f);
    const char *bare = "track Y";
    CHECK(parse(bare, 100, &f));
    CHECK(same(&f, bare));
    lamp = (LightTrackLamp){ .node = 1, .percent = 10 };
    CHECK(light_track_set_lamp(&f, LT_TIME_B, -1, &lamp, why, sizeof why));
    CHECK(same(&f, "track Y\n\ntime B\n  lamp 1 10\n"));
    light_track_free(&f);
}

#ifndef _WIN32
static void folder(const char *dir)
{
    DIR *d = opendir(dir);
    CHECK(d != NULL);
    if (d == NULL) return;
    int files = 0;
    for (struct dirent *e; (e = readdir(d)) != NULL;) {
        const size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 4, ".txt") != 0) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        FILE *fp = fopen(path, "rb");
        CHECK(fp != NULL);
        if (fp == NULL) continue;
        fseek(fp, 0, SEEK_END);
        const long n = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *text = malloc((size_t)n + 1);
        CHECK(text != NULL && fread(text, 1, (size_t)n, fp) == (size_t)n);
        fclose(fp);
        text[n] = '\0';
        LightTrack f;
        char why[LT_WHY];
        const int ok = light_track_parse(text, (size_t)n, 6000, &f, why, sizeof why);
        if (!ok) printf("%s: %s\n", path, why);
        CHECK(ok);
        if (ok) {
            if (light_track_first_bad(&f) != 0) printf("%s: line %d is refused\n", path, light_track_first_bad(&f));
            CHECK(light_track_first_bad(&f) == 0);
            CHECK(same(&f, text));
            light_track_free(&f);
            files++;
        }
        free(text);
    }
    closedir(d);
    CHECK(files > 0);
}
#endif


// ---- the game's own limits and words (dev2's review, 2026-11-23): the editor saves no file the game refuses and keeps no word the game reads as another ---------------------------

static void append(char **text, const char *add)
{
    const size_t n = strlen(*text), m = strlen(add);
    char *t = realloc(*text, n + m + 1);
    CHECK(t != NULL);
    memcpy(t + n, add, m + 1);
    *text = t;
}

// D1: the halls and the `tunnel_look A B` lines of a time share the game's 64 zones
static void shared_zones(void)
{
    for (int looks = 31; looks <= 32; looks++) {
        char *t = calloc(1, 1), line[96];
        append(&t, "track CY1\n");
        for (int i = 0; i < 33; i++) snprintf(line, sizeof line, "tunnel %d %d lit\n", i, i), append(&t, line);
        append(&t, "time B\n");
        for (int i = 0; i < looks; i++) snprintf(line, sizeof line, "  tunnel_look %d %d 50\n", i, i), append(&t, line);
        LightTrack f;
        CHECK(parse(t, NODES, &f));
        CHECK((light_track_first_bad(&f) == 0) == (looks == 31));   // 33 + 31 = 64 fit, 33 + 32 = 65 do not: the game refuses the second
        if (looks == 31) {   // one more look is refused, with the game's reason, and the file is as it was
            LightTrackLook v;
            memset(&v, 0, sizeof v);
            v.has_nodes = 1, v.from = v.to = 32, v.has_light = 1, v.light = 50.0;
            char why[LT_WHY];
            CHECK(!light_track_set_look(&f, LT_TIME_B, -1, &v, why, sizeof why));
            CHECK(light_track_count(&f, LT_TIME_B, LT_LOOK) == 31 && light_track_first_bad(&f) == 0);
            CHECK(same(&f, t));
        }
        light_track_free(&f);
        free(t);
    }
}

// D2: the 16 tone slots hold the `tone` lines' tones and the zones' own coloured looks together
static void shared_tones(void)
{
    for (int coloured = 0; coloured <= 1; coloured++) {
        char *t = calloc(1, 1), line[96];
        append(&t, "track CY1\ntunnel 0 10\ntime B\n");
        for (int i = 0; i < 16; i++) snprintf(line, sizeof line, "  tone 20 30 %02x0000:50\n", i + 1), append(&t, line);
        if (coloured) append(&t, "  tunnel_look 0 5 50 ff8800\n");
        LightTrack f;
        CHECK(parse(t, NODES, &f));
        CHECK((light_track_first_bad(&f) == 0) == !coloured);   // the game refuses the 17th slot
        light_track_free(&f);
        free(t);
    }
}

// D3: `sun AZ EL SHADOW REACH`
static void sun_reach(void)
{
    LightTrack f;
    LightTrackSun sun;
    char why[LT_WHY];
    CHECK(parse("track CY1\ntime B\n  sun 150 50 55 800 # noon\n", NODES, &f));
    CHECK(light_track_first_bad(&f) == 0);
    CHECK(light_track_get_sun(&f, LT_TIME_B, &sun) && sun.has_reach && sun.reach == 800 && sun.shadow == 55);
    sun.azimuth = 160;
    CHECK(light_track_set_sun(&f, LT_TIME_B, &sun, why, sizeof why));
    CHECK(same(&f, "track CY1\ntime B\n  sun 160 50 55 800 # noon\n"));
    sun.reach = 3000;
    CHECK(!light_track_set_sun(&f, LT_TIME_B, &sun, why, sizeof why));   // the game's range is 1..2000
    light_track_free(&f);
    CHECK(parse("track CY1\ntime B\n  sun 150 50 55 -\n", NODES, &f) && light_track_first_bad(&f) == 0 && light_track_get_sun(&f, LT_TIME_B, &sun) && !sun.has_reach);
    light_track_free(&f);
    CHECK(parse("track CY1\ntime B\n  sun 150 50 55 0.5\n", NODES, &f) && light_track_first_bad(&f) == 3);
    light_track_free(&f);
}

// D5: the numbers the game reads with sscanf %u (a sign taken, a negative wrapped) in `brightness` and `rows`
static void scanf_numbers(void)
{
    const struct { const char *text; int good; } files[] = {
        { "track CY1\ntime B\n  brightness 0 -4294967295 50\n", 1 },   // wraps to 1
        { "track CY1\ntime B\n  rows 0 1 +1 2 3 4 5 6 7 8 9 10 11\n", 1 },
        { "track CY1\ntime B\n  brightness 0 +3 50\n", 1 },
        { "track CY1\ntime B\n  brightness 0 -1 50\n", 0 },   // wraps to 4294967295: outside the track
        { "track CY1\ntime B\n  rows 0 1 -1 2 3 4 5 6 7 8 9 10 11\n", 0 },   // 4294967295 is over 255
    };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        LightTrack f;
        CHECK(parse(files[i].text, NODES, &f));
        CHECK((light_track_first_bad(&f) == 0) == files[i].good);
        light_track_free(&f);
    }
}

// D7: a NUL byte: the file is not opened (it would not write back as it was read); D11: a file over the size limit is not opened
static void odd_files(void)
{
    static const char nul[] = "track CY1\ntime B\n  sun 0 0 0\0JUNK\n";
    LightTrack f;
    char why[LT_WHY];
    CHECK(!light_track_parse(nul, sizeof nul - 1, NODES, &f, why, sizeof why) && strstr(why, "NUL") != NULL);
    const size_t big = (size_t)LT_MAX_FILE_BYTES + 1;
    char *t = malloc(big);
    CHECK(t != NULL);
    memset(t, 'a', big);
    memcpy(t, "track CY1\n", 10);
    CHECK(!light_track_parse(t, big, NODES, &f, why, sizeof why) && strstr(why, "4 MB") != NULL);
    CHECK(light_track_parse(t, big - 1000000, NODES, &f, why, sizeof why));   // under the limit it reads (the junk is one long line the game refuses)
    light_track_free(&f);
    free(t);
}

// D9: a long comment on a line that is rewritten stays whole
static void long_comment(void)
{
    char comment[400], text[512];
    memset(comment, 'c', sizeof comment - 1);
    comment[sizeof comment - 1] = '\0';
    snprintf(text, sizeof text, "track CY1\ntime B\n  sun 1 2 3 # %s\n", comment);
    LightTrack f;
    LightTrackSun sun;
    char why[LT_WHY];
    CHECK(parse(text, NODES, &f));
    CHECK(light_track_get_sun(&f, LT_TIME_B, &sun));
    sun.azimuth = 9;
    CHECK(light_track_set_sun(&f, LT_TIME_B, &sun, why, sizeof why));
    char want[512];
    snprintf(want, sizeof want, "track CY1\ntime B\n  sun 9 2 3 # %s\n", comment);
    CHECK(same(&f, want));
    light_track_free(&f);
}

int main(int argc, char **argv)
{
    reads();
    refuses_reading();
    edits();
    refuses_edits();
    shapes();
    shared_zones();
    shared_tones();
    sun_reach();
    scanf_numbers();
    odd_files();
    long_comment();
#ifndef _WIN32
    if (argc > 1) folder(argv[1]);
#else
    (void)argc, (void)argv;
#endif
    return TEST_REPORT();
}
