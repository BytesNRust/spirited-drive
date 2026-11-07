/* The track light file (track_light_file.h): a made light written and read back is the same light, the header keeps a file to its track, time, nodes and flags, a bad line
 * is refused, and the mod's folder is looked in first. Made-up rows, no game data. */
#include "game/race/track_fence.h"
#include "game/race/psx_light_make.h"
#include "game/race/track_light_file.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 40 };

static PsxLight make(void)
{
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * PSX_LIGHT_POINTS);
    for (int n = 0; n < NODES; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) l.data[n * PSX_LIGHT_POINTS + p] = (uint8_t)(n < 10 ? 255 : n % 2 ? 60 + p : 90 - p);   /* a run, then an alternation */
    l.tunnel = calloc(NODES, 1);
    memset(l.tunnel + 12, 1, 8);
    l.hall = calloc(NODES, 1);                       /* a hall: a tunnel lit by its lamps */
    memset(l.hall + 25, 1, 3);
    l.car_dim = calloc(NODES, 1);
    memset(l.car_dim + 14, 1, 2);
    l.canyon = calloc(NODES, 1);
    l.canyon[3] = l.canyon[39] = 1;
    l.car_disc = 1;
    for (int i = 0; i < 3; i++) l.car_even[i] = 0.625f + (float)i / 7.0f, l.car_odd[i] = 0.1f * (float)(i + 1);
    l.look_set = 1, l.look_light = 104.4, l.look_cast[0] = 1.0f, l.look_cast[1] = 0.68f, l.look_cast[2] = 0.28f;   /* CY1's day: a made look the bytes cannot hold exactly */
    return l;
}

static void test_round_trip(void)
{
    PsxLight a = make(), b = { 0 };
    char *text = NULL, *places = NULL;
    size_t size = 0, places_size = 0;
    CHECK(track_light_format(&a, "CY1", 'N', 0x3fff, &text, &size));
    CHECK(track_light_place_format(&a, "CY1", &places, &places_size));
    int bad = -1;
    TrackLightFile f = { 0 };
    CHECK(track_light_parse(text, size, "CY1", 'N', NODES, 0x3fff, &f, &bad));
    CHECK(f.base);
    b = f.light;
    CHECK(track_light_place_parse(places, places_size, "CY1", NODES, &b, &bad));   /* the places are the track's own file */
    track_light_apply(&f.tweaks, &b, 0, 0);                                              /* the halls and the look come from this time's file */
    CHECK(b.look_set && b.look_light == 104.4 && memcmp(b.look_cast, a.look_cast, sizeof a.look_cast) == 0);   /* the look's floats exactly */
    CHECK(memcmp(a.data, b.data, (size_t)NODES * PSX_LIGHT_POINTS) == 0);
    CHECK(b.tunnel != NULL && memcmp(a.tunnel, b.tunnel, NODES) == 0);
    CHECK(b.hall != NULL && memcmp(a.hall, b.hall, NODES) == 0);   /* a hall is a place's tunnel the time's file calls lit */
    CHECK(b.car_dim != NULL && memcmp(a.car_dim, b.car_dim, NODES) == 0 && b.car_pool == NULL);   /* absent stays absent */
    CHECK(b.canyon != NULL && memcmp(a.canyon, b.canyon, NODES) == 0);
    CHECK(b.pool_class == NULL && b.car_pool == NULL);   /* the pools are the lamps' (`lamp` lines): a file has no pool runs */
    CHECK(b.car_disc == 1 && memcmp(a.car_even, b.car_even, sizeof a.car_even) == 0 && memcmp(a.car_odd, b.car_odd, sizeof a.car_odd) == 0);   /* the floats exactly */
    CHECK(b.nodes == NODES);
    /* the file is by runs: the first ten identical rows are one line; the places hold no light and the time's file no place */
    CHECK(strstr(text, "rows 0 9 ") != NULL);
    CHECK(strstr(places, "rows") == NULL && strstr(places, "tunnel 12 19") != NULL && strstr(places, "tunnel 25 27") != NULL && strstr(places, "canyon 3 3") != NULL && strstr(places, "car_dim 14 15") != NULL);
    CHECK(strstr(text, "canyon") == NULL && strstr(text, "car_dim") == NULL && strstr(text, "tunnel 12 19") == NULL && strstr(text, "tunnel 25 27 lit") != NULL && strstr(text, "tunnel_tone 104.4 1/0.68/0.28") != NULL);
    CHECK(strstr(text, "hall") == NULL && strstr(text, "cast") == NULL);
    psx_light_free(&a), track_light_file_free(&f);
    free(text), free(places);
}

