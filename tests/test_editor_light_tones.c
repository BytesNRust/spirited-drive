/* The Track light window's tone rows in the file model (editor/light_file.h, dep98 step 2 part D): `tone`, `car_tone` and `tunnel` (zone) lines of a time's file, and the
 * `tunnel`/`canyon`/`car_dim` lines of a track's places file, read, set, added and removed by the game's rules, every other line left byte for byte. */
#include "light_file.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char NIGHT[] =
    "# tnfs track light 1\n"
    "track CY1 time N nodes 1024 flags 3fff\n"
    "rows 0 1023 255 255 255 255 255 255 255 255 255 255 255\n"
    "tunnel 1011 1023 lit\n"
    "tunnel 76 107 80 ffd070:50\n"
    "tone 120 180 ffe080:50\n"
    "tone 200 240 p6 p7 ff2020:60\n"
    "car_tone ff8020:60\n"
    "car_tone 120 180 20ff80:30\n"
    "brightness 10 20 70\n"
    "tone 300 310 12:40\n"            // a tone with no colour: not well-formed
    "car_tone 5 3 ff8020:60\n";       // from after to

static const char PLACES[] =
    "# tnfs track places 1\n"
    "track CY1\n"
    "present tunnel\n"
    "tunnel 1064 1171\n"
    "canyon 5 9\n"
    "snow 1\n";

static LightFile open_text(const char *text)
{
    LightFile f;
    char why[LIGHT_WHY];
    CHECK(light_file_parse(text, strlen(text), &f, why, sizeof why));
    return f;
}

static char *bytes(const LightFile *f)
{
    size_t n;
    char *s = light_file_format(f, &n);
    CHECK(s != NULL);
    return s;
}

static void test_read(void)
{
    LightFile f = open_text(NIGHT);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_TONE), 2);        // the malformed one is BAD, not a tone
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_CAR_TONE), 2);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_ZONE), 2);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_BAD), 2);
    CHECK_EQ_INT(light_file_first_bad(&f), 11);                     // `tone 300 310 12:40`
    LightTone t;
    CHECK(light_file_get_tone(&f, 0, &t) && t.from == 120 && t.to == 180 && t.mask == 0 && strcmp(t.colour, "ffe080") == 0 && t.percent == 50);
    CHECK(light_file_get_tone(&f, 1, &t) && t.from == 200 && t.to == 240 && t.mask == ((1u << 6) | (1u << 7)) && strcmp(t.colour, "ff2020") == 0 && t.percent == 60);
    CHECK(!light_file_get_tone(&f, 2, &t));
    LightCarTone c;
    CHECK(light_file_get_car_tone(&f, 0, &c) && !c.has_nodes && strcmp(c.colour, "ff8020") == 0 && c.percent == 60);
    CHECK(light_file_get_car_tone(&f, 1, &c) && c.has_nodes && c.from == 120 && c.to == 180 && strcmp(c.colour, "20ff80") == 0 && c.percent == 30);
    LightZone z;
    CHECK(light_file_get_zone(&f, 0, &z) && z.from == 1011 && z.to == 1023 && z.lit && !z.has_light && !z.has_colour);
    CHECK(light_file_get_zone(&f, 1, &z) && z.from == 76 && z.to == 107 && !z.lit && z.has_light && z.light == 80.0 && z.has_colour && strcmp(z.colour, "ffd070:50") == 0);
    light_file_free(&f);
}

static void test_edit(void)
{
    LightFile f = open_text(NIGHT);
    char why[LIGHT_WHY];
    char *before = bytes(&f);
    LightTone t = { 130, 150, 0, "ffe080", 70 };
    CHECK(light_file_set_tone(&f, 0, &t, why, sizeof why));
    char *after = bytes(&f);
    CHECK(strstr(after, "\ntone 130 150 ffe080:70\n") != NULL && strstr(after, "\ntone 120 180 ") == NULL);
    free(after);
    free(before);
    t = (LightTone){ 10, 20, (1u << 3) | (1u << 10), "00ff00", 5 };
    CHECK(light_file_set_tone(&f, -1, &t, why, sizeof why));
    char *s = bytes(&f);
    CHECK(strstr(s, "\ntone 10 20 p3 p10 00ff00:5") != NULL);
    CHECK(strcmp(s + strlen(s) - strlen("tone 10 20 p3 p10 00ff00:5\n"), "tone 10 20 p3 p10 00ff00:5\n") == 0);   // a new line goes last
    free(s);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_TONE), 3);
    CHECK(light_file_remove(&f, LIGHT_KIND_TONE, 2) && light_file_count(&f, LIGHT_KIND_TONE) == 2);
    LightCarTone c = { 0, 0, 0, "102030", 40 };
    CHECK(light_file_set_car_tone(&f, 0, &c, why, sizeof why));
    s = bytes(&f);
    CHECK(strstr(s, "\ncar_tone 102030:40\n") != NULL);
    free(s);
    c = (LightCarTone){ 1, 5, 9, "102030", 40 };
    CHECK(light_file_set_car_tone(&f, -1, &c, why, sizeof why));
    s = bytes(&f);
    CHECK(strstr(s, "car_tone 5 9 102030:40\n") != NULL);
    free(s);
    LightZone z = { 300, 320, 0, 1, 60.0, 1, "ffffff" };
    CHECK(light_file_set_zone(&f, -1, &z, why, sizeof why));
    s = bytes(&f);
    CHECK(strstr(s, "tunnel 300 320 60 ffffff\n") != NULL);
    free(s);
    z = (LightZone){ 400, 410, 1, 0, 0, 0, "" };
    CHECK(light_file_set_zone(&f, 0, &z, why, sizeof why));
    s = bytes(&f);
    CHECK(strstr(s, "\ntunnel 400 410 lit\n") != NULL && strstr(s, "tunnel 1011 1023 lit") == NULL);
    free(s);
    CHECK(light_file_remove(&f, LIGHT_KIND_ZONE, 0) && light_file_count(&f, LIGHT_KIND_ZONE) == 2);
    light_file_free(&f);
}

static void expect_refused(LightFile *f, int ok, const char *why, const char *word)
{
    CHECK(!ok);
    if (ok || strstr(why, word) == NULL) fprintf(stderr, "refusal was: %s (wanted '%s')\n", why, word);
    CHECK(!ok && strstr(why, word) != NULL);
    (void)f;
}

static void test_rules(void)
{
    LightFile f = open_text(NIGHT);
    char why[LIGHT_WHY];
    char *before = bytes(&f);
    LightTone t = { 50, 40, 0, "ffffff", 10 };
    expect_refused(&f, light_file_set_tone(&f, -1, &t, why, sizeof why), why, "later");
    t = (LightTone){ 0, 2000, 0, "ffffff", 10 };
    expect_refused(&f, light_file_set_tone(&f, -1, &t, why, sizeof why), why, "past");
    t = (LightTone){ 0, 10, 0, "ffffff", 101 };
    expect_refused(&f, light_file_set_tone(&f, -1, &t, why, sizeof why), why, "100");
    t = (LightTone){ 0, 10, 0, "zzzzzz", 10 };
    expect_refused(&f, light_file_set_tone(&f, -1, &t, why, sizeof why), why, "colour");
    t = (LightTone){ 0, 10, 1u << 11, "ffffff", 10 };
    expect_refused(&f, light_file_set_tone(&f, -1, &t, why, sizeof why), why, "points");
    LightCarTone c = { 1, 9, 5, "ffffff", 10 };
    expect_refused(&f, light_file_set_car_tone(&f, -1, &c, why, sizeof why), why, "later");
    c = (LightCarTone){ 0, 0, 0, "ffffff", 200 };
    expect_refused(&f, light_file_set_car_tone(&f, -1, &c, why, sizeof why), why, "100");
    LightZone z = { 10, 5, 0, 0, 0, 0, "" };
    expect_refused(&f, light_file_set_zone(&f, -1, &z, why, sizeof why), why, "later");
    z = (LightZone){ 10, 20, 1, 1, 50.0, 0, "" };   // a hall has no light of its own
    expect_refused(&f, light_file_set_zone(&f, -1, &z, why, sizeof why), why, "hall");
    z = (LightZone){ 10, 20, 0, 1, 20000.0, 0, "" };
    expect_refused(&f, light_file_set_zone(&f, -1, &z, why, sizeof why), why, "10000");
    expect_refused(&f, light_file_set_tone(&f, 9, &(LightTone){ 0, 1, 0, "ffffff", 1 }, why, sizeof why), why, "no such");
    char *after = bytes(&f);
    CHECK(strcmp(before, after) == 0);   // a refusal leaves the file as it was
    free(before);
    free(after);
    // the limits: 16 distinct tones, 64 lines of each kind
    for (int i = 0; i < 14; ++i) {   // 2 distinct tones already
        char colour[8];
        snprintf(colour, sizeof colour, "%02x0000", i + 1);
        LightTone n = { 0, 5, 0, "", 10 };
        memcpy(n.colour, colour, 7);
        CHECK(light_file_set_tone(&f, -1, &n, why, sizeof why));
    }
    LightTone n17 = { 0, 5, 0, "aabbcc", 10 };
    expect_refused(&f, light_file_set_tone(&f, -1, &n17, why, sizeof why), why, "16");
    LightTone same = { 6, 7, 0, "010000", 10 };   // a tone that is already one of the 16 is not a 17th
    CHECK(light_file_set_tone(&f, -1, &same, why, sizeof why));
    light_file_free(&f);
    f = open_text("track CY1 time B nodes 100\n");
    for (int i = 0; i < 64; ++i) CHECK(light_file_set_zone(&f, -1, &(LightZone){ 1, 2, 1, 0, 0, 0, "" }, why, sizeof why));
    expect_refused(&f, light_file_set_zone(&f, -1, &(LightZone){ 1, 2, 1, 0, 0, 0, "" }, why, sizeof why), why, "64");
    for (int i = 0; i < 64; ++i) CHECK(light_file_set_car_tone(&f, -1, &(LightCarTone){ 0, 0, 0, "ffffff", 5 }, why, sizeof why));
    expect_refused(&f, light_file_set_car_tone(&f, -1, &(LightCarTone){ 0, 0, 0, "ffffff", 5 }, why, sizeof why), why, "64");
    light_file_free(&f);
}