static void test_header_and_bad_lines(void)
{
    PsxLight a = make();
    TrackLightFile f = { 0 };
    char *text = NULL;
    size_t size = 0;
    CHECK(track_light_format(&a, "CY1", 'N', 0x3fff, &text, &size));
    CHECK(!track_light_parse(text, size, "CY2", 'N', NODES, 0x3fff, &f, NULL));   /* another track */
    CHECK(!track_light_parse(text, size, "CY1", 'B', NODES, 0x3fff, &f, NULL));   /* another time */
    CHECK(!track_light_parse(text, size, "CY1", 'N', NODES + 1, 0x3fff, &f, NULL));   /* another node count */
    char *places = NULL;
    size_t places_size = 0;
    PsxLight none = { 0 };
    CHECK(track_light_place_format(&a, "CY1", &places, &places_size));
    CHECK(!track_light_place_parse(places, places_size, "CY2", NODES, &none, NULL));   /* another track's places */
    CHECK(!track_light_place_parse(places, places_size, "CY1", 20, &none, NULL));      /* a node past a shorter track */
    free(places);
    CHECK(track_light_parse(text, size, "CY1", 'N', NODES, 0x3ffe, &f, NULL) && !f.base && f.light.data == NULL);   /* other flags: no base, the file still read */
    track_light_file_free(&f);
    CHECK(track_light_parse(text, size, "CY1", 'N', NODES, TRACK_LIGHT_ANY_FLAGS, &f, NULL) && f.base && f.light.data != NULL && f.flags != 0x3ffe);   /* the file rules: its flags are a record */
    track_light_file_free(&f);
    char *bad_text = malloc(size + 32);
    memcpy(bad_text, text, size);
    memcpy(bad_text + size, "tunel 5 99\n", 12);   /* past the track */
    int bad = 0;
    CHECK(!track_light_parse(bad_text, size + 11, "CY1", 'N', NODES, 0x3fff, &f, &bad));
    CHECK(bad > 2);
    char *missing = strdup("pista CY1 hora N nodos 40 banderas 3fff\nfilas 0 9 1 1 1 1 1 1 1 1 1 1 1\n");
    CHECK(!track_light_parse(missing, strlen(missing), "CY1", 'N', NODES, 0x3fff, &f, NULL));   /* nodes without a row */
    free(missing), free(bad_text), free(text);
    psx_light_free(&a);
}

static void test_tweaks_stay_with_other_flags(void)
{
    /* a file made with other flags: its rows are not used, but its retouches are read and apply over a light made some other way */
    const char *text = "pista CY1 hora N nodos 40 banderas 1\nfilas 0 39 200 200 200 200 200 200 200 200 200 200 200\nbrillo 10 19 50\nbrillo 30 30 300\ntono_tunel 130 ff8040\ntono_coche ff8020:60\ntono_coche 5 7 000000:0\n";
    TrackLightFile f = { 0 };
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 0x2, &f, NULL));
    CHECK(!f.base);
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * PSX_LIGHT_POINTS);
    memset(l.data, 100, (size_t)NODES * PSX_LIGHT_POINTS);
    track_light_apply(&f.tweaks, &l, 0, 0);
    CHECK(l.data[9 * PSX_LIGHT_POINTS] == 100 && l.data[10 * PSX_LIGHT_POINTS] == 50 && l.data[19 * PSX_LIGHT_POINTS + 10] == 50 && l.data[20 * PSX_LIGHT_POINTS] == 100);
    CHECK(l.data[30 * PSX_LIGHT_POINTS] == 255);   /* clamped */
    CHECK(l.car_tone != NULL && l.car_tone[4 * 4] == 0xff && l.car_tone[4 * 4 + 1] == 0x80 && l.car_tone[4 * 4 + 3] == 60 && l.car_tone[4 * 6 + 3] == 0 && l.car_tone[4 * 8 + 3] == 60);   /* the car's tone: whole track, then 5..7 off */
    CHECK(l.look_set && l.look_light == 130.0 && l.look_cast[0] == 1.0f && l.look_cast[1] == (float)(128.0 / 255.0) && l.look_cast[2] == (float)(64.0 / 255.0));   /* the time's look */
    char names[128];
    track_light_diff(0x1, 0x2, names, sizeof names);
    CHECK(strstr(names, "trans_shadows=off") != NULL && strstr(names, "made_tunnels=on") != NULL);
    track_light_diff(0x2, 0x2, names, sizeof names);
    CHECK(names[0] == '\0');
    psx_light_free(&l);
    track_light_file_free(&f);
}

static PsxLight applied(const char *text, double *light)
{
    TrackLightFile f = { 0 };
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 0x2, &f, NULL));
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * PSX_LIGHT_POINTS);
    memset(l.data, 100, (size_t)NODES * PSX_LIGHT_POINTS);
    track_light_apply(&f.tweaks, &l, 0, 0);
    *light = f.tweaks.look_light;
    track_light_file_free(&f);
    return l;
}

static void test_english_and_spanish_words_load_the_same(void)
{
    const char *english = "track CY1 time N nodes 40 flags 1\nrows 0 39 200 200 200 200 200 200 200 200 200 200 200\nbrightness 10 19 50\ntunnel_tone 130 ff8040\ncar_tone ff8020:60\ntunnel 5 9\n";
    const char *spanish = "pista CY1 hora N nodos 40 banderas 1\nfilas 0 39 200 200 200 200 200 200 200 200 200 200 200\nbrillo 10 19 50\ntono_tunel 130 ff8040\ntono_coche ff8020:60\ntunel 5 9\n";
    double we = 0, ws = 0;
    PsxLight e = applied(english, &we), s = applied(spanish, &ws);
    CHECK(we == 130.0 && ws == 130.0);
    CHECK(memcmp(e.data, s.data, (size_t)NODES * PSX_LIGHT_POINTS) == 0 && e.data[10 * PSX_LIGHT_POINTS] == 50);
    CHECK(e.car_tone != NULL && s.car_tone != NULL && memcmp(e.car_tone, s.car_tone, (size_t)NODES * 4) == 0);
    psx_light_free(&e), psx_light_free(&s);

    /* the writer says English only */
    PsxLight a = make();
    char *text = NULL;
    size_t size = 0;
    CHECK(track_light_format(&a, "CY1", 'N', 0x3fff, &text, &size));
    CHECK(strstr(text, "track CY1 time N nodes ") != NULL && strstr(text, "flags ") != NULL);
    CHECK(strstr(text, "pista") == NULL && strstr(text, "hora ") == NULL && strstr(text, "filas") == NULL && strstr(text, "presente") == NULL);
    free(text);
    psx_light_free(&a);
}