static void test_places(void)
{
    LightFile f = open_text(PLACES);
    CHECK_EQ_INT(f.letter, 0);
    CHECK_EQ_INT(light_file_count(&f, LIGHT_KIND_PLACE), 2);   // `present` and `snow` stay as they are
    LightPlace p;
    CHECK(light_file_get_place(&f, 0, &p) && strcmp(p.what, "tunnel") == 0 && p.from == 1064 && p.to == 1171);
    CHECK(light_file_get_place(&f, 1, &p) && strcmp(p.what, "canyon") == 0 && p.from == 5 && p.to == 9);
    char why[LIGHT_WHY];
    CHECK(light_file_set_place(&f, 1, &(LightPlace){ "car_dim", 5, 12 }, 1024, why, sizeof why));
    CHECK(light_file_set_place(&f, -1, &(LightPlace){ "tunnel", 20, 30 }, 1024, why, sizeof why));
    char *s = bytes(&f);
    CHECK(strstr(s, "\ncar_dim 5 12\n") != NULL && strstr(s, "canyon 5 9") == NULL && strstr(s, "tunnel 20 30\n") != NULL && strstr(s, "present tunnel\n") != NULL);
    free(s);
    expect_refused(&f, light_file_set_place(&f, -1, &(LightPlace){ "tunnel", 20, 2000 }, 1024, why, sizeof why), why, "past");
    expect_refused(&f, light_file_set_place(&f, -1, &(LightPlace){ "tunnel", 30, 20 }, 1024, why, sizeof why), why, "later");
    expect_refused(&f, light_file_set_place(&f, -1, &(LightPlace){ "hall", 1, 2 }, 1024, why, sizeof why), why, "tunnel");
    CHECK(light_file_remove(&f, LIGHT_KIND_PLACE, 0));
    s = bytes(&f);
    CHECK(strstr(s, "tunnel 1064 1171") == NULL);
    free(s);
    light_file_free(&f);
    // a time's file has no places lines, a places file has no tones
    f = open_text(NIGHT);
    expect_refused(&f, light_file_set_place(&f, -1, &(LightPlace){ "tunnel", 1, 2 }, 1024, why, sizeof why), why, "places file");
    light_file_free(&f);
    f = open_text(PLACES);
    expect_refused(&f, light_file_set_tone(&f, -1, &(LightTone){ 0, 1, 0, "ffffff", 1 }, why, sizeof why), why, "places file");
    light_file_free(&f);
}

static void test_untouched_bytes(void)
{
    LightFile f = open_text(NIGHT);
    char *s = bytes(&f);
    CHECK(strcmp(s, NIGHT) == 0);   // read only: the same bytes, the malformed lines included
    free(s);
    light_file_free(&f);
    f = open_text(PLACES);
    s = bytes(&f);
    CHECK(strcmp(s, PLACES) == 0);
    free(s);
    light_file_free(&f);
}

int main(void)
{
    test_read();
    test_edit();
    test_rules();
    test_places();
    test_untouched_bytes();
    return TEST_REPORT();
}