/* the scene tones: a stretch's, a side strip's, the 16 distinct a track has at most, and the class a polygon takes from its four corners */
static void test_tones(void)
{
    const char *text = "track CY1 time N nodes 40 flags 1\nrows 0 39 200 200 200 200 200 200 200 200 200 200 200\ntone 10 19 ffe080:50\ntone 15 25 p6 p7 ff2020:60\ntone 30 31 ffe080:50\n";
    TrackLightFile f = { 0 };
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 0x2, &f, NULL));
    CHECK(f.tweaks.tone_count == 2 && f.tweaks.tone_run_count == 3);   /* ffe080:50 twice is one tone */
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = calloc((size_t)NODES * PSX_LIGHT_POINTS, 1);
    track_light_apply(&f.tweaks, &l, 0, 0);
    CHECK(l.tone_count == 2 && l.tone[0].rgb[0] == 0xff && l.tone[0].rgb[1] == 0xe0 && l.tone[0].percent == 50 && l.tone[1].rgb[1] == 0x20);
    CHECK(l.tone_id[9] == 0 && l.tone_id[10] == 1 && l.tone_mask[10] == 0x7ff);          /* a stretch: every point */
    CHECK(l.tone_id[17] == 2 && l.tone_mask[17] == ((1u << 6) | (1u << 7)));             /* the later line over the same nodes: its strip only */
    CHECK(l.tone_id[30] == 1 && l.tone_id[32] == 0);
    CHECK(psx_light_vertex_class(&l, 12, 3) == (1 << PSX_TONE_BASE_BITS) && psx_light_vertex_class(&l, 9, 3) == 0);
    CHECK(psx_light_vertex_class(&l, 17, 6) == (2 << PSX_TONE_BASE_BITS) && psx_light_vertex_class(&l, 17, 5) == 0);   /* the strip's points p6, p7 only */
    /* a polygon takes a tone when its four corners have the same one; the class of the road is kept under it */
    uint8_t cls[8] = { 0 }, now[6] = { 0 }, light_row[8] = { 0 };
    TrackFenceInputs in = { .tunnel = cls, .light = light_row, .light_now = now };
    const uint8_t corner[4] = { 0, 1, 2, 3 };
    cls[0] = cls[1] = cls[2] = cls[3] = (uint8_t)(1 << PSX_TONE_BASE_BITS);
    track_light_set(&in, 0, corner);
    CHECK(now[5] == (1 << PSX_TONE_BASE_BITS));
    cls[3] = 0;
    track_light_set(&in, 0, corner);
    CHECK(now[5] == 0);   /* one corner out of the tone: the polygon is the road's */
    cls[0] = cls[1] = cls[2] = cls[3] = (uint8_t)(1 | 3 << PSX_TONE_BASE_BITS);   /* a tunnel's polygon with a tone */
    track_light_set(&in, 0, corner);
    CHECK(now[5] == (1 | 3 << PSX_TONE_BASE_BITS));
    psx_light_free(&l);
    track_light_file_free(&f);
    /* bad lines: a point past p10, a 17th distinct tone, nodes past the track */
    TrackLightFile g = { 0 };
    int bad = 0;
    { const char *t = "track CY1 time N nodes 40 flags 1\nrows 0 39 1 1 1 1 1 1 1 1 1 1 1\ntone 1 2 p11 ff0000:10\n"; CHECK(!track_light_parse(t, strlen(t), "CY1", 'N', NODES, 0x2, &g, &bad) && bad == 3); }
    char many[1024] = "track CY1 time N nodes 40 flags 1\nrows 0 39 1 1 1 1 1 1 1 1 1 1 1\n";
    for (int i = 0; i < 17; i++) snprintf(many + strlen(many), sizeof many - strlen(many), "tone %d %d %02x0000:%d\n", i, i, i + 1, 10 + i);
    CHECK(!track_light_parse(many, strlen(many), "CY1", 'N', NODES, 0x2, &g, &bad) && bad == 2 + 17);
    { const char *t = "track CY1 time N nodes 40 flags 1\nrows 0 39 1 1 1 1 1 1 1 1 1 1 1\ntone 38 40 ff0000:10\n"; CHECK(!track_light_parse(t, strlen(t), "CY1", 'N', NODES, 0x2, &g, &bad) && bad == 3); }
}

/* the places file and the time's zones make the closed zones for the weather: a tunnel and a hall (a tunnel with a lot of light) are closed, a canyon is open air (AL1 600..771 rains) */
static void test_weather_closed_zones(void)
{
    const char *time_text = "track AL1 time B nodes 800 flags 1\nrows 0 799 255 255 255 255 255 255 255 255 255 255 255\ntunnel 300 330 lit\n";
    const char *place_text = "track AL1\npresent tunnel\ncanyon 600 771\ntunnel 100 140\ntunnel 300 330\ntunnel 650 660\n";
    TrackLightFile f;
    CHECK(track_light_parse(time_text, strlen(time_text), "AL1", 'B', 800, 1, &f, NULL));
    CHECK(track_light_place_parse(place_text, strlen(place_text), "AL1", 800, &f.light, NULL));
    track_light_apply(&f.tweaks, &f.light, 0, 0);
    const PsxLight *l = &f.light;
    CHECK_EQ_INT(psx_light_weather_closed(l, 602), 0);   /* the canyon: open air, as the hall bit says what it says */
    CHECK_EQ_INT(psx_light_weather_closed(l, 600), 0);
    CHECK_EQ_INT(psx_light_weather_closed(l, 771), 0);
    CHECK_EQ_INT(psx_light_weather_closed(l, 597), 0);
    CHECK_EQ_INT(psx_light_weather_closed(l, 120), 1);   /* a tunnel: closed */
    CHECK_EQ_INT(psx_light_weather_closed(l, 100), 1);
    CHECK_EQ_INT(psx_light_weather_closed(l, 141), 0);
    CHECK_EQ_INT(psx_light_weather_closed(l, 310), 1);   /* a hall (TR7's steelworks) is a tunnel with a lot of light: it has a roof */
    CHECK(l->hall != NULL && l->hall[310] && l->tunnel[310] == 0 && l->tunnel[120] == 1);
    CHECK_EQ_INT(psx_light_weather_closed(l, 655), 0);   /* a tunnel line under a canyon: the canyon wins */
    CHECK_EQ_INT(psx_light_weather_closed(NULL, 5), 0);
    track_light_file_free(&f);
}

/* the new lines: tunnel [light] [colour | lit], tunnel_tone, the colours' three spellings, one look a time, and what is no longer read */
static void test_the_tunnel_lines(void)
{
    const char *head = "track CY1 time N nodes 40 flags 1\nrows 0 39 200 200 200 200 200 200 200 200 200 200 200\n";
    char text[512];
    TrackLightFile f;
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = calloc((size_t)NODES * PSX_LIGHT_POINTS, 1);
    snprintf(text, sizeof text, "%stunnel_tone 115 ffe080\n", head);   /* the track's look this time: a light (100 is full) and a colour */
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 1, &f, NULL));
    track_light_apply(&f.tweaks, &l, 0, 0);
    CHECK(l.look_set && l.look_light == 115.0 && l.look_cast[0] == 1.0f && l.look_cast[1] == (float)(224.0 / 255.0) && l.look_cast[2] == (float)(128.0 / 255.0));
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%stunnel 5 9 60\ntunnel 15 19 60 ff0000:50\n", head);   /* a zone with its own light and colour; the colour at half strength is half way to no change */
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 1, &f, NULL));
    CHECK(f.tweaks.zone_count == 2 && f.tweaks.look_light == 60.0 && f.tweaks.look_cast[0] == 1.0f && f.tweaks.look_cast[1] == 0.5f);
    track_light_file_free(&f);
    snprintf(text, sizeof text, "%stunnel 5 9\n", head);   /* no look at all: the default, dark (10 %) and without a colour */
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 1, &f, NULL));
    PsxLight d = { 0 };
    d.nodes = NODES, d.data = calloc((size_t)NODES * PSX_LIGHT_POINTS, 1);
    track_light_apply(&f.tweaks, &d, 0, 0);
    CHECK(d.look_set && d.look_light == 10.0 && d.look_cast[0] == 1.0f && d.look_cast[1] == 1.0f && d.look_cast[2] == 1.0f && d.tunnel[7] == 1 && d.tunnel[4] == 0);
    track_light_file_free(&f);
    psx_light_free(&d);
    int bad = 0;
    const char *refused[] = { "tunnel 5 9 60\ntunnel 15 19 70\n",            /* one look a time: a second zone with another light */
                              "tunnel 5 9 60 ff0000\ntunnel 15 19 60 00ff00\n",   /* ... or another colour */
                              "tunnel_tone 60\ntunnel 5 9 70\n",
                              "tunnel_tone\n", "tunnel_tone day 1 0.5 0.25 130\n", "tunnel_tone night 1 0.5 0.25 130\n",   /* no light, or the old form */
                              "hall 5 9\n", "cast neutral 1 city 0\n", "canyon 1 2\n", "car_dim 1 2\n", "present tunnel\n",   /* no longer read here: places are the track's file */
                              "tunnel 5 9 lit 60\n", "tunnel 5 9 60 lit\n", "tunnel_tone 60 lit\n", "tunnel 5 9 ffffff ffffff\n", "tunnel 5 9 60 ff00\n", "tunnel 5 9 60 ff0000:101\n", "tunnel 5 9 60 2/0/0\n" };
    for (size_t i = 0; i < sizeof refused / sizeof *refused; i++) {
        snprintf(text, sizeof text, "%s%s", head, refused[i]);
        CHECK(!track_light_parse(text, strlen(text), "CY1", 'N', NODES, 1, &f, &bad) && bad == (i < 3 ? 4 : 3));   /* the line that disagrees */
    }
    snprintf(text, sizeof text, "%stunnel 5 9 60 1/0.5/0.25\ntunnel 11 12 60 ff8040\n", head);   /* the written-out factors, as the export writes the made looks */
    CHECK(!track_light_parse(text, strlen(text), "CY1", 'N', NODES, 1, &f, &bad));   /* ff8040 is 1, 0.50196, 0.251: not the same look as 1/0.5/0.25 */
    snprintf(text, sizeof text, "%stunnel 5 9 60 1/0.5/0.25\ntunnel 11 12 60 1/0.5/0.25\n", head);
    CHECK(track_light_parse(text, strlen(text), "CY1", 'N', NODES, 1, &f, &bad) && f.tweaks.look_cast[1] == 0.5f && f.tweaks.look_cast[2] == 0.25f);
    track_light_file_free(&f);
    psx_light_free(&l);
}

/* the made look and the file's give the same tables' inputs: the file's light is a percent of full light, so the gain is it over the time's own tint */
static void test_a_made_look_and_its_file_agree(void)
{
    PsxLight made = { 0 };
    made.nodes = NODES;
    made.tunnel = calloc(NODES, 1);
    made.city_tunnel = 1;
    made.warm_percent[0] = 115, made.warm_percent[1] = 174;
    const float day_cast[3] = { 1.0f, 0.68f, 0.28f }, night_cast[3] = { 1.0f, 0.35f, 0.05f };
    memcpy(made.warm_cast[0], day_cast, sizeof day_cast), memcpy(made.warm_cast[1], night_cast, sizeof night_cast);
    const float tints[3][3] = { { 1, 1, 1 }, { 0.8f, 0.7f, 0.9f }, { 0.35f, 0.4f, 0.5f } };   /* the day, a dusk, a night */
    for (int t = 0; t < 3; t++) {
        PsxTunnelLook was;
        psx_light_tunnel_look(&made, tints[t], tints[t], 0, &was);
        PsxLight filed = made;   /* the file's: what the export derives from the made look */
        const float mean = (tints[t][0] + tints[t][1] + tints[t][2]) / 3.0f;
        filed.look_set = 1, filed.look_light = (double)was.tint[0] * (double)mean * 100.0;
        memcpy(filed.look_cast, was.cast, sizeof was.cast);
        char *text = NULL;
        size_t size = 0;
        CHECK(track_light_format(&(PsxLight){ .nodes = NODES, .data = made.data == NULL ? calloc((size_t)NODES * PSX_LIGHT_POINTS, 1) : made.data, .look_set = 1, .look_light = filed.look_light, .look_cast = { was.cast[0], was.cast[1], was.cast[2] } }, "CY1", 'B', 1, &text, &size));
        TrackLightFile f;
        CHECK(track_light_parse(text, size, "CY1", 'B', NODES, 1, &f, NULL));
        PsxLight loaded = { 0 };
        loaded.nodes = NODES, loaded.tunnel = made.tunnel;
        track_light_apply(&f.tweaks, &loaded, 0, 0);
        PsxTunnelLook now;
        psx_light_tunnel_look(&loaded, tints[t], tints[t], 0, &now);
        CHECK(now.cast_on == was.cast_on && memcmp(now.cast, was.cast, sizeof now.cast) == 0 && memcmp(now.tint, was.tint, sizeof now.tint) == 0 && now.min_light == was.min_light);   /* bit for bit */
        loaded.tunnel = NULL;
        psx_light_free(&loaded);
        track_light_file_free(&f);
        free(text);
    }
    free(made.tunnel);

#define LAMP_HEAD "track CY1 time N nodes 40 flags 1\nrows 0 39 100 100 100 100 100 100 100 100 100 100 100\n"

static int parse_lamp_lines(const char *lines, TrackLightFile *f, int *bad)
{
    char text[2048];
    snprintf(text, sizeof text, "%s%s", LAMP_HEAD, lines);
    return track_light_parse(text, strlen(text), "CY1", 'N', NODES, 0x2, f, bad);
}

/* `lamp <node> <light%> [RRGGBB[:strength]] [radius]` (and `farola`): the colour is optional (no colour is the warm street lamp), the radius is optional (PSX_LIGHT_LAMP_RADIUS) */
static void test_lamp_lines_parse(void)
{
    TrackLightFile f = { 0 };
    int bad = 0;
    CHECK(parse_lamp_lines("lamp 10 80\nlamp 11 100 ff2000\nlamp 12 150 8\nfarola 13 30 00ff80:50 12\nlamp 14 100 ff2000 3\nlamp 17 100 ff2000:100\nlamp 18 100 ff2000:0\n", &f, &bad));
    const TrackLightTweaks *t = &f.tweaks;
    CHECK(t->lamp_count == 7);
    CHECK(t->lamps[0].node == 10 && t->lamps[0].percent == 80 && !t->lamps[0].coloured && t->lamps[0].radius == PSX_LIGHT_LAMP_RADIUS);   /* no colour: warm; no radius: the default */
    CHECK(t->lamps[1].coloured && t->lamps[1].strength == 100 && t->lamps[1].rgb[0] == 0xff && t->lamps[1].rgb[1] == 0x20 && t->lamps[1].rgb[2] == 0 && t->lamps[1].radius == PSX_LIGHT_LAMP_RADIUS);
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
    CHECK_EQ_INT(psx_light_lamp_falloff(0.0f, 30.0f) == 1.0f, 1);
    CHECK_EQ_INT(psx_light_lamp_falloff(15.0f, 30.0f) == 0.25f, 1);
    CHECK_EQ_INT(psx_light_lamp_falloff(30.0f, 30.0f) == 0.0f && psx_light_lamp_falloff(31.0f, 30.0f) == 0.0f, 1);
    CHECK_EQ_INT(psx_light_lamp_falloff(5.0f, 0.0f) == 0.0f, 1);
    float last = 2.0f;
    for (int d = 0; d <= 30; d++) {   /* never rises, steps are small */
        const float f = psx_light_lamp_falloff((float)d, 30.0f);
        CHECK(f <= last && last - f < 0.1f + (d == 0 ? 2.0f : 0.0f));
        last = f;
    }
}

static int near_f(float a, float b) { return fabsf(a - b) < 1e-4f; }

static PsxLight lamp_light(const char *lines, TrackLightFile *f, int lamps, int ring)
{
    CHECK(parse_lamp_lines(lines, f, NULL));
    PsxLight l = { 0 };
    l.nodes = NODES;
    l.data = malloc((size_t)NODES * PSX_LIGHT_POINTS);
    memset(l.data, 100, (size_t)NODES * PSX_LIGHT_POINTS);
    l.pool_class = calloc(NODES, 1), l.car_pool = calloc(NODES, 1);   /* what the code made: a lamp line replaces it */
    track_light_apply(&f->tweaks, &l, lamps, ring);
    return l;
}

/* a lamp line is the light's lamp list (by node, the colour its strength): warm with no colour, the line's colour mixed toward warm by its strength; the old pools go; with the flag off or no lamp
 * line nothing changes */
static void test_lamp_apply(void)
{
    TrackLightFile f = { 0 };
    PsxLight on = lamp_light("lamp 30 55 ff2000 3\nlamp 20 100\nlamp 25 200 ff0000:0\n", &f, 1, 0);
    CHECK(on.lamps != NULL && on.lamp_n == 3 && on.lamp_reach_nodes == PSX_LIGHT_LAMP_RADIUS && !on.lamp_placed);
    CHECK(on.lamps[0].node == 20 && on.lamps[1].node == 25 && on.lamps[2].node == 30);   /* by node */
    CHECK(near_f(on.lamps[0].rgb[0], 1.0f) && near_f(on.lamps[0].rgb[1], 0.9f) && near_f(on.lamps[0].rgb[2], 0.55f));   /* the warm lamp at 100 percent */
    CHECK(near_f(on.lamps[1].rgb[0], 2.0f) && near_f(on.lamps[1].rgb[1], 1.8f));   /* strength 0: the warm colour again, at 200 percent */
    CHECK(on.lamps[2].rgb[0] > 0.54f && on.lamps[2].rgb[0] < 0.56f && on.lamps[2].rgb[1] < 0.2f && near_f(on.lamps[2].rgb[2], 0.0f));   /* a colour at 55 percent: red, little green */
    CHECK(on.lamps[0].reach == (float)PSX_LIGHT_LAMP_RADIUS && on.lamps[2].reach == 3.0f);   /* nodes until it is placed */
    CHECK(on.pool_class == NULL && on.car_pool == NULL);   /* the old pools are gone */
    CHECK(memcmp(on.data + 20 * PSX_LIGHT_POINTS, on.data + 5 * PSX_LIGHT_POINTS, PSX_LIGHT_POINTS) == 0);   /* the rows are not touched at load */
    psx_light_free(&on);
    track_light_file_free(&f);
    /* the port flag off: the lines are read and laid nowhere; the pools stay */
    TrackLightFile off = { 0 };
    PsxLight none = lamp_light("lamp 20 100 ff0000\n", &off, 0, 0);
    CHECK(none.lamps == NULL && none.lamp_n == 0 && none.pool_class != NULL && none.car_pool != NULL);
    psx_light_free(&none);
    track_light_file_free(&off);
    /* a file with no lamp line, the flag on: the light is the same as with it off */
    TrackLightFile plain = { 0 };
    PsxLight a = lamp_light("brightness 5 6 50\n", &plain, 1, 0), b = lamp_light("brightness 5 6 50\n", &plain, 0, 0);
    CHECK(memcmp(a.data, b.data, (size_t)NODES * PSX_LIGHT_POINTS) == 0 && a.lamps == NULL && a.pool_class != NULL);
    psx_light_free(&a), psx_light_free(&b);
    track_light_file_free(&plain);
}

/* the export writes the street lamps as `lamp <node> 100` lines (there are no pool runs), and the file reads back as the lamp list */
static void test_export_writes_lamps(void)
{
    PsxLight a = make();
    const uint32_t lamps[] = { 22, 30, 3, 22, 99 };   /* a node twice and one past the track (a stage drops it) */
    a.lamp_node = malloc(sizeof lamps), a.lamp_node_count = 5, a.lamp_ring = 0;
    memcpy(a.lamp_node, lamps, sizeof lamps);
    char *text = NULL;
    size_t size = 0;
    CHECK(track_light_format(&a, "CY1", 'N', 0x3fff, &text, &size));
    CHECK(strstr(text, "\nlamp 3 100\nlamp 22 100\nlamp 30 100\n") != NULL && strstr(text, "pool") == NULL);   /* by node, once each */
    TrackLightFile f = { 0 };
    CHECK(track_light_parse(text, size, "CY1", 'N', NODES, 0x3fff, &f, NULL));
    PsxLight b = f.light;
    memset(&f.light, 0, sizeof f.light);
    track_light_apply(&f.tweaks, &b, 1, 0);
    CHECK(b.lamp_n == 3 && b.lamps[0].node == 3 && b.lamps[1].node == 22 && b.lamps[2].node == 30 && near_f(b.lamps[1].rgb[0], 1.0f));
    CHECK(memcmp(b.data, a.data, (size_t)NODES * PSX_LIGHT_POINTS) == 0);   /* the rows are the file's: a lamp line lifts nothing at load */
    free(text);
    psx_light_free(&b);
    track_light_file_free(&f);
    psx_light_free(&a);
    PsxLight none = make();   /* no lamps: no lines */
    CHECK(track_light_format(&none, "CY1", 'N', 0x3fff, &text, &size) && strstr(text, "lamp") == NULL);
    free(text);
    psx_light_free(&none);
}

int main(void)
{
    test_weather_closed_zones();
    test_the_tunnel_lines();
    test_a_made_look_and_its_file_agree();
    test_tones();
    test_round_trip();
    test_header_and_bad_lines();
    test_tweaks_stay_with_other_flags();
    test_english_and_spanish_words_load_the_same();
    test_lamp_lines_parse();
    test_lamp_falloff();
    test_lamp_apply();
    test_export_writes_lamps();
    return TEST_REPORT();
}
